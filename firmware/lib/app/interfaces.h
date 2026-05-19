#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "device_context.h"

// Abstraction over the sensor hardware. The real implementation reads from
// dht11/light/soil drivers. A mock implementation returns fixed test values.
typedef struct {
    void (*read)(uint8_t *temp_int, uint8_t *temp_dec,
                 uint8_t *hum_int,  uint8_t *hum_dec,
                 uint16_t *light,   uint16_t *soil);
} sensor_interface_t;

// Abstraction over the water pump actuator. The real implementation drives
// the relay via wpump_controller. A mock records calls without moving water.
typedef struct {
    void (*dispense)(uint32_t amount_ml);
} pump_interface_t;

// Abstraction over MQTT transport. The real implementation uses wifi/TCP.
// A mock can record published messages and simulate incoming commands.
typedef struct {
    bool (*connect)(device_context_t *ctx);
    bool (*subscribe)(const char *topic);
    bool (*publish)(const char *topic, const char *payload);
    void (*poll)(device_context_t *ctx);
} mqtt_interface_t;

// Abstraction over log output. The real implementation calls printf.
// A mock can silence output during tests or record messages for assertion.
typedef struct {
    void (*write)(const char *msg);
} logger_interface_t;