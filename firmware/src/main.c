/*****************************************************************************
 *  Urban MicroFarm IoT Device Firmware
 *  MQTT telemetry and command handling
 *****************************************************************************/
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
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
#include "commands.h"
#include "connections_commands.h"
#include "mqtt_commands.h"

// Configuration
//#define MQTT_BROKER_IP            "20.240.208.122"
//#define MQTT_BROKER_PORT          1883
//#define MQTT_CLIENT_ID            "arduino_mega_001"
#define SETUP_ID                  1
#define SENSOR_ID                 1
#define TELEMETRY_INTERVAL_SEC    15
#define HEARTBEAT_INTERVAL_SEC    30
#define SENSOR_DISPLAY_INTERVAL   5

// Runtime state
static uint16_t setup_id = SETUP_ID;            // Fixed device setup ID for MQTT topics
static bool mqtt_connected = false;
static char mqtt_rx_buffer[256] = {0};
static uint16_t telemetry_counter = 0;         // Counts 100ms intervals (600 = 60 sec)
static uint16_t heartbeat_counter = 0;         // Counts 100ms intervals (300 = 30 sec)
static uint16_t sensor_display_counter = 0;    // Counts 100ms intervals (50 = 5 sec)
static uint32_t millis_counter = 0;
static bool mqtt_command_received = false;

static void handle_backend_command(const char *payload) {
    if (strstr(payload, "\"actuator\":\"water_pump\"") != NULL) {
        const char *amount_ptr = strstr(payload, "\"amount_ml\":");
        uint16_t amount_ml = 0;

        if (amount_ptr != NULL) {
            amount_ptr += strlen("\"amount_ml\":");
            amount_ml = (uint16_t)atoi(amount_ptr);
        }

        printf("MQTT command received: water_pump %u ml\n", amount_ml);
        wpump_start();
        for (uint16_t step = 0; step < amount_ml; step++) {
            _delay_ms(100);
        }
        wpump_stop();
        printf("Water pump command completed\n");
    }
}

static void poll_mqtt_incoming(void) {
    if (!mqtt_connected) {
        return;
    }

    if (mqtt_rx_buffer[0] == '\0') {
        return;
    }

    if (strstr(mqtt_rx_buffer, "farm/") != NULL && strstr(mqtt_rx_buffer, "/cmd") != NULL) {
        handle_backend_command(mqtt_rx_buffer);
        mqtt_rx_buffer[0] = '\0';
        mqtt_command_received = true;
    }
}

uint32_t millis(void) {
    return millis_counter / 2;  // Assuming ~2ms per tick
}

static bool wait_for_station_ip(void) {
    char ip_buffer[128] = {0};

    printf("Waiting for WiFi IP address...\n");
    for (uint8_t attempt = 0; attempt < 20; attempt++) {
        if (wifi_command_get_station_ip(ip_buffer, sizeof(ip_buffer)) == WIFI_OK) {
            printf("WiFi IP acquired: %s\n", ip_buffer);
            return true;
        }
        _delay_ms(1000);
    }

    printf("WiFi IP not acquired\n");
    return false;
}

static void log_connection_status(void) {
    char status_buffer[160] = {0};

    if (wifi_command_get_connection_status(status_buffer, sizeof(status_buffer)) == WIFI_OK) {
        printf("ESP8266 connection status: %s\n", status_buffer);
    } else {
        printf("ESP8266 connection status query failed\n");
    }
}

// MQTT: Encode remaining length for MQTT packet
static uint8_t mqtt_encode_remaining_length(uint16_t value, uint8_t *out) {
    uint8_t idx = 0;
    do {
        uint8_t encoded = value % 128;
        value /= 128;
        if (value > 0) encoded |= 0x80;
        out[idx++] = encoded;
    } while (value > 0 && idx < 4);
    return idx;
}

// MQTT: Send CONNECT packet
static bool mqtt_send_connect_packet(void) {
    uint8_t packet[128];
    uint8_t idx = 0;
    
    uint8_t rem_len_bytes[4];
    uint16_t len = strlen(MQTT_CLIENT_ID);
    uint16_t remaining = 10 + 2 + len;
    
    uint8_t rem_size = mqtt_encode_remaining_length(remaining, rem_len_bytes);
    
    packet[idx++] = 0x10;  // CONNECT control packet
    for(uint8_t i = 0; i < rem_size; i++) packet[idx++] = rem_len_bytes[i];
    
    // MQTT protocol name
    packet[idx++] = 0x00;
    packet[idx++] = 0x04;
    packet[idx++] = 'M';
    packet[idx++] = 'Q';
    packet[idx++] = 'T';
    packet[idx++] = 'T';
    packet[idx++] = 0x04;  // Protocol level 4 (MQTT 3.1.1)
    packet[idx++] = 0x02;  // Connect flags: clean session
    packet[idx++] = 0x00;
    packet[idx++] = 60;    // Keep alive (seconds)
    
    packet[idx++] = len >> 8;
    packet[idx++] = len & 0xFF;
    
    memcpy(&packet[idx], MQTT_CLIENT_ID, len);
    idx += len;
    
    return wifi_command_TCP_transmit(packet, idx) == WIFI_OK;
}

