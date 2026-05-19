/**
 * @file wifi.c
 * @brief ESP8266 WiFi module driver with AT command implementation
 */
#include "wifi.h"
#include "uart.h"

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <util/delay.h>

#define WIFI_DATABUFFERSIZE 256
#define WIFI_TIMEOUT_MS 5000

static uint8_t  wifi_dataBuffer[WIFI_DATABUFFERSIZE];
static uint8_t  wifi_dataBufferIndex;
static void   (*_callback)(uint8_t byte);

static char    *tcp_received_message_buffer      = NULL;
static uint16_t tcp_received_message_buffer_size  = 0;
static uint16_t tcp_received_message_buffer_index = 0;

static uint8_t  ipd_prefix_index        = 0;
static uint16_t ipd_expected_length     = 0;
static uint16_t ipd_payload_index       = 0;
static uint8_t  ipd_length_buffer[6]    = {0};
static uint8_t  ipd_length_buffer_index = 0;
static uint8_t  ipd_collecting_payload  = 0;
static uint8_t  ipd_packet_buffer[256];

volatile uint8_t wifi_ipd_received = 0;

static void wifi_clear_tcp_received_buffer(void)
{
    if (tcp_received_message_buffer != NULL && tcp_received_message_buffer_size > 0) {
        tcp_received_message_buffer[0] = '\0';
    }
    tcp_received_message_buffer_index = 0;
}

static void wifi_append_to_ascii_buffer(char *buffer, uint16_t buffer_size,
                                         uint16_t *buffer_index, uint8_t byte)
{
    if (buffer == NULL || buffer_size == 0 || buffer_index == NULL) return;
    if (*buffer_index < (uint16_t)(buffer_size - 1U)) {
        buffer[*buffer_index] = (char)byte;
        (*buffer_index)++;
        buffer[*buffer_index] = '\0';
    }
}

static void wifi_reset_ipd_parser(void)
{
    ipd_prefix_index        = 0;
    ipd_expected_length     = 0;
    ipd_payload_index       = 0;
    ipd_length_buffer_index = 0;
    ipd_collecting_payload  = 0;
}

static void wifi_store_mqtt_command(const uint8_t *packet, uint16_t length)
{
    if (tcp_received_message_buffer == NULL || tcp_received_message_buffer_size == 0) return;

    tcp_received_message_buffer[0]    = '\0';
    tcp_received_message_buffer_index = 0;

    if (packet == NULL || length < 4U) return;

    // Only handle PUBLISH packets (type 0x30)
    if ((packet[0] & 0xF0U) != 0x30U) return;

    uint16_t index            = 1U;
    uint32_t remaining_length = 0U;
    uint32_t multiplier       = 1U;
    uint8_t  encoded_byte     = 0U;

    do {
        if (index >= length) return;
        encoded_byte       = packet[index++];
        remaining_length  += (uint32_t)(encoded_byte & 0x7FU) * multiplier;
        multiplier        *= 128U;
    } while ((encoded_byte & 0x80U) != 0U);

    (void)remaining_length;

    if ((uint16_t)(index + 2U) > length) return;

    uint16_t topic_length = ((uint16_t)packet[index] << 8) | (uint16_t)packet[index + 1U];
    index += 2U;

    if ((uint16_t)(index + topic_length) > length) return;

    char     topic[64];
    uint16_t topic_copy_length = topic_length < (uint16_t)(sizeof(topic) - 1U)
                                 ? topic_length
                                 : (uint16_t)(sizeof(topic) - 1U);
    for (uint16_t i = 0; i < topic_copy_length; i++) {
        topic[i] = (char)packet[index + i];
    }
    topic[topic_copy_length] = '\0';
    index += topic_length;

    // Skip packet identifier for QoS 1/2
    if ((packet[0] & 0x06U) != 0U) {
        if ((uint16_t)(index + 2U) > length) return;
        index += 2U;
    }

    if (index > length) return;

    for (uint16_t i = 0; topic[i] != '\0'; i++) {
        wifi_append_to_ascii_buffer(tcp_received_message_buffer,
                                    tcp_received_message_buffer_size,
                                    &tcp_received_message_buffer_index,
                                    (uint8_t)topic[i]);
    }
    wifi_append_to_ascii_buffer(tcp_received_message_buffer,
                                tcp_received_message_buffer_size,
                                &tcp_received_message_buffer_index, ' ');

    while (index < length) {
        wifi_append_to_ascii_buffer(tcp_received_message_buffer,
                                    tcp_received_message_buffer_size,
                                    &tcp_received_message_buffer_index,
                                    packet[index++]);
    }

    wifi_ipd_received = 1;
}

