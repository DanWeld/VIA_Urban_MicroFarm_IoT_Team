#pragma once
#include <stdint.h>
#include <stddef.h>
#include "device_context.h"

// Reads temperature, humidity, light and soil moisture from their respective
// sensors into the provided output pointers. All six pointers must be non-NULL.
// Does not touch the display or any other output — pure sensor acquisition.
void device_read_sensors(uint8_t *temp_int, uint8_t *temp_dec,
                         uint8_t *hum_int,  uint8_t *hum_dec,
                         uint16_t *light_value, uint16_t *soil_value);

// Reads all sensors, updates the 7-segment display with the current temperature,
// and prints all values to the serial console. Called periodically from the main
// loop for local monitoring without requiring a connected dashboard.
void device_display_sensor_values(void);

// Serialises all sensor readings into a JSON telemetry payload and writes the
// result into the caller-supplied buffer. Temperature and humidity are scaled
// ×10 to preserve one decimal place without floating point (23.5 °C → 235).
// The backend must divide by 10 to recover the real value.
// Output format:
// {"setup_id":...,"sensor_id":...,"temperature":...,"humidity":...,"light":...,"soil_moisture":...}
void device_build_telemetry_payload(char *payload, size_t size,
                                    uint8_t temp_int, uint8_t temp_dec,
                                    uint8_t hum_int,  uint8_t hum_dec,
                                    uint16_t light_value, uint16_t soil_value,
                                    uint16_t setup_id);

// Serialises a heartbeat payload indicating the device is online and writes it
// into the caller-supplied buffer.
// Output format: {"setup_id":...,"status":"online"}
void device_build_heartbeat_payload(char *payload, size_t size, uint16_t setup_id);

// Reads all sensors, builds the JSON telemetry payload and publishes it to
// "farm/<setup_id>/inf". Sets ctx->mqtt_connected to false if the publish fails
// so the main loop knows to trigger a reconnection attempt.
void device_send_telemetry(device_context_t *ctx);

// Builds a heartbeat payload and publishes it to "farm/<setup_id>/status".
// Retries once on transient failure before marking the connection as lost in ctx.
void device_send_heartbeat(device_context_t *ctx);

// Parses an inbound MQTT command payload and executes the requested actuator
// action. Currently supports water pump commands:
// {"actuator":"water_pump","amount_ml":<n>}
void device_handle_command(const char *payload);