// MQTT: Send PUBLISH packet for telemetry
static bool mqtt_publish_telemetry(const char *topic, const char *payload) {
    uint8_t packet[512];
    uint8_t idx = 0;
    
    uint16_t topic_len = strlen(topic);
    uint16_t payload_len = strlen(payload);
    uint16_t remaining = 2 + topic_len + payload_len;  // topic_len_bytes + topic + payload
    
    uint8_t rem_len_bytes[4];
    uint8_t rem_size = mqtt_encode_remaining_length(remaining, rem_len_bytes);
    
    packet[idx++] = 0x30;  // PUBLISH QoS 0
    for(uint8_t i = 0; i < rem_size; i++) packet[idx++] = rem_len_bytes[i];
    
    // Topic string
    packet[idx++] = topic_len >> 8;
    packet[idx++] = topic_len & 0xFF;
    memcpy(&packet[idx], topic, topic_len);
    idx += topic_len;
    
    // Payload
    memcpy(&packet[idx], payload, payload_len);
    idx += payload_len;
    
    bool result = wifi_command_TCP_transmit(packet, idx) == WIFI_OK;
    return result;
}

// MQTT: Send SUBSCRIBE packet for commands
static bool mqtt_subscribe_commands(void) {
    uint8_t packet[128];
    uint8_t idx = 0;
    
    char topic[32];
    snprintf(topic, sizeof(topic), "farm/%u/cmd", setup_id);
    uint16_t topic_len = strlen(topic);
    
    uint16_t remaining = 2 + 2 + topic_len + 1;  // packet_id + topic_len + topic + QoS
    
    uint8_t rem_len_bytes[4];
    uint8_t rem_size = mqtt_encode_remaining_length(remaining, rem_len_bytes);
    
    packet[idx++] = 0x82;  // SUBSCRIBE with QoS 1 (10000010)
    for(uint8_t i = 0; i < rem_size; i++) packet[idx++] = rem_len_bytes[i];
    
    // Packet identifier
    packet[idx++] = 0x00;
    packet[idx++] = 0x01;
    
    // Topic string
    packet[idx++] = topic_len >> 8;
    packet[idx++] = topic_len & 0xFF;
    memcpy(&packet[idx], topic, topic_len);
    idx += topic_len;
    
    packet[idx++] = 0x01;  // QoS 1
    
    return wifi_command_TCP_transmit(packet, idx) == WIFI_OK;
}

// Read all sensors
static void read_sensors(uint8_t *temp_int, uint8_t *temp_dec,
                        uint8_t *hum_int, uint8_t *hum_dec,
                        uint16_t *light_value, uint16_t *soil_value) {
    dht11_get(hum_int, hum_dec, temp_int, temp_dec);
    *light_value = light_measure_raw();
    *soil_value = soil_measure_raw(ADC_PK0);
    
    display_setDecimals(1);
    display_int((*temp_int) * 10 + (*temp_dec));
}

// Display sensor values to console every 5 seconds
static void display_sensor_values(void) {
    uint8_t temp_int, temp_dec, hum_int, hum_dec;
    uint16_t light_value, soil_value;
    
    read_sensors(&temp_int, &temp_dec, &hum_int, &hum_dec, &light_value, &soil_value);
}

// Build telemetry payload JSON
static void build_telemetry_payload(char *payload, size_t size,
                                    uint8_t temp_int, uint8_t temp_dec,
                                    uint8_t hum_int, uint8_t hum_dec,
                                    uint16_t light_value, uint16_t soil_value) {
    uint16_t temperature_x10 = temp_int * 10 + temp_dec;
    uint16_t humidity_x10 = hum_int * 10 + hum_dec;
    
    snprintf(payload, size,
             "{\"setup_id\":%u,\"sensor_id\":%u,\"temperature\":%u,\"humidity\":%u,\"light\":%u,\"soil_moisture\":%u}",
             setup_id, SENSOR_ID, temperature_x10, humidity_x10, light_value, soil_value);
}

// Build status/heartbeat payload JSON
static void build_status_payload(char *payload, size_t size) {
    snprintf(payload, size,
             "{\"setup_id\":%u,\"status\":\"online\"}",
             setup_id);
}

// Send telemetry data
static void send_telemetry(void) {
    uint8_t temp_int, temp_dec, hum_int, hum_dec;
    uint16_t light_value, soil_value;
    char payload[256];
    char topic[32];

    // Allow DHT11 time to settle before reading (it was just read 100ms ago in display)
    _delay_ms(500);
    
    read_sensors(&temp_int, &temp_dec, &hum_int, &hum_dec, &light_value, &soil_value);
    build_telemetry_payload(payload, sizeof(payload), temp_int, temp_dec, hum_int, hum_dec, light_value, soil_value);

    snprintf(topic, sizeof(topic), "farm/%u/inf", setup_id);

    if (mqtt_connected) {
        _delay_ms(500);  // Stabilize connection before send
        if (mqtt_publish_telemetry(topic, payload)) {
            printf("Telemetry published: %s\n", payload);
        } else {
            printf("Failed to publish telemetry\n");
            mqtt_connected = false;
        }
        _delay_ms(1000);  // Longer delay after telemetry
    } else {
        printf("MQTT not connected, buffering: %s\n", payload);
    }
}

