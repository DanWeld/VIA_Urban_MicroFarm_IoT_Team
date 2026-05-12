#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "mqtt_commands.h"

extern bool mqtt_connected;
extern char mqtt_rx_buffer[];
extern bool mqtt_command_received;
extern uint32_t millis_counter;

extern void poll_mqtt_incoming(void) {
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
extern uint32_t millis(void) {
    return millis_counter / 2;  // Assuming ~2ms per tick
}