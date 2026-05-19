#pragma once
#include <stdint.h>
#include <stddef.h>

// Reads temperature, humidity, light and soil moisture from their
// respective sensors into the provided output pointers.
// All six pointers must be non-NULL.
// Also updates the 7-segment display with the current temperature.
void device_read_sensors(uint8_t *temp_int, uint8_t *temp_dec,
                         uint8_t *hum_int,  uint8_t *hum_dec,
                         uint16_t *light_value, uint16_t *soil_value);

// Reads all sensors and prints their current values to the console.
// Called periodically from the main loop for local monitoring.
void device_display_sensor_values(void);

// Serialises all sensor readings into a JSON telemetry payload.
// Temperature and humidity are scaled x10 to preserve one decimal place
// without using floating point (e.g. 23.5 C is stored as 235).
// The backend must divide by 10 to recover the real value.
// Output format:
// {"setup_id":…,"sensor_id":…,"temperature":…,"humidity":…,"light":…,"soil_moisture":…}
void device_build_telemetry_payload(char *payload, size_t size,
                                    uint8_t temp_int, uint8_t temp_dec,
                                    uint8_t hum_int,  uint8_t hum_dec,
                                    uint16_t light_value, uint16_t soil_value);

// Serialises a heartbeat payload indicating the device is online.
// Output format: {"setup_id":…,"status":"online"}
void device_build_heartbeat_payload(char *payload, size_t size);

// Reads all sensors, builds the telemetry JSON and publishes it
// to "farm/<setup_id>/inf". Sets mqtt_connected to false if the
// publish fails so the main loop knows to reconnect.
void device_send_telemetry(void);

// Builds a heartbeat payload and publishes it to "farm/<setup_id>/status".
// Retries once on failure before marking the connection as lost.
void device_send_heartbeat(void);

// Parses an inbound MQTT command payload and executes the requested
// actuator action. Currently supports:
// {"actuator":"water_pump","amount_ml":<n>}
void device_handle_command(const char *payload);