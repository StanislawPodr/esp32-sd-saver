#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "include/spiffs.h"
#include "include/uart.h"
#include "esp_log.h"

#define MAX_VAR_NAME_SIZE 20
#define MAX_LINE_SIZE 60
#define TAG "UART_MESSAGE"
#define MESSAGE_FORMAT "{get|set|del} <varName> [varType] [var]"
#define FILE_PATH SPIFFS_MOUNT_DIRECTORY "/variables.txt"
#define TEMP_FILE_PATH SPIFFS_MOUNT_DIRECTORY "/variables.txt.tmp"

static void setIntVar(const char *const varNameBuff_p, const int variable);
static void setBoolVar(const char *const varNameBuff_p, const int variableBool);
static void setFloatVar(const char *const varNameBuff_p, const float variable);
static void getVar(const char *const varNameBuff_p);
static void delVar(const char *const varNameBuff_p);

static inline bool getVariableLine(const char *const varNameBuff_p, char *const lineBuff_p, const uint16_t lineBuffSize);

/*
 * @TODO Handler to those variables (not necessary now as we just send them)
 *       Why read them if those are just strings. Would require a hash map for decent
 *       performence (and no one will make me to implement it). From what I see
 *       the best solution would be to just use NVS. It's too late now though.
 */

typedef enum MessageMode_e
{
    WRONG_MESSAGE_MODE,
    SET,
    GET,
    DEL
} MessageMode_e;

typedef enum MessageType_e
{
    MESSAGE_VAR_UNDEFINED,
    MESSAGE_VAR_INT,
    MESSAGE_VAR_BOOL,
    MESSAGE_VAR_FLOAT
} MessageType_e;

/*
 * @brief Handle new UART message
 *
 * @param[in] message       incoming UART message
 *
 */
void UART_MESSAGE_handleFullMessage(const char *const message_p)
{
    const char *currMsgPos_p = message_p;
    char varNameBuff[MAX_VAR_NAME_SIZE + 1] = {0};
    MessageMode_e messageMode = WRONG_MESSAGE_MODE;
    MessageType_e messageType = MESSAGE_VAR_UNDEFINED;

    // Skip non alphanumeric caracters
    while (*currMsgPos_p != '\0' && !isalnum((unsigned char)(*currMsgPos_p)))
    {
        currMsgPos_p++;
    }

    // Check initial sequence for set variable
    int initialSeqSize = 0;
    sscanf(currMsgPos_p, "set%*[ \t]%20[a-zA-Z0-9_]%n", &varNameBuff[0], &initialSeqSize);
    if (initialSeqSize > 0)
    {
        messageMode = SET;
    }
    else
    {
        // If not variable set, than check for get
        sscanf(currMsgPos_p, "get%*[ \t]%20[a-zA-Z0-9_]%n", &varNameBuff[0], &initialSeqSize);
        if (initialSeqSize > 0)
        {
            messageMode = GET;
        }
        else
        {
            // If not variable get, than check for del
            sscanf(currMsgPos_p, "del%*[ \t]%20[a-zA-Z0-9_]%n", &varNameBuff[0], &initialSeqSize);
            if (initialSeqSize == 0)
            {
                ESP_LOGW(TAG, "Wrong message format! See:\"" MESSAGE_FORMAT "\"");
                return;
            }
            messageMode = DEL;
        }
    }

    currMsgPos_p += initialSeqSize;

    // Verify that variable has proper name
    if (isdigit((unsigned char)(*varNameBuff)))
    {
        ESP_LOGW(TAG, "Variable name shouldn't start from a digit!");
        return;
    }

    // Check variable type for SET
    int intVar;
    int boolVar;
    float floatVar;
    if (messageMode == SET)
    {
        if (sscanf(currMsgPos_p, "%*[ \t]int%*[ \t]%d", &intVar) == 1)
        {
            messageType = MESSAGE_VAR_INT;
        }
        else if (sscanf(currMsgPos_p, "%*[ \t]bool%*[ \t]%d", &boolVar) == 1)
        {
            messageType = MESSAGE_VAR_BOOL;
        }
        else if (sscanf(currMsgPos_p, "%*[ \t]float%*[ \t]%f", &floatVar) == 1)
        {
            messageType = MESSAGE_VAR_FLOAT;
        }
        else
        {
            ESP_LOGW(TAG, "Variable type not supported! See:\"" MESSAGE_FORMAT "\"");
            return;
        }
    }

    // Handle message with mode and variable type
    if (messageMode == SET)
    {
        switch (messageType)
        {
        case MESSAGE_VAR_INT:
            setIntVar(&varNameBuff[0], intVar);
            break;
        case MESSAGE_VAR_BOOL:
            setBoolVar(&varNameBuff[0], boolVar);
            break;
        case MESSAGE_VAR_FLOAT:
            setFloatVar(&varNameBuff[0], floatVar);
            break;

        default:
            ESP_LOGE(TAG, "Message type unsupported");
            return;
        }
    }
    else if (messageMode == GET)
    {
        getVar(&varNameBuff[0]);
    }
    else if (messageMode == DEL)
    {
        delVar(&varNameBuff[0]);
    }
    else
    {
        ESP_LOGE(TAG, "Message mode unsupported");
    }
}

