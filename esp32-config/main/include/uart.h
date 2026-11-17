#pragma once

/*
 * @brief Begin UART message interrupts handler with default config
 */
void UART_init(void);

/*
 * @brief Send null terminated string on UART
 */
void UART_sendMessage(const char *const message_p);
