# Comunicación TWAI (CAN) con ESP32 e Interfaz Web

## Descripción General

Este proyecto implementa un sistema de comunicación TWAI (Two-Wire Automotive Interface, también conocido como CAN) utilizando un microcontrolador ESP32-C3. Incluye una interfaz web para monitorear y controlar los mensajes TWAI. El sistema está diseñado para filtrar y mostrar mensajes CAN específicos, centrándose particularmente en mensajes con el identificador 0x762.

## Características

- Comunicación TWAI (CAN) a 500 kbits/s
- Punto de Acceso WiFi para una conexión fácil
- Interfaz web para monitorear mensajes TWAI
- Filtrado de mensajes TWAI específicos (ID 0x762)
- Determinación de estado basada en el contenido del mensaje
- Capacidad de enviar mensajes TWAI predefinidos a través de la interfaz web
- Visualización en tiempo real de los mensajes recibidos
- Actualización automática de la página para actualizaciones en vivo

## Requisitos de Hardware

- Placa de desarrollo ESP32-C3
- Transceptor CAN (por ejemplo, SN65HVD230)
- Conexión al bus CAN de tu sistema objetivo

## Configuración de Pines

- TWAI_TX_PIN: GPIO 18
- TWAI_RX_PIN: GPIO 19

## Requisitos de Software

- ESP-IDF (Espressif IoT Development Framework)
- Navegador web para acceder a la interfaz

## Instrucciones de Configuración

1. Clona este repositorio en tu máquina local.
2. Abre el proyecto en tu entorno de desarrollo ESP-IDF.
3. Configura tu ESP-IDF para el objetivo ESP32-C3.
4. Compila el proyecto utilizando el sistema de construcción de ESP-IDF.
5. Flashea el firmware compilado en tu placa ESP32-C3.

## Uso

1. Enciende tu placa ESP32-C3.
2. Conéctate al Punto de Acceso WiFi llamado "ESP32_AP" (no se requiere contraseña).
3. Abre un navegador web y navega a la dirección IP del ESP32 (típicamente 192.168.4.1).
4. La interfaz web mostrará:
   - Un botón para enviar mensajes TWAI predefinidos
   - Una lista de mensajes TWAI recibidos y filtrados (ID 0x762)
5. Haz clic en el botón "Comprobar estado de la configuración de ángulo de volante" para enviar los mensajes predefinidos.
6. Observa los mensajes recibidos y sus estados en la página web.

## Filtrado de Mensajes TWAI

El sistema filtra los mensajes TWAI con los siguientes criterios:
- ID del mensaje: 0x762
- Primer byte (índice 0): 0x23
- Segundo byte (índice 1): 0x00

## Determinación del Estado

El estado de cada mensaje filtrado se determina por el cuarto byte (índice 3):
- Si el nibble menos significativo es 0xC (por ejemplo, 0xEC, 0x6C, 0x8C), el estado es "Status 4"
- De lo contrario, el estado es "Status 3"

## Personalización

Para modificar la lógica de filtrado de mensajes TWAI o la determinación del estado, edita la función `twai_receive_task` en el archivo `main.c`.

## Solución de Problemas

- Si no puedes conectarte al Punto de Acceso WiFi, asegúrate de que tu ESP32-C3 esté encendido y el firmware esté correctamente flasheado.
- Si no se reciben mensajes TWAI, verifica tus conexiones del bus CAN y asegúrate de que la tasa de bits TWAI coincida con tu red CAN (actualmente configurada a 500 kbit/s).
- Para cualquier otro problema, revisa la salida serial del ESP32 en busca de mensajes de error e información de depuración.


## Contribuciones

Las contribuciones a este proyecto son bienvenidas. Por favor, haz un fork del repositorio y envía un pull request con tus cambios propuestos.

## Soporte

Para preguntas o soporte, por favor abre un issue en el repositorio de GitHub.