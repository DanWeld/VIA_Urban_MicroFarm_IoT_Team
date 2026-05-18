#include "wifi_connection.h"
#include "wifi.h"

#include <stdint.h>
#include <stdio.h>
#include <util/delay.h>

bool wifi_configure(const char *ssid, const char *password)
{
    // The ESP8266 needs up to 4 seconds after power-on before
    // it is ready to accept AT commands.
    _delay_ms(4000);
    printf("Configuring WiFi module...\n");

    // Disable command echo so the receive buffer only contains
    // responses, not the commands we sent.
    if (wifi_command_disable_echo() != WIFI_OK) {
        printf("Failed to disable echo\n");
        return false;
    }

    // Station mode (mode 1): the device connects to an existing network.
    if (wifi_command_set_mode_to_1() != WIFI_OK) {
        printf("Failed to set station mode\n");
        return false;
    }

    // Single-connection mode is required for MQTT – the module only
    // supports one TCP socket at a time when CIPMUX=0.
    if (wifi_command_set_to_single_Connection() != WIFI_OK) {
        printf("Failed to set single-connection mode\n");
        return false;
    }

    printf("Connecting to network '%s'...\n", ssid);
    if (wifi_command_join_AP(ssid, password) != WIFI_OK) {
        printf("Failed to join network '%s'\n", ssid);
        return false;
    }

    printf("WiFi connected\n");

    // Allow DHCP time to assign an IP before the caller queries it.
    _delay_ms(2000);
    return true;
}

bool wifi_wait_for_ip(void)
{
    char ip_buffer[128] = {0};

    printf("Waiting for IP address...\n");
    for (uint8_t attempt = 0; attempt < 20; attempt++) {
        if (wifi_command_get_station_ip(ip_buffer, sizeof(ip_buffer)) == WIFI_OK) {
            printf("IP acquired: %s\n", ip_buffer);
            return true;
        }
        _delay_ms(1000);
    }

    printf("IP not assigned after 20 seconds\n");
    return false;
}

void wifi_log_status(void)
{
    char status[160] = {0};

    if (wifi_command_get_connection_status(status, sizeof(status)) == WIFI_OK) {
        printf("WiFi status: %s\n", status);
    } else {
        printf("WiFi status query failed\n");
    }
}