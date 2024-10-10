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

static const char *TAG = "TWAI_APP";

// Queue to store received 0x762 messages
QueueHandle_t message_queue;

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

void twai_send_task(void *pvParameters) {
    while (1) {
        for (int i = 0; i < sizeof(messages_to_send) / sizeof(messages_to_send[0]); i++) {
            ESP_ERROR_CHECK(twai_transmit(&messages_to_send[i], pdMS_TO_TICKS(1000)));
            ESP_LOGI(TAG, "Message sent: ID=0x%03" PRIx32 ", DLC=%d", messages_to_send[i].identifier, messages_to_send[i].data_length_code);
            vTaskDelay(pdMS_TO_TICKS(1000)); // Wait 1 second between messages
        }
        vTaskDelay(pdMS_TO_TICKS(5000)); // Wait 5 seconds before sending the sequence again
    }
}

void twai_receive_task(void *pvParameters) {
    twai_message_t rx_message;
    while (1) {
        if (twai_receive(&rx_message, pdMS_TO_TICKS(1000)) == ESP_OK) {
            if (rx_message.identifier == 0x762) {
                ESP_LOGI(TAG, "Received message: ID=0x%03" PRIx32 ", DLC=%d, Data=", rx_message.identifier, rx_message.data_length_code);
                for (int i = 0; i < rx_message.data_length_code; i++) {
                    printf("%02X ", rx_message.data[i]);
                }
                printf("\n");
                
                // Send the message to the queue
                xQueueSend(message_queue, &rx_message, portMAX_DELAY);
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

// HTTP GET handler
esp_err_t get_handler(httpd_req_t *req) {
    char response[1024];
    char *p = response;
    p += sprintf(p, "<html><body><h1>Received 0x762 Messages</h1><ul>");

    twai_message_t rx_message;
    while (xQueueReceive(message_queue, &rx_message, 0) == pdTRUE) {
        p += sprintf(p, "<li>ID: 0x762, Data: ");
        for (int i = 0; i < rx_message.data_length_code; i++) {
            p += sprintf(p, "%02X ", rx_message.data[i]);
        }
        p += sprintf(p, "</li>");
    }

    p += sprintf(p, "</ul></body></html>");

    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

// Function to start the webserver
httpd_handle_t start_webserver() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_get = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &uri_get);
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
    start_webserver();

    // Create message queue
    message_queue = xQueueCreate(15, sizeof(twai_message_t));

    // Initialize TWAI driver
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_GPIO_NUM, RX_GPIO_NUM, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_ERROR_CHECK(twai_start());

    ESP_LOGI(TAG, "TWAI driver installed and started");

    // Create tasks for sending and receiving TWAI messages
    xTaskCreate(twai_send_task, "TWAI_send_task", 2048, NULL, 5, NULL);
    xTaskCreate(twai_receive_task, "TWAI_receive_task", 2048, NULL, 5, NULL);
}