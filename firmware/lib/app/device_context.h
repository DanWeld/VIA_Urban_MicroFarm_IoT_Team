#pragma once
#include <stdbool.h>
#include <stdint.h>

// Size of the buffer used to receive raw bytes from the MQTT broker over TCP.
// Must be large enough to hold a complete incoming PUBLISH packet.
#define MQTT_RX_BUFFER_SIZE 256

// Shared application state passed by pointer to every module that needs it.
// Centralising this state eliminates extern globals and makes dependencies
// explicit: if a function needs mqtt_connected, it receives a context pointer.
typedef struct {
    bool     mqtt_connected;        // true while the broker TCP connection is active
    bool     mqtt_command_received; // set by mqtt_poll_incoming when a cmd message arrives
    uint16_t setup_id;              // identifies this physical device to the backend
    char     mqtt_rx_buffer[MQTT_RX_BUFFER_SIZE]; // raw receive buffer written by the WiFi driver
} device_context_t;