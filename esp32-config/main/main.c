#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "include/spiffs.h"
#include "include/uart.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#define TAG "app_main"

void app_main(void)
{
    if (SPIFFS_init())
    {
        UART_init();
    }
    else
    {
        ESP_LOGE(TAG, "Could not init SPIFFS!");
    }
}