static void wifi_handle_ipd_byte(uint8_t byte)
{
    if (!ipd_collecting_payload) {
        if (ipd_prefix_index == 0U) {
            if (byte == '+') ipd_packet_buffer[ipd_prefix_index++] = byte;
            return;
        }
        if (ipd_prefix_index < 4U) {
            ipd_packet_buffer[ipd_prefix_index++] = byte;
            if ((ipd_prefix_index == 2U && byte != 'I') ||
                (ipd_prefix_index == 3U && byte != 'P') ||
                (ipd_prefix_index == 4U && byte != 'D')) {
                wifi_reset_ipd_parser();
            }
            return;
        }
        if (ipd_prefix_index == 4U) {
            if (byte == ',') {
                ipd_prefix_index++;
                ipd_length_buffer_index = 0U;
                ipd_length_buffer[0]    = '\0';
                return;
            }
            wifi_reset_ipd_parser();
            return;
        }
        if (byte == ':') {
            ipd_expected_length    = (uint16_t)atoi((char *)ipd_length_buffer);
            ipd_collecting_payload = 1U;
            ipd_payload_index      = 0U;
            return;
        }
        if (byte >= '0' && byte <= '9' &&
            ipd_length_buffer_index < (uint8_t)(sizeof(ipd_length_buffer) - 1U)) {
            ipd_length_buffer[ipd_length_buffer_index++] = byte;
            ipd_length_buffer[ipd_length_buffer_index]   = '\0';
            return;
        }
        wifi_reset_ipd_parser();
        return;
    }

    if (ipd_payload_index < (uint16_t)sizeof(ipd_packet_buffer)) {
        ipd_packet_buffer[ipd_payload_index] = byte;
    }
    ipd_payload_index++;

    if (ipd_payload_index >= ipd_expected_length) {
        uint16_t safe_len = (ipd_expected_length < (uint16_t)sizeof(ipd_packet_buffer))
                            ? ipd_expected_length
                            : (uint16_t)sizeof(ipd_packet_buffer);
        wifi_store_mqtt_command(ipd_packet_buffer, safe_len);
        wifi_reset_ipd_parser();
    }
}

static void wifi_rx_callback(uint8_t byte)
{
    if (wifi_dataBufferIndex < WIFI_DATABUFFERSIZE - 1) {
        wifi_dataBuffer[wifi_dataBufferIndex++] = byte;
    }
    wifi_handle_ipd_byte(byte);
}

void wifi_command_callback(uint8_t byte)
{
    if (wifi_dataBufferIndex < WIFI_DATABUFFERSIZE - 1) {
        if (byte == 0U) byte = ' ';
        if (byte == '\r' || byte == '\n' ||
            (byte >= 0x20U && byte <= 0x7EU)) {
            wifi_dataBuffer[wifi_dataBufferIndex++] = byte;
            wifi_dataBuffer[wifi_dataBufferIndex]   = '\0';
        }
    }
}

void wifi_clear_databuffer_and_index(void)
{
    wifi_dataBufferIndex = 0;
    memset(wifi_dataBuffer, 0, sizeof(wifi_dataBuffer));
}