/*
 * @brief Set an int variable in a variable file
 *
 * @param[in]  varNameBuff_p            Null terminated string with variable name
 * @param[in]  variable                 Integer variable
 */
static void setIntVar(const char *const varNameBuff_p, const int variable)
{
    // Remove variable if it exists
    delVar(varNameBuff_p);

    FILE *file_p = fopen(FILE_PATH, "a");
    if (file_p == NULL)
    {
        ESP_LOGE(TAG, "Can't open/create a file" FILE_PATH);
        return;
    }

    fprintf(file_p, "%-20s int %d\n", varNameBuff_p, variable);
    fclose(file_p);
}

/*
 * @brief Set a bool variable in a variable file
 *
 * @param[in]  varNameBuff_p            Null terminated string with variable name
 * @param[in]  variableBool             Boolean variable
 */
static void setBoolVar(const char *const varNameBuff_p, const int variableBool)
{
    // Remove variable if it exists
    delVar(varNameBuff_p);

    FILE *file_p = fopen(FILE_PATH, "a");
    if (file_p == NULL)
    {
        ESP_LOGE(TAG, "Can't open/create a file" FILE_PATH);
        return;
    }

    fprintf(file_p, "%-20s bool %d\n", varNameBuff_p, (int)(variableBool != 0));
    fclose(file_p);
}

/*
 * @brief Set a float variable in a variable file
 *
 * @param[in]  varNameBuff_p            Null terminated string with variable name
 * @param[in]  variable                 Float variable
 */
static void setFloatVar(const char *const varNameBuff_p, const float variable)
{
    // Remove variable if it exists
    delVar(varNameBuff_p);

    FILE *file_p = fopen(FILE_PATH, "a");
    if (file_p == NULL)
    {
        ESP_LOGE(TAG, "Can't open/create a file" FILE_PATH);
        return;
    }

    fprintf(file_p, "%-20s float %f\n", varNameBuff_p, variable);
    fclose(file_p);
}

/*
 * @brief Get a variable from a variables file
 *
 * @param[in]  varNameBuff_p            Null terminated string with variable name
 */
static void getVar(const char *const varNameBuff_p)
{
    char lineBuff[MAX_LINE_SIZE] = {0};

    if (!getVariableLine(varNameBuff_p, &lineBuff[0], sizeof(lineBuff)))
    {
        return;
    }

    UART_sendMessage(&lineBuff[0]);
}

/*
 * @brief Delete a variable from a variables file
 *
 * @param[in]  varNameBuff_p            Null terminated string with variable name
 */
static void delVar(const char *const varNameBuff_p)
{
    size_t varNameLen = strlen(varNameBuff_p);
    char lineBuff[MAX_LINE_SIZE] = {0};

    FILE *file_p = fopen(FILE_PATH, "r");
    if (file_p == NULL)
    {
        ESP_LOGD(TAG, "File: " FILE_PATH " don't exist");
        return;
    }

    FILE *tempFile_p = fopen(TEMP_FILE_PATH, "w");
    if (tempFile_p == NULL)
    {
        ESP_LOGE(TAG, "Can't create a file: " TEMP_FILE_PATH " !");
        fclose(file_p);
        return;
    }

    // Write lines without variable to temp
    while (fgets(lineBuff, sizeof(lineBuff), file_p) != NULL)
    {
        if (strncmp(varNameBuff_p, lineBuff, varNameLen) != 0 || !isspace((unsigned char)(lineBuff[varNameLen])))
        {
            fputs(lineBuff, tempFile_p);
        }
    }

    fclose(file_p);
    fclose(tempFile_p);

    // Swap and remove
    if (remove(FILE_PATH) != 0)
    {
        ESP_LOGE(TAG, "Can't remove existing file: " FILE_PATH);
        return;
    }

    if (rename(TEMP_FILE_PATH, FILE_PATH))
    {
        ESP_LOGE(TAG, "Can't rename file: " TEMP_FILE_PATH);
        return;
    }
}

/*
 * @brief Read the line that defines a variable to a buffer
 *
 * @param[in]  varNameBuff_p        Pointer to a null terminated variable name string
 * @param[out] lineBuff_p           Pointer to an output line
 * @param[in]  lineBuffSize         Size of the buffer for lines
 *
 * @return
 *      - true: variable found
 *      - false: variable not found
 */
static inline bool getVariableLine(const char *const varNameBuff_p, char *const lineBuff_p, const uint16_t lineBuffSize)
{
    FILE *file_p = fopen(FILE_PATH, "r");
    if (file_p == NULL)
    {
        return false;
    }

    size_t varNameLen = strlen(varNameBuff_p);

    while (fgets(lineBuff_p, lineBuffSize, file_p) != NULL)
    {
        if (strncmp(varNameBuff_p, lineBuff_p, varNameLen) == 0)
        {
            if (isspace((unsigned char)(lineBuff_p[varNameLen])))
            {
                fclose(file_p);
                return true;
            }
        }
    }

    fclose(file_p);

    return false;
}
