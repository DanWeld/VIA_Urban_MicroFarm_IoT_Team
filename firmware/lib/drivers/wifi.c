/**
 * @file wifi.c
 * @brief ESP8266 WiFi module driver with AT command implementation
 */
#include "wifi.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <util/delay.h>
#include "uart.h"

#define WIFI_DATABUFFERSIZE 256
#define WIFI_TIMEOUT_MS 5000

static uint8_t wifi_dataBuffer[WIFI_DATABUFFERSIZE];
static uint8_t wifi_dataBufferIndex;
static uint32_t wifi_baudrate;
static void (*_callback)(uint8_t byte);

// WiFi receive callback for handling incoming data
static void wifi_rx_callback(uint8_t byte) {
    if (wifi_dataBufferIndex < WIFI_DATABUFFERSIZE - 1) {
        wifi_dataBuffer[wifi_dataBufferIndex++] = byte;
    }
}

// Command callback (for TCP transmit)
void wifi_command_callback(uint8_t byte) {
    if (wifi_dataBufferIndex < WIFI_DATABUFFERSIZE - 1) {
        wifi_dataBuffer[wifi_dataBufferIndex++] = byte;
    }
}

// Clear buffer
void wifi_clear_databuffer_and_index(void) {
    wifi_dataBufferIndex = 0;
    memset(wifi_dataBuffer, 0, sizeof(wifi_dataBuffer));
}

// Send AT command and wait for response
static WIFI_ERROR_MESSAGE_t wifi_send_command(const char *command, const char *expected_response, uint32_t timeout_ms) {
    wifi_clear_databuffer_and_index();
    
    char cmd_buffer[128];
    snprintf(cmd_buffer, sizeof(cmd_buffer), "%s\r\n", command);
    uart_send_string_blocking(UART2_ID, cmd_buffer);
    
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms) {
        _delay_ms(50);
        elapsed += 50;

        // Check for expected response
        if (expected_response && strstr((char *)wifi_dataBuffer, expected_response) != NULL) {
            return WIFI_OK;
        }

        // Check for common success responses as fallback
        if (expected_response && strcmp(expected_response, "OK") != 0) {
            if (strstr((char *)wifi_dataBuffer, "OK") != NULL) {
                return WIFI_OK;
            }
        }

        // Check for errors
        if (strstr((char *)wifi_dataBuffer, "ERROR") != NULL) {
            // Print debug buffer for diagnostics
            printf("[wifi] ERROR response for '%s': %s\n", command, (char *)wifi_dataBuffer);
            return WIFI_ERROR_RECEIVED_ERROR;
        }
        if (strstr((char *)wifi_dataBuffer, "FAIL") != NULL) {
            printf("[wifi] FAIL response for '%s': %s\n", command, (char *)wifi_dataBuffer);
            return WIFI_FAIL;
        }
    }

    if (wifi_dataBufferIndex == 0) {
        // No response received at all
        printf("[wifi] no response for '%s' (timeout=%lu ms)\n", command, (unsigned long)timeout_ms);
        return WIFI_ERROR_NOT_RECEIVING;
    }

    // If we got some response but not the expected one, print it for debugging
    if (expected_response != NULL) {
        printf("[wifi] unexpected response for '%s': %s\n", command, (char *)wifi_dataBuffer);
        return WIFI_ERROR_RECEIVING_GARBAGE;
    }

    return WIFI_OK; // no specific expected response requested
}

// Initialize WiFi module
void wifi_init(void) {
    uart_init(UART2_ID, 115200, wifi_rx_callback, 256);
    _delay_ms(2000);  // Wait for module to stabilize
    wifi_clear_databuffer_and_index();
}

// AT command - Echo disable
WIFI_ERROR_MESSAGE_t wifi_command_disable_echo(void) {
    // Retry a few times if no response
    for (int i = 0; i < 3; i++) {
        WIFI_ERROR_MESSAGE_t r = wifi_send_command("ATE0", "OK", WIFI_TIMEOUT_MS);
        if (r == WIFI_OK) return WIFI_OK;
        _delay_ms(200);
    }
    return WIFI_ERROR_NOT_RECEIVING;
}

// AT+CWMODE=1 - Station mode
WIFI_ERROR_MESSAGE_t wifi_command_set_mode_to_1(void) {
    for (int i = 0; i < 3; i++) {
        WIFI_ERROR_MESSAGE_t r = wifi_send_command("AT+CWMODE=1", "OK", WIFI_TIMEOUT_MS);
        if (r == WIFI_OK) return WIFI_OK;
        _delay_ms(200);
    }
    return WIFI_ERROR_NOT_RECEIVING;
}