static WIFI_ERROR_MESSAGE_t wifi_send_command(const char *command,
                                               const char *expected_response,
                                               uint32_t timeout_ms)
{
    wifi_clear_databuffer_and_index();

    char cmd_buffer[128];
    snprintf(cmd_buffer, sizeof(cmd_buffer), "%s\r\n", command);
    uart_send_string_blocking(UART2_ID, cmd_buffer);

    uint32_t elapsed = 0;
    while (elapsed < timeout_ms) {
        _delay_ms(50);
        elapsed += 50;

        if (expected_response &&
            strstr((char *)wifi_dataBuffer, expected_response) != NULL) {
            return WIFI_OK;
        }
        if (expected_response && strcmp(expected_response, "OK") != 0 &&
            strstr((char *)wifi_dataBuffer, "OK") != NULL) {
            return WIFI_OK;
        }
        if (strstr((char *)wifi_dataBuffer, "ERROR") != NULL) {
            printf("[wifi] ERROR response for '%s': %s\n", command, (char *)wifi_dataBuffer);
            return WIFI_ERROR_RECEIVED_ERROR;
        }
        if (strstr((char *)wifi_dataBuffer, "FAIL") != NULL) {
            printf("[wifi] FAIL response for '%s': %s\n", command, (char *)wifi_dataBuffer);
            return WIFI_FAIL;
        }
    }

    if (wifi_dataBufferIndex == 0) {
        printf("[wifi] no response for '%s' (timeout=%lu ms)\n",
               command, (unsigned long)timeout_ms);
        return WIFI_ERROR_NOT_RECEIVING;
    }
    if (expected_response != NULL) {
        printf("[wifi] unexpected response for '%s': %s\n",
               command, (char *)wifi_dataBuffer);
        return WIFI_ERROR_RECEIVING_GARBAGE;
    }
    return WIFI_OK;
}

void wifi_init(void)
{
    uart_init(UART2_ID, 115200, wifi_rx_callback, 256);
    _delay_ms(2000);
    wifi_clear_databuffer_and_index();
}

WIFI_ERROR_MESSAGE_t wifi_command_disable_echo(void)
{
    for (int i = 0; i < 3; i++) {
        if (wifi_send_command("ATE0", "OK", WIFI_TIMEOUT_MS) == WIFI_OK) return WIFI_OK;
        _delay_ms(200);
    }
    return WIFI_ERROR_NOT_RECEIVING;
}

WIFI_ERROR_MESSAGE_t wifi_command_set_mode_to_1(void)
{
    for (int i = 0; i < 3; i++) {
        if (wifi_send_command("AT+CWMODE=1", "OK", WIFI_TIMEOUT_MS) == WIFI_OK) return WIFI_OK;
        _delay_ms(200);
    }
    return WIFI_ERROR_NOT_RECEIVING;
}

WIFI_ERROR_MESSAGE_t wifi_command_set_to_single_Connection(void)
{
    for (int i = 0; i < 3; i++) {
        if (wifi_send_command("AT+CIPMUX=0", "OK", WIFI_TIMEOUT_MS) == WIFI_OK) return WIFI_OK;
        _delay_ms(200);
    }
    return WIFI_ERROR_NOT_RECEIVING;
}

WIFI_ERROR_MESSAGE_t wifi_command_get_station_ip(char *ip_address, uint16_t ip_address_size)
{
    WIFI_ERROR_MESSAGE_t result = wifi_send_command("AT+CIFSR", "STAIP", WIFI_TIMEOUT_MS);
    if (result == WIFI_OK && ip_address != NULL && ip_address_size > 0) {
        strncpy(ip_address, (char *)wifi_dataBuffer, ip_address_size - 1);
        ip_address[ip_address_size - 1] = '\0';
    }
    return result;
}

WIFI_ERROR_MESSAGE_t wifi_command_get_connection_status(char *status_buffer,
                                                         uint16_t status_buffer_size)
{
    WIFI_ERROR_MESSAGE_t result = wifi_send_command("AT+CIPSTATUS", "STATUS", WIFI_TIMEOUT_MS);
    if (result == WIFI_OK && status_buffer != NULL && status_buffer_size > 0) {
        strncpy(status_buffer, (char *)wifi_dataBuffer, status_buffer_size - 1);
        status_buffer[status_buffer_size - 1] = '\0';
    }
    return result;
}

