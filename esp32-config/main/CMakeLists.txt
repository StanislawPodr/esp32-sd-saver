idf_component_register(SRCS "main.c" "uart.c" "uart_message.c" "spiffs.c"
                       PRIV_REQUIRES spiffs esp_driver_gpio esp_driver_uart
                       INCLUDE_DIRS ".")
