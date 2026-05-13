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
//#define SENSOR_ID                 1
#define TELEMETRY_INTERVAL_SEC    15
#define HEARTBEAT_INTERVAL_SEC    30
#define SENSOR_DISPLAY_INTERVAL   5

// Runtime state
 uint16_t setup_id = SETUP_ID;            // Fixed device setup ID for MQTT topics
 bool mqtt_connected = false;
 char mqtt_rx_buffer[256] = {0};
 uint16_t telemetry_counter = 0;         // Counts 100ms intervals (600 = 60 sec)
 uint16_t heartbeat_counter = 0;         // Counts 100ms intervals (300 = 30 sec)
 uint16_t sensor_display_counter = 0;    // Counts 100ms intervals (50 = 5 sec)
 uint32_t millis_counter = 0;
 bool mqtt_command_received = false;


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