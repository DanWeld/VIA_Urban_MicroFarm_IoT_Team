/*****************************************************************************
 * main.c
 *  Main application file for the IoT hardware drivers demo.
 *  This file initializes all the hardware drivers and demonstrates their
 *  functionality.
 *  Push button 2 on the shield during reset to enter continious sensor
 *  reading mode. Otherwise the program will run an interactive demo that
 *  allows you to test each driver individually by sending commands over UART.
 *  See interactive.c for details.
 * 
 *****************************************************************************/
#ifndef WINDOWS_TEST
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#endif
#include <stdio.h>

#include <string.h>
#include <stdbool.h>
#include "interactive.h"
#include "button.h"
#include "uart_stdio.h"
#include "led.h"
#include "pir.h"
#include "display.h"
#include "wifi.h"
#include "buzzer.h"
#include "dht11.h"
#include "proximity.h"
#include "servo.h"
#include "adc.h"
#include "light.h"
#include "soil.h"
#include "tone.h"
#include "timer.h"

//#include "adxl345.h"

uint8_t humidity_integer, humidity_decimal, temperature_integer, temperature_decimal;
static int8_t _led_no = 0;
static bool _mqtt_connected = false;
static bool _mqtt_message_received = false;
static char _mqtt_rx_buffer[128] = {0};
//static int16_t _x, _y, _z;


#define MQTT_BROKER_IP "192.168.1.61"
#define MQTT_BROKER_PORT 1883
#define MQTT_CLIENT_ID "arduino_mega_001"
#define MQTT_TOPIC_TELEMETRY "farm/sensor/telemetry"

static void mqtt_tcp_callback(void)
{
    _mqtt_message_received = true;
}

static uint8_t mqtt_encode_remaining_length(uint16_t value, uint8_t *out)
{
    uint8_t idx = 0;

    do {
        uint8_t encoded = (uint8_t)(value % 128U);
        value /= 128U;
        if (value > 0U)
            encoded |= 0x80U;
        out[idx++] = encoded;
    } while (value > 0U && idx < 4U);

    return idx;
}

static bool mqtt_send_connect_packet(const char *client_id)
{
    uint8_t packet[128];
    uint8_t idx = 0;
    uint8_t rem_len_bytes[4];
    uint16_t client_id_len = (uint16_t)strlen(client_id);
    uint16_t remaining_length = (uint16_t)(10U + 2U + client_id_len);
    uint8_t rem_len_size = mqtt_encode_remaining_length(remaining_length, rem_len_bytes);

    packet[idx++] = 0x10; // CONNECT
    for (uint8_t i = 0; i < rem_len_size; i++)
        packet[idx++] = rem_len_bytes[i];

    // Variable header
    packet[idx++] = 0x00;
    packet[idx++] = 0x04;
    packet[idx++] = 'M';
    packet[idx++] = 'Q';
    packet[idx++] = 'T';
    packet[idx++] = 'T';
    packet[idx++] = 0x04; // MQTT 3.1.1
    packet[idx++] = 0x02; // Clean session
    packet[idx++] = 0x00;
    packet[idx++] = 60;   // Keep alive 60s

    // Payload (client id)
    packet[idx++] = (uint8_t)((client_id_len >> 8) & 0xFF);
    packet[idx++] = (uint8_t)(client_id_len & 0xFF);
    memcpy(&packet[idx], client_id, client_id_len);
    idx += (uint8_t)client_id_len;

    return (wifi_command_TCP_transmit(packet, idx) == WIFI_OK);
}

