#pragma once

#include <stdbool.h>

#define SPIFFS_MOUNT_DIRECTORY "/spiffs"

/*
 * @brief Initialise SPIFFS on /spiffs
 * @return
 *      - true: success
 *      - false: error
 */ 
bool SPIFFS_init(void);

/*
 * @brief Unmount SPIFFS partition
 */
void SPIFFS_deinit(void);
