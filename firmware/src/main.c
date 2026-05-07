/*****************************************************************************
 *  Main application file for the IoT hardware drivers.
 *  This file initializes all the hardware drivers and demonstrates their functionality.
 *****************************************************************************/
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "uart_stdio.h"
#include "led.h"
#include "display.h"
#include "wifi.h"
#include "dht11.h"
#include "adc.h"
#include "light.h"
#include "soil.h"
#include "wpump.h"

//  MQTT CONFIG 
#define MQTT_BROKER_IP "20.240.208.122"
#define MQTT_BROKER_PORT 1883
#define MQTT_CLIENT_ID "arduino_mega_001"
#define MQTT_TOPIC_TELEMETRY "farm/sensor/telemetry"

static bool _mqtt_connected = false;
static char _mqtt_rx_buffer[128] = {0};

// MQTT 
static void mqtt_tcp_callback(void)
{
    // not used but required
}

static uint8_t mqtt_encode_remaining_length(uint16_t value, uint8_t *out)
{
    uint8_t idx = 0;
    do {
        uint8_t encoded = value % 128;
        value /= 128;
        if (value > 0) encoded |= 0x80;
        out[idx++] = encoded;
    } while (value > 0 && idx < 4);
    return idx;
}

static bool mqtt_send_connect_packet(const char *client_id)
{
    uint8_t packet[128];
    uint8_t idx = 0;

    uint8_t rem_len_bytes[4];
    uint16_t len = strlen(client_id);
    uint16_t remaining = 10 + 2 + len;

    uint8_t rem_size = mqtt_encode_remaining_length(remaining, rem_len_bytes);

    packet[idx++] = 0x10;
    for(uint8_t i=0;i<rem_size;i++) packet[idx++] = rem_len_bytes[i];

    // MQTT header
    packet[idx++] = 0x00; packet[idx++] = 0x04;
    packet[idx++] = 'M'; packet[idx++] = 'Q';
    packet[idx++] = 'T'; packet[idx++] = 'T';
    packet[idx++] = 0x04;
    packet[idx++] = 0x02;
    packet[idx++] = 0x00; packet[idx++] = 60;

    packet[idx++] = len >> 8;
    packet[idx++] = len & 0xFF;

    memcpy(&packet[idx], client_id, len);
    idx += len;

    return wifi_command_TCP_transmit(packet, idx) == WIFI_OK;
}

static bool mqtt_send_publish_packet(const char *topic, const char *payload)
{
    uint8_t packet[256];
    uint8_t idx = 0;

    uint16_t topic_len = strlen(topic);
    uint16_t payload_len = strlen(payload);
    uint16_t remaining = 2 + topic_len + payload_len;

    uint8_t rem_len_bytes[4];
    uint8_t rem_size = mqtt_encode_remaining_length(remaining, rem_len_bytes);

    packet[idx++] = 0x30;
    for(uint8_t i=0;i<rem_size;i++) packet[idx++] = rem_len_bytes[i];

    packet[idx++] = topic_len >> 8;
    packet[idx++] = topic_len & 0xFF;

    memcpy(&packet[idx], topic, topic_len);
    idx += topic_len;

    memcpy(&packet[idx], payload, payload_len);
    idx += payload_len;

    return wifi_command_TCP_transmit(packet, idx) == WIFI_OK;
}

static bool mqtt_connect(void)
{
    if (wifi_command_create_TCP_connection(
            MQTT_BROKER_IP,
            MQTT_BROKER_PORT,
            mqtt_tcp_callback,
            _mqtt_rx_buffer) != WIFI_OK)
        return false;

    return mqtt_send_connect_packet(MQTT_CLIENT_ID);
}

//
void read_sensors(uint8_t *temp_int, uint8_t *temp_dec,
                  uint8_t *hum_int, uint8_t *hum_dec,
                  uint16_t *light_value, uint16_t *soil_value)
{
    dht11_get(hum_int, hum_dec, temp_int, temp_dec);
    *light_value = light_measure_raw();
    *soil_value = soil_measure_raw(ADC_PK0);

    display_setDecimals(1);
    display_int((*temp_int) * 10 + (*temp_dec));
}

void build_payload(char *payload, size_t size,
                   uint16_t setup_id,
                   uint8_t temp_int, uint8_t temp_dec,
                   uint8_t hum_int, uint8_t hum_dec,
                   uint16_t light_value, uint16_t soil_value)
{
    uint16_t temperature_x10 = temp_int * 10 + temp_dec;
    uint16_t humidity_x10 = hum_int * 10 + hum_dec;

    snprintf(payload, size,
             "{\"setup_id\":%u,\"sensor_id\":null,\"temperature\":%u,\"humidity\":%u,\"light\":%u,\"soil_moisture\":%u}",
             setup_id, temperature_x10, humidity_x10, light_value, soil_value);
}

//  MQTT + fallback
void send_payload(const char *payload)
{
    if (_mqtt_connected)
    {
        if (!mqtt_send_publish_packet(MQTT_TOPIC_TELEMETRY, payload))
        {
            _mqtt_connected = false;
            printf("MQTT failed, fallback UART: %s\n", payload);
        }
    }
    else
    {
        printf("%s\n", payload);
    }
}


int main(void)
{
    uint16_t setup_id = 1;
    uint8_t temp_int, temp_dec, hum_int, hum_dec;
    uint16_t light_value, soil_value;
    char payload[150];
    uint8_t retry = 0;

    display_init();
    light_init();
    soil_init(ADC_PK0);
    wifi_init();

    if (UART_OK != uart_stdio_init(115200))
    {
        led_on(4);
        while (1);
    }

    sei();

    // WiFi connect
    _delay_ms(4000);
    wifi_command_disable_echo();
    wifi_command_set_mode_to_1();
    wifi_command_set_to_single_Connection();

    if (wifi_command_join_AP("iPhone", "Mita1234") == WIFI_OK)
    {
        _mqtt_connected = mqtt_connect();
    }

    while (1)
    {
        if (!_mqtt_connected && ++retry >= 5)
        {
            _mqtt_connected = mqtt_connect();
            retry = 0;
        }

        read_sensors(&temp_int, &temp_dec,
                     &hum_int, &hum_dec,
                     &light_value, &soil_value);

        build_payload(payload, sizeof(payload),
                      setup_id,
                      temp_int, temp_dec,
                      hum_int, hum_dec,
                      light_value, soil_value);

        send_payload(payload);

        _delay_ms(2000);
    }
}