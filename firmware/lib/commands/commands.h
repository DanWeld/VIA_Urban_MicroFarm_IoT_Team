#pragma once
#include <stdint.h>

//activate water pump from mqtt
void handle_backend_command(const char *payload);
// Read all sensors
void read_sensors(uint8_t *temp_int, uint8_t *temp_dec,
                uint8_t *hum_int, uint8_t *hum_dec,
                uint16_t *light_value, uint16_t *soil_value);

// Display sensor values to console every 5 seconds
void display_sensor_values(void);

// Build telemetry payload JSON
void build_telemetry_payload(char *payload, size_t size,
                            uint8_t temp_int, uint8_t temp_dec,
                            uint8_t hum_int, uint8_t hum_dec,
                            uint16_t light_value, uint16_t soil_value);


// Build status/heartbeat payload JSON
void build_status_payload(char *payload, size_t size);

// Send telemetry data
void send_telemetry(void);

// Send heartbeat/status
void send_heartbeat(void)