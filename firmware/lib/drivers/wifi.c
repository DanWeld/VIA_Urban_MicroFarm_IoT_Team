/**
 * @file wifi.h
 * @author Laurits Ivar / Erland Larsen
 */
#include "wifi.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <util/delay.h>
#include "uart.h"

#define WIFI_DATABUFFERSIZE 128

static uint8_t wifi_dataBuffer[WIFI_DATABUFFERSIZE];
static uint8_t wifi_dataBufferIndex;
static uint32_t wifi_baudrate;
static void (*_callback)(uint8_t byte);

static void wifi_TCP_callback(uint8_t byte);

// Stub for missing callback
void wifi_command_callback(uint8_t byte) {
    // TODO: Implement actual callback logic if needed
    (void)byte;
}

// Stub for missing buffer clear function
void wifi_clear_databuffer_and_index(void) {
    wifi_dataBufferIndex = 0;
    memset(wifi_dataBuffer, 0, sizeof(wifi_dataBuffer));
}

// Stubs for missing WiFi API functions
WIFI_ERROR_MESSAGE_t wifi_command_create_TCP_connection(char *IP, uint16_t port, WIFI_TCP_Callback_t callback_when_message_received, char *received_message_buffer) {
    (void)IP; (void)port; (void)callback_when_message_received; (void)received_message_buffer;
    return WIFI_OK;
}

void wifi_init(void) {
    // TODO: Implement WiFi initialization
}

WIFI_ERROR_MESSAGE_t wifi_command_disable_echo(void) {
    return WIFI_OK;
}

WIFI_ERROR_MESSAGE_t wifi_command_set_mode_to_1(void) {
    return WIFI_OK;
}

WIFI_ERROR_MESSAGE_t wifi_command_set_to_single_Connection(void) {
    return WIFI_OK;
}

WIFI_ERROR_MESSAGE_t wifi_command_join_AP(char *ssid, char *password) {
    (void)ssid; (void)password;
    return WIFI_OK;
}



WIFI_ERROR_MESSAGE_t wifi_command_TCP_transmit(uint8_t * data, uint16_t length)
{
    char sendbuffer[128];
    char lenString[7];

    strcpy(sendbuffer, "AT+CIPSEND=");
    sprintf(lenString, "%u", length);
    strcat(sendbuffer, lenString);

    // Save previous callback
    void* prev_callback = _callback;
    _callback = wifi_command_callback;

    // Send command
    uart_send_string_blocking(UART2_ID, strcat(sendbuffer, "\r\n"));

    bool prompt_received = false;

    // Wait for '>' prompt
    for (uint16_t i = 0; i < 20U * 100U; i++)
    {
        _delay_ms(10);

        if (strchr((char *)wifi_dataBuffer, '>') != NULL)
        {
            prompt_received = true;
            break;
        }

        if (strstr((char *)wifi_dataBuffer, "ERROR") != NULL ||
            strstr((char *)wifi_dataBuffer, "FAIL") != NULL)
        {
            break;
        }
    }

    WIFI_ERROR_MESSAGE_t error;

    if (!prompt_received)
    {
        if (wifi_dataBufferIndex == 0)
            error = WIFI_ERROR_NOT_RECEIVING;
        else if (strstr((char *)wifi_dataBuffer, "ERROR") != NULL)
            error = WIFI_ERROR_RECEIVED_ERROR;
        else if (strstr((char *)wifi_dataBuffer, "FAIL") != NULL)
            error = WIFI_FAIL;
        else
            error = WIFI_ERROR_RECEIVING_GARBAGE;

        wifi_clear_databuffer_and_index();
        _callback = prev_callback;
        return error;
    }

    // Send actual payload
    wifi_clear_databuffer_and_index();
    uart_write_bytes(UART2_ID, data, length);

    // Wait for SEND OK
    for (uint16_t i = 0; i < 10U * 100U; i++)
    {
        _delay_ms(10);

        if (strstr((char *)wifi_dataBuffer, "SEND OK") != NULL)
        {
            wifi_clear_databuffer_and_index();
            _callback = prev_callback;
            return WIFI_OK;
        }

        if (strstr((char *)wifi_dataBuffer, "ERROR") != NULL ||
            strstr((char *)wifi_dataBuffer, "FAIL") != NULL)
        {
            break;
        }
    }

    // Error handling
    if (wifi_dataBufferIndex == 0)
        error = WIFI_ERROR_NOT_RECEIVING;
    else if (strstr((char *)wifi_dataBuffer, "ERROR") != NULL)
        error = WIFI_ERROR_RECEIVED_ERROR;
    else if (strstr((char *)wifi_dataBuffer, "FAIL") != NULL)
        error = WIFI_FAIL;
    else
        error = WIFI_ERROR_RECEIVING_GARBAGE;

    wifi_clear_databuffer_and_index();
    _callback = prev_callback;

    return error;
}