// AT+CIPMUX=0 - Single connection mode
WIFI_ERROR_MESSAGE_t wifi_command_set_to_single_Connection(void) {
    for (int i = 0; i < 3; i++) {
        WIFI_ERROR_MESSAGE_t r = wifi_send_command("AT+CIPMUX=0", "OK", WIFI_TIMEOUT_MS);
        if (r == WIFI_OK) return WIFI_OK;
        _delay_ms(200);
    }
    return WIFI_ERROR_NOT_RECEIVING;
}

// Query station IP address with AT+CIFSR
WIFI_ERROR_MESSAGE_t wifi_command_get_station_ip(char *ip_address, uint16_t ip_address_size) {
    WIFI_ERROR_MESSAGE_t result = wifi_send_command("AT+CIFSR", "STAIP", WIFI_TIMEOUT_MS);
    if (result == WIFI_OK && ip_address != NULL && ip_address_size > 0) {
        strncpy(ip_address, (char *)wifi_dataBuffer, ip_address_size - 1);
        ip_address[ip_address_size - 1] = '\0';
    }
    return result;
}

// Query connection status with AT+CIPSTATUS
WIFI_ERROR_MESSAGE_t wifi_command_get_connection_status(char *status_buffer, uint16_t status_buffer_size) {
    WIFI_ERROR_MESSAGE_t result = wifi_send_command("AT+CIPSTATUS", "STATUS", WIFI_TIMEOUT_MS);
    if (result == WIFI_OK && status_buffer != NULL && status_buffer_size > 0) {
        strncpy(status_buffer, (char *)wifi_dataBuffer, status_buffer_size - 1);
        status_buffer[status_buffer_size - 1] = '\0';
    }
    return result;
}

// AT+CWJAP - Connect to AP
WIFI_ERROR_MESSAGE_t wifi_command_join_AP(char *ssid, char *password) {
    char cmd[160];
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);

    // Send the command directly and wait for either "WIFI CONNECTED" or "WIFI GOT IP".
    for (int attempt = 0; attempt < 3; attempt++) {
        wifi_clear_databuffer_and_index();
        uart_send_string_blocking(UART2_ID, cmd);
        uart_send_string_blocking(UART2_ID, "\r\n");

        uint32_t elapsed = 0;
        while (elapsed < 15000) {
            _delay_ms(200);
            elapsed += 200;

            if (strstr((char *)wifi_dataBuffer, "WIFI GOT IP") != NULL) {
                return WIFI_OK;
            }
            if (strstr((char *)wifi_dataBuffer, "WIFI CONNECTED") != NULL) {
                return WIFI_OK;
            }
            if (strstr((char *)wifi_dataBuffer, "ERROR") != NULL) {
                printf("[wifi] CWJAP ERROR: %s\n", (char *)wifi_dataBuffer);
                break;
            }
            if (strstr((char *)wifi_dataBuffer, "FAIL") != NULL) {
                printf("[wifi] CWJAP FAIL: %s\n", (char *)wifi_dataBuffer);
                break;
            }
        }
        _delay_ms(500);
    }
    return WIFI_ERROR_NOT_RECEIVING;
}

// AT+CIPSTART - Create TCP connection
WIFI_ERROR_MESSAGE_t wifi_command_create_TCP_connection(char *IP, uint16_t port, WIFI_TCP_Callback_t callback_when_message_received, char *received_message_buffer) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u", IP, port);
    
    _callback = callback_when_message_received;
    WIFI_ERROR_MESSAGE_t result = wifi_send_command(cmd, "CONNECTED", 10000);
    
    return result;
}

// AT+CIPSEND - Transmit data over TCP
WIFI_ERROR_MESSAGE_t wifi_command_TCP_transmit(uint8_t * data, uint16_t length)
{
    char sendbuffer[128];
    char lenString[7];

    strcpy(sendbuffer, "AT+CIPSEND=");
    sprintf(lenString, "%u", length);
    strcat(sendbuffer, lenString);

    // Clear buffer before sending command
    wifi_clear_databuffer_and_index();

    // Save previous callback
    void* prev_callback = _callback;
    _callback = wifi_command_callback;

    // Send command
    uart_send_string_blocking(UART2_ID, strcat(sendbuffer, "\r\n"));

    bool prompt_received = false;

    // Wait for '>' prompt (3 seconds)
    for (uint16_t i = 0; i < 30U * 100U; i++)
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
    
    // Give ESP8266 time to process and send the data
    _delay_ms(100);

    // Wait for SEND OK (5 seconds for large payloads)
    for (uint16_t i = 0; i < 50U * 100U; i++)
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

// AT+CIPCLOSE - Close TCP connection
WIFI_ERROR_MESSAGE_t wifi_command_close_TCP_connection(void) {
    return wifi_send_command("AT+CIPCLOSE", "OK", WIFI_TIMEOUT_MS);
}

// AT+CWQAP - Disconnect from AP
WIFI_ERROR_MESSAGE_t wifi_command_quit_AP(void) {
    return wifi_send_command("AT+CWQAP", "OK", WIFI_TIMEOUT_MS);
}