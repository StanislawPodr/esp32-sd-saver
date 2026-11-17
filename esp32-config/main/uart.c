#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "include/uart_message.h"
#include "driver/uart_vfs.h"

#define UART_NUM UART_NUM_0
#define TX_PIN GPIO_NUM_1
#define RX_PIN GPIO_NUM_3
#define RX_BUF_SIZE 1024
#define TX_BUF_SIZE 1024
#define TAG "UART"
#define MAX_MESSAGE_SIZE (2 * RX_BUF_SIZE)
#define MESSAGE_SEPARATOR '\n'

static void uart_event_task(void *pvParameters);
static inline void handleMessageChunk(const char *data_p, const size_t dataSize);
static inline void updateMessageBuffer(const char *const data_p, const uint16_t dataToMessage);

// UART config
static QueueHandle_t uart_queue;
static const uart_config_t uart_config = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
};

// Null terminated message string
static char messageBuff[MAX_MESSAGE_SIZE + 1] = {0};
static uint16_t messageSize = 0;

// UART_DATA read buffer
static uint8_t readBuff[RX_BUF_SIZE + 1];

/*
 * @brief Begin UART message interrupts handler with default config
 */
void UART_init(void)
{
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, RX_BUF_SIZE, TX_BUF_SIZE, 20, &uart_queue, 0));

    uart_vfs_dev_use_driver(UART_NUM_0);

    // Event handler
    xTaskCreate(uart_event_task, "uart_event_task", 4096, NULL, 12, NULL);
}

/*
 * @brief Send null terminated string on UART
 */
void UART_sendMessage(const char *const message_p)
{
    uart_write_bytes(UART_NUM, message_p, strlen(message_p));
}

/*
 * @brief UART interrupts handler
 */
static void uart_event_task(void *pvParameters)
{
    uart_event_t event;

    while (1)
    {
        if (xQueueReceive(uart_queue, (void *)&event, portMAX_DELAY))
        {
            switch (event.type)
            {
            case UART_DATA:
                uart_read_bytes(UART_NUM, &readBuff[0], event.size, portMAX_DELAY);
                readBuff[event.size] = '\0';
                ESP_LOGI(TAG, "Bytes received %dB]: %s", event.size, (char *)&readBuff[0]);
                handleMessageChunk((char *)&readBuff[0], event.size);
                break;

            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "FIFO overflow");
                uart_flush_input(UART_NUM);
                messageBuff[0] = '\0';
                messageSize = 0;
                xQueueReset(uart_queue);
                break;

            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "Ring buffer full");
                uart_flush_input(UART_NUM);
                messageBuff[0] = '\0';
                messageSize = 0;
                xQueueReset(uart_queue);
                break;

            case UART_BREAK:
                ESP_LOGI(TAG, "Break signal detected");
                break;

            case UART_PARITY_ERR:
            case UART_FRAME_ERR:
            case UART_DATA_BREAK:
                ESP_LOGE(TAG, "Data error");
                break;

            case UART_PATTERN_DET:
                ESP_LOGW(TAG, "Pattern detect currently not supported");
                break;

            default:
                ESP_LOGE(TAG, "Unknown signal detected: %d", event.type);
                break;
            }
        }
    }
}

/*
 * @brief Convert UART message chunks to messages
 *
 * @param[in] data_p                Pointer to incoming UART data
 * @param[in] dataSize              Incoming chunk size in bytes
 */
static inline void handleMessageChunk(const char *data_p, const size_t dataSize)
{
    size_t dataLeft = dataSize;
    char *messageEndInData_p;
    do
    {
        messageEndInData_p = memchr(data_p, MESSAGE_SEPARATOR, dataLeft);
        if (messageEndInData_p != NULL)
        {
            uint16_t noOfBytesToCopy = (uint16_t)(messageEndInData_p - data_p);

            updateMessageBuffer(data_p, noOfBytesToCopy);

            data_p += noOfBytesToCopy + 1;
            dataLeft -= noOfBytesToCopy + 1;

            UART_MESSAGE_handleFullMessage(&messageBuff[0]);

            messageBuff[0] = '\0';
            messageSize = 0;
        }
        else
        {
            updateMessageBuffer(data_p, dataLeft);
            dataLeft = 0;
        }
    } while (dataLeft > 0);
}

/*
 * @brief Update null terminated string in Message buffer
 *
 * @param[in] data_p               Pointer to incoming UART data chunk
 * @param[in] dataToMessage        Number of bytes to copy from chunk
 */
static inline void updateMessageBuffer(const char *const data_p, const uint16_t dataToMessage)
{
    // We need to keep only last MAX_MESSAGE_SIZE bytes for a message
    if (messageSize + dataToMessage > MAX_MESSAGE_SIZE)
    {
        uint16_t bytesToRemove = messageSize + dataToMessage - MAX_MESSAGE_SIZE;
        messageSize -= bytesToRemove;
        memmove(&messageBuff[0], &messageBuff[bytesToRemove], messageSize);
    }

    memcpy(&messageBuff[messageSize], data_p, dataToMessage);
    messageSize += dataToMessage;
    messageBuff[messageSize] = '\0';
}
