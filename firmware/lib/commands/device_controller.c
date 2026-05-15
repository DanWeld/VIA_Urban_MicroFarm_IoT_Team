#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdbool.h>

#include "device_controller.h"
#include "dht11.h"
#include "light.h"
#include "soil.h"
#include "display.h"
#include "mqtt_client.h"
// please uncomment #include "wpump_controller.h" when add water pump converter branch is merged
// it include  wpump_controller
//#include "wpump_controller.h"

#define SENSOR_ID                 1

extern bool mqtt_connected;
extern uint16_t setup_id;

void commands_handle_backend_command(const char *payload) {
    if (strstr(payload, "\"actuator\":\"water_pump\"") != NULL) {
        const char *amount_ptr = strstr(payload, "\"amount_ml\":");
        uint16_t amount_ml = 0;

        if (amount_ptr != NULL) {
            amount_ptr += strlen("\"amount_ml\":");
            amount_ml = (uint16_t)atoi(amount_ptr);
        }

        printf("MQTT command received: water_pump %u ml\n", amount_ml);
        // same thing here un comment when water pump converter branch is merged
        //wpump_controller_dispense(amount_ml);
        printf("Water pump command completed\n");
    }
}


// Read all sensors
void commands_read_sensors(uint8_t *temp_int, uint8_t *temp_dec,
                        uint8_t *hum_int, uint8_t *hum_dec,
                        uint16_t *light_value, uint16_t *soil_value) {
    dht11_get(hum_int, hum_dec, temp_int, temp_dec);
    *light_value = light_measure_raw();
    *soil_value = soil_measure_raw(ADC_PK0);
    
    display_setDecimals(1);
    display_int((*temp_int) * 10 + (*temp_dec));
}

void commands_display_sensor_values(void) {
    uint8_t temp_int, temp_dec, hum_int, hum_dec;
    uint16_t light_value, soil_value;
    
    commands_read_sensors(&temp_int, &temp_dec, &hum_int, &hum_dec, &light_value, &soil_value);
}

void commands_build_telemetry_payload(char *payload, size_t size,
                                    uint8_t temp_int, uint8_t temp_dec,
                                    uint8_t hum_int, uint8_t hum_dec,
                                    uint16_t light_value, uint16_t soil_value) {
    uint16_t temperature_x10 = temp_int * 10 + temp_dec;
    uint16_t humidity_x10 = hum_int * 10 + hum_dec;
    
    snprintf(payload, size,
             "{\"setup_id\":%u,\"sensor_id\":%u,\"temperature\":%u,\"humidity\":%u,\"light\":%u,\"soil_moisture\":%u}",
             setup_id, SENSOR_ID, temperature_x10, humidity_x10, light_value, soil_value);
}

void commands_build_status_payload(char *payload, size_t size) {
    snprintf(payload, size,
             "{\"setup_id\":%u,\"status\":\"online\"}",
             setup_id);
}

void commands_send_telemetry(void) {
    uint8_t temp_int, temp_dec, hum_int, hum_dec;
    uint16_t light_value, soil_value;
    char payload[256];
    char topic[32];

    // Allow DHT11 time to settle before reading (it was just read 100ms ago in display)
    _delay_ms(500);
    
    commands_read_sensors(&temp_int, &temp_dec, &hum_int, &hum_dec, &light_value, &soil_value);
    commands_build_telemetry_payload(payload, sizeof(payload), temp_int, temp_dec, hum_int, hum_dec, light_value, soil_value);

    snprintf(topic, sizeof(topic), "farm/%u/inf", setup_id);

    if (mqtt_connected) {
        _delay_ms(500);  // Stabilize connection before send
        if (mqtt_command_mqtt_publish_telemetry(topic, payload)) {
            printf("Telemetry published: %s\n", payload);
        } else {
            printf("Failed to publish telemetry\n");
            mqtt_connected = false;
        }
        _delay_ms(1000);  // Longer delay after telemetry
    } else {
        printf("MQTT not connected, buffering: %s\n", payload);
    }
}


void commands_send_heartbeat(void) {
    char payload[64];
    char topic[32];
    
    commands_build_status_payload(payload, sizeof(payload));
    snprintf(topic, sizeof(topic), "farm/%u/status", setup_id);
    
    if (mqtt_connected) {
        _delay_ms(500);  // Stabilize connection before send
        
        // Try to publish with 2 retries
        uint8_t retry_count = 0;
        while (retry_count < 2) {
            if (mqtt_command_mqtt_publish_telemetry(topic, payload)) {
                _delay_ms(500);
                return;
            }
            retry_count++;
            _delay_ms(300);  // Delay between retries
        }
        
        mqtt_connected = false;
        _delay_ms(500);
    }
}