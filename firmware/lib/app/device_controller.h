#pragma once
#include <stdint.h>
#include <stddef.h>
#include "device_context.h"
#include "interfaces.h"

// Reads temperature, humidity, light and soil moisture from their respective
// hardware drivers into the provided output pointers. All six must be non-NULL.
// Used as the real implementation of sensor_interface_t.read in main.
void device_read_sensors(uint8_t *temp_int, uint8_t *temp_dec,
                         uint8_t *hum_int,  uint8_t *hum_dec,
                         uint16_t *light_value, uint16_t *soil_value);

// Reads all sensors via the injected sensor interface, updates the 7-segment
// display and prints values to the logger. Called periodically from the main loop.
void device_display_sensor_values(const sensor_interface_t *sensors,
                                  const logger_interface_t *logger);

// Serialises all sensor readings into a JSON telemetry payload.
// Temperature and humidity are scaled x10 (23.5 C stored as 235).
// Output: {"setup_id":...,"sensor_id":...,"temperature":...,"humidity":...,"light":...,"soil_moisture":...}
void device_build_telemetry_payload(char *payload, size_t size,
                                    uint8_t temp_int, uint8_t temp_dec,
                                    uint8_t hum_int,  uint8_t hum_dec,
                                    uint16_t light_value, uint16_t soil_value,
                                    uint16_t setup_id);

// Serialises a heartbeat payload.
// Output: {"setup_id":...,"status":"online"}
void device_build_heartbeat_payload(char *payload, size_t size, uint16_t setup_id);

// Reads all sensors, builds the JSON payload and publishes it via the injected
// MQTT interface. Sets ctx->mqtt_connected to false on publish failure.
void device_send_telemetry(device_context_t *ctx,
                           const mqtt_interface_t *mqtt,
                           const sensor_interface_t *sensors,
                           const logger_interface_t *logger);

// Builds a heartbeat payload and publishes it via the injected MQTT interface.
// Retries once before marking the connection as lost.
void device_send_heartbeat(device_context_t *ctx,
                           const mqtt_interface_t *mqtt,
                           const logger_interface_t *logger);

// Parses an inbound command payload and dispatches the actuator action
// via the injected pump interface.
// Supports: {"actuator":"water_pump","amount_ml":<n>}
void device_handle_command(const char *payload,
                           const pump_interface_t *pump,
                           const logger_interface_t *logger);