#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "wifi.h"
#include "dht11.h"
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