// Send heartbeat/status
static void send_heartbeat(void) {
    char payload[64];
    char topic[32];
    
    build_status_payload(payload, sizeof(payload));
    snprintf(topic, sizeof(topic), "farm/%u/status", setup_id);
    
    if (mqtt_connected) {
        _delay_ms(500);  // Stabilize connection before send
        
        // Try to publish with 2 retries
        uint8_t retry_count = 0;
        while (retry_count < 2) {
            if (mqtt_publish_telemetry(topic, payload)) {
                _delay_ms(500);
                return;
            }
            retry_count++;
            _delay_ms(300);  // Delay between retries
        }
        
        mqtt_connected = false;
        _delay_ms(500);
    }
}

// MQTT Connection
static bool mqtt_connect(void) {
    mqtt_rx_buffer[0] = '\0';

    if (wifi_command_create_TCP_connection(MQTT_BROKER_IP, MQTT_BROKER_PORT, NULL, mqtt_rx_buffer) != WIFI_OK) {
        printf("Failed to connect to MQTT broker\n");
        return false;
    }
    
    _delay_ms(1000);  // Wait for connection to fully establish
    
    if (!mqtt_send_connect_packet()) {
        printf("Failed to send MQTT CONNECT\n");
        wifi_command_close_TCP_connection();
        return false;
    }
    
    _delay_ms(1000);  // Wait for CONNECT response
    
    if (!mqtt_subscribe_commands()) {
        printf("Failed to subscribe to commands\n");
    }
    
    _delay_ms(500);  // Wait for SUBSCRIBE response
    
    printf("MQTT connected\n");
    return true;
}

int main(void) {
    display_init();
    light_init();
    soil_init(ADC_PK0);
    wpump_configure();
    wifi_init();
    
    if (UART_OK != uart_stdio_init(115200)) {
        led_on(4);
        while (1);
    }
    
    sei();  // Enable global interrupts
    
    printf("\n=== Urban MicroFarm IoT Device ===\n");
    printf("Firmware v1.0\n");
    
    // WiFi Setup
    _delay_ms(4000);
    printf("Configuring WiFi module...\n");
    
    if (wifi_command_disable_echo() != WIFI_OK) {
        printf("Failed to disable echo\n");
    }
    if (wifi_command_set_mode_to_1() != WIFI_OK) {
        printf("Failed to set mode\n");
    }
    if (wifi_command_set_to_single_Connection() != WIFI_OK) {
        printf("Failed to set single connection\n");
    }
    
    // Connect to AP
    printf("Connecting to WiFi...\n");
    if (wifi_command_join_AP("3Bredband-CB45", "t+hPgqG^ma") != WIFI_OK) {
        printf("Failed to join AP\n");
        led_on(4);
        while (1);
    }
    
    printf("WiFi connected: yes\n");
    _delay_ms(2000);

    if (!connections_commands_wait_for_station_ip()) {
        led_on(4);
        while (1);
    }

    connections_commands_log_connection_status();
    
    // Connect to MQTT broker
    _delay_ms(2000);
    if (!mqtt_command_mqtt_connect()) {
        printf("MQTT connection failed\n");
        led_on(4);
        while (1);
    }
    
    mqtt_connected = true;
    printf("MQTT connected: %s\n", mqtt_connected ? "yes" : "no");
    printf("Broker IP: %s\n", MQTT_BROKER_IP);
    printf("Device initialized and ready\n");
    
    // Main loop
    while (1) {
        mqtt_command_poll_mqtt_incoming();
        if (mqtt_command_received) {
            mqtt_command_received = false;
        }
        
        // Increment counters (each iteration is ~100ms)
        sensor_display_counter++;
        heartbeat_counter++;
        telemetry_counter++;
        
        // Display sensor values every 5 seconds (50 x 100ms)
        if (sensor_display_counter >= 50) {
            commands_display_sensor_values();
            sensor_display_counter = 0;
        }
        
        // Send telemetry every 15 seconds (150 x 100ms)
        if (telemetry_counter >= 150) {
            commands_send_telemetry();
            telemetry_counter = 0;
        }
        
        // Send heartbeat every 30 seconds (300 x 100ms)
        if (heartbeat_counter >= 300) {
            commands_send_heartbeat();
            heartbeat_counter = 0;
        }
        
        // Reconnect if MQTT disconnected
        if (!mqtt_connected) {
            _delay_ms(5000);
            if (mqtt_command_mqtt_connect()) {
                mqtt_connected = true;
            }
        }
        
        _delay_ms(100);
    }
    
    return 0;
}