#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include <inttypes.h>

#define TX_GPIO_NUM 18
#define RX_GPIO_NUM 19

#define WIFI_SSID "ESP32_AP"
#define WIFI_PASS ""

#define MAX_STORED_MESSAGES 20

static const char *TAG = "TWAI_APP";

// Queue to store filtered 0x762 messages
QueueHandle_t message_queue;

// Structure to store message and status
typedef struct {
    twai_message_t message;
    const char* status;
} message_with_status_t;

// Array to store the last N messages
message_with_status_t stored_messages[MAX_STORED_MESSAGES];
int stored_message_count = 0;

// Define the TWAI messages to be sent
twai_message_t messages_to_send[] = {
    {.identifier = 0x742, .data_length_code = 8, .data = {0x02, 0x10, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.identifier = 0x742, .data_length_code = 8, .data = {0x02, 0x21, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.identifier = 0x742, .data_length_code = 8, .data = {0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.identifier = 0x742, .data_length_code = 8, .data = {0x02, 0x21, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.identifier = 0x742, .data_length_code = 8, .data = {0x02, 0x21, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.identifier = 0x742, .data_length_code = 8, .data = {0x02, 0x21, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.identifier = 0x742, .data_length_code = 8, .data = {0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}
};

// Function to send TWAI messages
void send_twai_messages() {
    for (int i = 0; i < sizeof(messages_to_send) / sizeof(messages_to_send[0]); i++) {
        ESP_ERROR_CHECK(twai_transmit(&messages_to_send[i], pdMS_TO_TICKS(1000)));
        ESP_LOGI(TAG, "Message sent: ID=0x%03" PRIx32 ", DLC=%d", messages_to_send[i].identifier, messages_to_send[i].data_length_code);
        vTaskDelay(pdMS_TO_TICKS(100)); // Small delay between messages
    }
}

void twai_receive_task(void *pvParameters) {
    twai_message_t rx_message;
    message_with_status_t message_with_status;
    while (1) {
        if (twai_receive(&rx_message, pdMS_TO_TICKS(1000)) == ESP_OK) {
            if (rx_message.identifier == 0x762 && rx_message.data[0] == 0x23 && rx_message.data[1] == 0x00) {
                message_with_status.message = rx_message;
                message_with_status.status = (rx_message.data[2] == 0x88) ? "Status 4" : "Status 3";
                
                ESP_LOGI(TAG, "Filtered message: ID=0x%03" PRIx32 ", DLC=%d, Data=", rx_message.identifier, rx_message.data_length_code);
                for (int i = 0; i < rx_message.data_length_code; i++) {
                    printf("%02X ", rx_message.data[i]);
                }
                printf(", Status: %s\n", message_with_status.status);
                
                // Store the message in the array
                if (stored_message_count < MAX_STORED_MESSAGES) {
                    stored_messages[stored_message_count++] = message_with_status;
                } else {
                    // Shift messages to make room for the new one
                    for (int i = 0; i < MAX_STORED_MESSAGES - 1; i++) {
                        stored_messages[i] = stored_messages[i + 1];
                    }
                    stored_messages[MAX_STORED_MESSAGES - 1] = message_with_status;
                }
            }
        }
    }
}

// Function to initialize WiFi
void wifi_init_softap() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = 1,
            .authmode = WIFI_AUTH_OPEN,
            .max_connection = 4,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP started. SSID:%s (Open Network)", WIFI_SSID);
}

// Modified HTTP GET handler
esp_err_t get_handler(httpd_req_t *req) {
    char *response = malloc(8192);  // Increased buffer size
    if (response == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for response");
        return ESP_FAIL;
    }

    char *p = response;
    p += sprintf(p, "<html><body>");
    p += sprintf(p, "<h1>TWAI Control Panel</h1>");
    p += sprintf(p, "<button onclick='sendMessages()'>Send 0x742 Messages</button>");
    p += sprintf(p, "<div id='status'></div>");
    p += sprintf(p, "<h2>Filtered 0x762 Messages (byte0=0x23, byte1=0x00)</h2><ul id='messageList'>");

    for (int i = 0; i < stored_message_count; i++) {
        p += sprintf(p, "<li>ID: 0x762, Data: ");
        for (int j = 0; j < stored_messages[i].message.data_length_code; j++) {
            p += sprintf(p, "%02X ", stored_messages[i].message.data[j]);
        }
        p += sprintf(p, ", Status: %s</li>", stored_messages[i].status);
    }

    p += sprintf(p, "</ul>");
    p += sprintf(p, "<script>");
    p += sprintf(p, "function sendMessages() {");
    p += sprintf(p, "  fetch('/send', { method: 'POST' })");
    p += sprintf(p, "    .then(response => response.text())");
    p += sprintf(p, "    .then(data => {");
    p += sprintf(p, "      document.getElementById('status').innerHTML = '<p>Messages sent successfully</p>';");
    p += sprintf(p, "      setTimeout(() => location.reload(), 1000);");
    p += sprintf(p, "    })");
    p += sprintf(p, "    .catch(error => {");
    p += sprintf(p, "      console.error('Error:', error);");
    p += sprintf(p, "      document.getElementById('status').innerHTML = '<p>Error sending messages</p>';");
    p += sprintf(p, "    });");
    p += sprintf(p, "}");
    p += sprintf(p, "setInterval(() => location.reload(), 5000);");
    p += sprintf(p, "</script>");
    p += sprintf(p, "</body></html>");

    httpd_resp_send(req, response, strlen(response));
    free(response);
    return ESP_OK;
}

// Simplified handler for sending TWAI messages
esp_err_t send_handler(httpd_req_t *req) {
    send_twai_messages();
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

// Function to start the webserver
httpd_handle_t start_webserver() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_get = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &uri_get);

        httpd_uri_t uri_send = {
            .uri       = "/send",
            .method    = HTTP_POST,
            .handler   = send_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &uri_send);
    }
    return server;
}

void app_main() {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi
    wifi_init_softap();

    // Start the webserver
    httpd_handle_t server = start_webserver();
    if (server == NULL) {
        ESP_LOGE(TAG, "Failed to start webserver");
    } else {
        ESP_LOGI(TAG, "Webserver started successfully");
    }

    // Initialize TWAI driver
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_GPIO_NUM, RX_GPIO_NUM, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_ERROR_CHECK(twai_start());

    ESP_LOGI(TAG, "TWAI driver installed and started");

    // Create task for receiving TWAI messages
    xTaskCreate(twai_receive_task, "TWAI_receive_task", 2048, NULL, 5, NULL);
}