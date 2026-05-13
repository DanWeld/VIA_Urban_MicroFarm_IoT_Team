#pragma once
#include <stdint.h>


//activate water pump from mqtt
void commands_handle_backend_command(const char *payload);
// Read all sensors
void commands_read_sensors(uint8_t *temp_int, uint8_t *temp_dec,
                uint8_t *hum_int, uint8_t *hum_dec,
                uint16_t *light_value, uint16_t *soil_value);

// Display sensor values to console every 5 seconds
void commands_display_sensor_values(void);

// Build telemetry payload JSON
void commands_build_telemetry_payload(char *payload, size_t size,
                            uint8_t temp_int, uint8_t temp_dec,
                            uint8_t hum_int, uint8_t hum_dec,
                            uint16_t light_value, uint16_t soil_value);


// Build status/heartbeat payload JSON
void commands_build_status_payload(char *payload, size_t size);

// Send telemetry data
void commands_send_telemetry(void);

// Send heartbeat/status
void commands_send_heartbeat(void);