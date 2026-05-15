#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "led.h"
#include "wifi.h"
#include "connections_commands.h"

bool connections_commands_wait_for_station_ip(void) {
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
void connections_commands_log_connection_status(void) {
    char status_buffer[160] = {0};

    if (wifi_command_get_connection_status(status_buffer, sizeof(status_buffer)) == WIFI_OK) {
        printf("ESP8266 connection status: %s\n", status_buffer);
    } else {
        printf("ESP8266 connection status query failed\n");
    }
}

void create_connections(){
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
}