static bool mqtt_send_publish_packet(const char *topic, const char *payload)
{
    uint8_t packet[256];
    uint8_t idx = 0;
    uint8_t rem_len_bytes[4];
    uint16_t topic_len = (uint16_t)strlen(topic);
    uint16_t payload_len = (uint16_t)strlen(payload);
    uint16_t remaining_length = (uint16_t)(2U + topic_len + payload_len);
    uint8_t rem_len_size = mqtt_encode_remaining_length(remaining_length, rem_len_bytes);

    if ((uint16_t)(1U + rem_len_size + remaining_length) > sizeof(packet))
        return false;

    packet[idx++] = 0x30; // PUBLISH, QoS 0
    for (uint8_t i = 0; i < rem_len_size; i++)
        packet[idx++] = rem_len_bytes[i];

    packet[idx++] = (uint8_t)((topic_len >> 8) & 0xFF);
    packet[idx++] = (uint8_t)(topic_len & 0xFF);
    memcpy(&packet[idx], topic, topic_len);
    idx += (uint8_t)topic_len;

    memcpy(&packet[idx], payload, payload_len);
    idx += (uint8_t)payload_len;

    return (wifi_command_TCP_transmit(packet, idx) == WIFI_OK);
}

static bool mqtt_connect_broker(void)
{
    WIFI_ERROR_MESSAGE_t tcp_result = wifi_command_create_TCP_connection(
        MQTT_BROKER_IP,
        MQTT_BROKER_PORT,
        mqtt_tcp_callback,
        _mqtt_rx_buffer);

    if (tcp_result != WIFI_OK) {
        printf("Failed to open TCP to broker %s:%u. Error code: %d\n", MQTT_BROKER_IP, MQTT_BROKER_PORT, tcp_result);
        return false;
    }

    if (!mqtt_send_connect_packet(MQTT_CLIENT_ID)) {
        printf("Failed to send MQTT CONNECT packet.\n");
        return false;
    }

    printf("MQTT CONNECT sent to broker %s:%u (client_id=%s).\n", MQTT_BROKER_IP, MQTT_BROKER_PORT, MQTT_CLIENT_ID);
    return true;
}

// Reads all sensor values and updates the 7-segment display
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

// Builds the telemetry payload in the agreed JSON contract format
void build_payload(char *payload, size_t size,
                   uint16_t setup_id,
                   uint8_t temp_int, uint8_t temp_dec,
                   uint8_t hum_int, uint8_t hum_dec,
                   uint16_t light_value, uint16_t soil_value)
{
    snprintf(payload, size,
             "{\"setup_id\":%u,\"sensor_id\":null,\"temperature\":%u.%u,\"humidity\":%u.%u,\"light\":%u.%u,\"soil_moisture\":%u.%u}",
             setup_id,
             temp_int, (uint8_t)(temp_dec % 10),
             hum_int, (uint8_t)(hum_dec % 10),
             (uint16_t)(light_value / 10), (uint8_t)(light_value % 10),
             (uint16_t)(soil_value / 10), (uint8_t)(soil_value % 10));
}

// Sends the payload through UART for now and can later be replaced by Wi-Fi/MQTT sending
void send_payload(const char *payload)
{
    if (_mqtt_connected) {
        if (mqtt_send_publish_packet(MQTT_TOPIC_TELEMETRY, payload)) {
            printf("MQTT published to %s: %s\n", MQTT_TOPIC_TELEMETRY, payload);
        } else {
            _mqtt_connected = false;
            printf("MQTT publish failed, UART fallback: %s\n", payload);
        }
    } else {
        printf("MQTT not connected, UART fallback: %s\n", payload);
    }
}

void timer_callback(uint8_t id)
{
    led_toggle((_led_no%4) + 1); // Toggle LEDs in sequence 1-4
    _led_no++;
}