WIFI_ERROR_MESSAGE_t wifi_command_join_AP(char *ssid, char *password)
{
    char cmd[160];
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);

    for (int attempt = 0; attempt < 3; attempt++) {
        wifi_clear_databuffer_and_index();
        uart_send_string_blocking(UART2_ID, cmd);
        uart_send_string_blocking(UART2_ID, "\r\n");

        uint32_t elapsed = 0;
        while (elapsed < 15000) {
            _delay_ms(200);
            elapsed += 200;

            if (strstr((char *)wifi_dataBuffer, "WIFI GOT IP") != NULL)    return WIFI_OK;
            if (strstr((char *)wifi_dataBuffer, "WIFI CONNECTED") != NULL)  return WIFI_OK;
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

WIFI_ERROR_MESSAGE_t wifi_command_create_TCP_connection(char *IP, uint16_t port,
                                                         WIFI_TCP_Callback_t callback_when_message_received,
                                                         char *received_message_buffer,
                                                         uint16_t received_message_buffer_size)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u", IP, port);

    _callback                         = callback_when_message_received;
    tcp_received_message_buffer       = received_message_buffer;
    tcp_received_message_buffer_size  = received_message_buffer_size;
    wifi_clear_tcp_received_buffer();
    wifi_reset_ipd_parser();

    return wifi_send_command(cmd, "CONNECTED", 10000);
}

WIFI_ERROR_MESSAGE_t wifi_command_TCP_transmit(uint8_t *data, uint16_t length)
{
    char sendbuffer[128];
    char lenString[7];

    strcpy(sendbuffer, "AT+CIPSEND=");
    sprintf(lenString, "%u", length);
    strcat(sendbuffer, lenString);

    wifi_clear_databuffer_and_index();

    void *prev_callback = _callback;
    _callback = wifi_command_callback;

    uart_send_string_blocking(UART2_ID, strcat(sendbuffer, "\r\n"));

    bool prompt_received = false;
    for (uint16_t i = 0; i < 30U * 100U; i++) {
        _delay_ms(10);
        if (strchr((char *)wifi_dataBuffer, '>') != NULL) {
            prompt_received = true;
            break;
        }
        if (strstr((char *)wifi_dataBuffer, "ERROR") != NULL ||
            strstr((char *)wifi_dataBuffer, "FAIL")  != NULL) {
            break;
        }
    }

    WIFI_ERROR_MESSAGE_t error;
    if (!prompt_received) {
        if      (wifi_dataBufferIndex == 0)                                error = WIFI_ERROR_NOT_RECEIVING;
        else if (strstr((char *)wifi_dataBuffer, "ERROR") != NULL)         error = WIFI_ERROR_RECEIVED_ERROR;
        else if (strstr((char *)wifi_dataBuffer, "FAIL")  != NULL)         error = WIFI_FAIL;
        else                                                                error = WIFI_ERROR_RECEIVING_GARBAGE;
        wifi_clear_databuffer_and_index();
        _callback = prev_callback;
        return error;
    }

    wifi_clear_databuffer_and_index();
    uart_write_bytes(UART2_ID, data, length);
    _delay_ms(100);

    for (uint16_t i = 0; i < 50U * 100U; i++) {
        _delay_ms(10);
        if (strstr((char *)wifi_dataBuffer, "SEND OK") != NULL) {
            wifi_clear_databuffer_and_index();
            _callback = prev_callback;
            return WIFI_OK;
        }
        if (strstr((char *)wifi_dataBuffer, "ERROR") != NULL ||
            strstr((char *)wifi_dataBuffer, "FAIL")  != NULL) {
            break;
        }
    }

    if      (wifi_dataBufferIndex == 0)                                error = WIFI_ERROR_NOT_RECEIVING;
    else if (strstr((char *)wifi_dataBuffer, "ERROR") != NULL)         error = WIFI_ERROR_RECEIVED_ERROR;
    else if (strstr((char *)wifi_dataBuffer, "FAIL")  != NULL)         error = WIFI_FAIL;
    else                                                                error = WIFI_ERROR_RECEIVING_GARBAGE;

    wifi_clear_databuffer_and_index();
    _callback = prev_callback;
    return error;
}

WIFI_ERROR_MESSAGE_t wifi_command_close_TCP_connection(void)
{
    return wifi_send_command("AT+CIPCLOSE", "OK", WIFI_TIMEOUT_MS);
}

WIFI_ERROR_MESSAGE_t wifi_command_quit_AP(void)
{
    return wifi_send_command("AT+CWQAP", "OK", WIFI_TIMEOUT_MS);
}

WIFI_ERROR_MESSAGE_t wifi_command_reset(void)
{
    return wifi_send_command("AT+RST", "ready", 6000);
}

void wifi_debug_scan(void)
{
    wifi_clear_databuffer_and_index();
    uart_send_string_blocking(UART2_ID, "AT+CWLAP\r\n");
    _delay_ms(10000);
    printf("Scan: %s\n", (char *)wifi_dataBuffer);
}