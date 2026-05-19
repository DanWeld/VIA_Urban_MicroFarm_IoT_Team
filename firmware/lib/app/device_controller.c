#include "device_controller.h"
#include "wpump_controller.h"
#include "mqtt_client.h"
#include "dht11.h"
#include "light.h"
#include "soil.h"
#include "display.h"
#include "adc.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <util/delay.h>
#include <stdbool.h>

// Shared with main.c – used to gate publish calls and signal reconnection.
extern bool     mqtt_connected;
extern uint16_t setup_id;

// Fixed sensor identifier included in every telemetry message so the backend
// can distinguish multiple sensors on the same setup.
#define SENSOR_ID 1

void device_handle_command(const char *payload)
{
    // Only the water pump actuator is supported currently.
    if (strstr(payload, "\"actuator\":\"water_pump\"") == NULL) return;

    // Extract the requested volume. atoi stops at the first non-digit, so
    // "amount_ml\":500}" correctly yields 500.
    uint32_t amount_ml = 0;
    const char *amount_ptr = strstr(payload, "\"amount_ml\":");
    if (amount_ptr != NULL) {
        amount_ptr += strlen("\"amount_ml\":");
        amount_ml = (uint32_t)atoi(amount_ptr);
    }

    printf("Command: water_pump %lu ml\n", (unsigned long)amount_ml);

    // Dispense the requested volume. The controller validates the range,
    // converts ml to a pump-on duration, and schedules an automatic stop.
    wpump_controller_dispense(amount_ml);

    printf("Water pump command dispatched\n");
}

void device_read_sensors(uint8_t *temp_int, uint8_t *temp_dec,
                         uint8_t *hum_int,  uint8_t *hum_dec,
                         uint16_t *light_value, uint16_t *soil_value)
{
    dht11_get(hum_int, hum_dec, temp_int, temp_dec);
    *light_value = light_measure_raw();
    *soil_value  = soil_measure_raw(ADC_PK0);

    // Mirror temperature on the physical display so engineers can read it
    // without a serial monitor connected. Decimal position 1 shows e.g. "23.5".
    display_setDecimals(1);
    display_int((*temp_int) * 10 + (*temp_dec));
}

void device_display_sensor_values(void)
{
    uint8_t  temp_int, temp_dec, hum_int, hum_dec;
    uint16_t light_value, soil_value;

    device_read_sensors(&temp_int, &temp_dec, &hum_int, &hum_dec,
                        &light_value, &soil_value);

    printf("Temp: %d.%d C  Hum: %d.%d%%  Light: %u  Soil: %u\n",
           temp_int, temp_dec, hum_int, hum_dec, light_value, soil_value);
}

void device_build_telemetry_payload(char *payload, size_t size,
                                    uint8_t temp_int, uint8_t temp_dec,
                                    uint8_t hum_int,  uint8_t hum_dec,
                                    uint16_t light_value, uint16_t soil_value)
{
    // Scale ×10 to preserve one decimal place in integer arithmetic.
    // The backend must divide by 10 to recover the real value.
    uint16_t temperature_x10 = temp_int * 10 + temp_dec;
    uint16_t humidity_x10    = hum_int  * 10 + hum_dec;

    snprintf(payload, size,
             "{\"setup_id\":%u,\"sensor_id\":%u,"
             "\"temperature\":%u,\"humidity\":%u,"
             "\"light\":%u,\"soil_moisture\":%u}",
             setup_id, SENSOR_ID,
             temperature_x10, humidity_x10,
             light_value, soil_value);
}

void device_build_heartbeat_payload(char *payload, size_t size)
{
    snprintf(payload, size,
             "{\"setup_id\":%u,\"status\":\"online\"}",
             setup_id);
}

void device_send_telemetry(void)
{
    uint8_t  temp_int, temp_dec, hum_int, hum_dec;
    uint16_t light_value, soil_value;
    char     payload[256];
    char     topic[32];

    // The DHT11 needs at least 2 seconds between reads. A 500 ms extra delay
    // here avoids collisions when display_sensor_values was called shortly before.
    _delay_ms(500);

    device_read_sensors(&temp_int, &temp_dec, &hum_int, &hum_dec,
                        &light_value, &soil_value);
    device_build_telemetry_payload(payload, sizeof(payload),
                                   temp_int, temp_dec,
                                   hum_int,  hum_dec,
                                   light_value, soil_value);

    snprintf(topic, sizeof(topic), "farm/%u/inf", setup_id);

    if (!mqtt_connected) {
        printf("MQTT not connected - dropping telemetry: %s\n", payload);
        return;
    }

    _delay_ms(500); // brief pause to let the TCP socket settle before transmit

    if (mqtt_publish(topic, payload)) {
        printf("Telemetry: %s\n", payload);
    } else {
        printf("Telemetry publish failed\n");
        mqtt_connected = false; // signal main loop to reconnect
    }

    _delay_ms(1000); // post-transmit pause to avoid flooding the broker
}

void device_send_heartbeat(void)
{
    char payload[64];
    char topic[32];

    device_build_heartbeat_payload(payload, sizeof(payload));
    snprintf(topic, sizeof(topic), "farm/%u/status", setup_id);

    if (!mqtt_connected) return;

    _delay_ms(500);

    // Retry once: a single transient failure should not drop the connection,
    // but two consecutive failures indicate a real problem.
    for (uint8_t attempt = 0; attempt < 2; attempt++) {
        if (mqtt_publish(topic, payload)) {
            _delay_ms(500);
            return;
        }
        _delay_ms(300);
    }

    printf("Heartbeat failed - marking MQTT disconnected\n");
    mqtt_connected = false;
}