int main(void)
{
    uint16_t setup_id = 1;
    uint8_t temp_int, temp_dec, hum_int, hum_dec;
    uint16_t light_value, soil_value;
    char payload[150];
    uint8_t mqtt_retry_counter = 0;

    // Initialize UART first for debug output
    if (UART_OK != uart_stdio_init(115200))
    {
        led_on(4); // Turn on LED4 to indicate error
        while (1)
            ;
    }
    sei(); // Enable global interrupts
    printf("VIA UNIVERSITY COLLEGE SEP4 IoT Hardware DRIVERS DEMO\n");

    // Initialize all hardware
    led_init();
    button_init();
    display_init();
    proximity_init();
    light_init();
    soil_init(ADC_PK0);
    pir_init(pir_callback);
    wifi_init();

    // WiFi module needs time to initialize (typically 2-4 seconds)
    printf("Initializing WiFi module...\n");
    _delay_ms(4000);

    // Verify WiFi module is responsive
    printf("Testing WiFi module communication...\n");
    if (wifi_command_AT() == WIFI_OK) {
        printf("WiFi module is responsive.\n");
    } else {
        printf("Warning: WiFi module not responding to AT command.\n");
    }

    // Disable echo for cleaner responses
    printf("Disabling WiFi echo...\n");
    if (wifi_command_disable_echo() == WIFI_OK) {
        printf("Echo disabled.\n");
    } else {
        printf("Warning: Failed to disable echo.\n");
    }

    // Set to station mode (mode 1)
    printf("Setting WiFi to station mode...\n");
    if (wifi_command_set_mode_to_1() == WIFI_OK) {
        printf("WiFi mode set to station.\n");
    } else {
        printf("Failed to set WiFi mode.\n");
    }

    // Set to single connection mode (recommended)
    printf("Setting WiFi to single connection mode...\n");
    if (wifi_command_set_to_single_Connection() == WIFI_OK) {
        printf("WiFi set to single connection mode.\n");
    } else {
        printf("Warning: Failed to set single connection mode.\n");
    }

    // Connect to WiFi hotspot
    printf("Connecting to WiFi hotspot '3Bredband-CB45'...\n");
    WIFI_ERROR_MESSAGE_t join_result = wifi_command_join_AP("3Bredband-CB45", "t+hPgqG^ma");
    if (join_result == WIFI_OK) {
        printf("Connected to WiFi hotspot '3Bredband-CB45'.\n");
        _mqtt_connected = mqtt_connect_broker();
    } else {
        printf("Failed to connect to WiFi hotspot '3Bredband-CB45'. Error code: %d\n", join_result);
    }

    servo_init(PWM_NORMAL);

    // Enter interactive demo only when button 2 is pressed during startup.
    // Default behavior is telemetry mode.
    if(button_get(2))
    {
        interactive_demo();
    }

    timer_create_sw(timer_callback, 1000); // Create a timer that toggles an LED every 1 second
    tone_play_starwars();

    // Test servo by sweeping from -90 to +90 degrees and back
    servo_start();
    for(int i=-90; i<=90; i+=10)
    {
        servo_setAngle(PWM_A, (int8_t)i);
        printf("Servo set to %d degrees.\n", i);
        _delay_ms(100);
    }
    servo_stop();

    // Test WiFi by sending AT command and printing response
    if(WIFI_OK == wifi_command_AT())
    {
        printf("WiFi module responded to AT command.\n");
    }
    else
    {
        printf("WiFi module did not respond to AT command.\n");
    }

    buzzer_beep();

    // Continuous sensor readings
    while (1)
    {
        if (!_mqtt_connected)
        {
            mqtt_retry_counter++;
            if (mqtt_retry_counter >= 5)
            {
                printf("MQTT reconnect attempt...\n");
                _mqtt_connected = mqtt_connect_broker();
                mqtt_retry_counter = 0;
            }
        }

        // Read sensors and update display with temperature
        read_sensors(&temp_int, &temp_dec,
                     &hum_int, &hum_dec,
                     &light_value, &soil_value);

        // Build payload string and print to UART/Wi-Fi/MQTT
        build_payload(payload, sizeof(payload),
                      setup_id,
                      temp_int, temp_dec,
                      hum_int, hum_dec,
                      light_value, soil_value);

        send_payload(payload);

        _delay_ms(2000);
    }

    return 0;
}
