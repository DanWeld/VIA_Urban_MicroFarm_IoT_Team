#include "device_controller.h"
#include "display.h"
#include "dht11.h"
#include "light.h"
#include "soil.h"
#include "adc.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <util/delay.h>
#include <stdbool.h>

// Fixed sensor identifier included in every telemetry message so the backend
// can distinguish multiple sensors belonging to the same physical setup.
#define SENSOR_ID 1

void device_handle_command(const char *payload,
                           const pump_interface_t *pump,
                           const logger_interface_t *logger)
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

    char msg[64];
    snprintf(msg, sizeof(msg), "Command: water_pump %lu ml", (unsigned long)amount_ml);
    logger->write(msg);

    // Dispatch the dispense command through the injected pump interface so
    // this function can be tested without activating real hardware.
    pump->dispense(amount_ml);

    logger->write("Water pump command dispatched");
}

void device_read_sensors(uint8_t *temp_int, uint8_t *temp_dec,
                         uint8_t *hum_int,  uint8_t *hum_dec,
                         uint16_t *light_value, uint16_t *soil_value)
{
    // DHT11 returns humidity first, then temperature.
    dht11_get(hum_int, hum_dec, temp_int, temp_dec);
    *light_value = light_measure_raw();
    *soil_value  = soil_measure_raw(ADC_PK0);
}

void device_display_sensor_values(const sensor_interface_t *sensors,
                                  const logger_interface_t *logger)
{
    uint8_t  temp_int, temp_dec, hum_int, hum_dec;
    uint16_t light_value, soil_value;

    sensors->read(&temp_int, &temp_dec, &hum_int, &hum_dec,
                  &light_value, &soil_value);

    // Mirror temperature on the physical display so engineers can read it
    // without a serial monitor connected. Decimal position 1 shows e.g. "23.5".
    display_setDecimals(1);
    display_int(temp_int * 10 + temp_dec);

    char msg[64];
    snprintf(msg, sizeof(msg), "Temp: %d.%d C  Hum: %d.%d%%  Light: %u  Soil: %u",
             temp_int, temp_dec, hum_int, hum_dec, light_value, soil_value);
    logger->write(msg);
}

void device_build_telemetry_payload(char *payload, size_t size,
                                    uint8_t temp_int, uint8_t temp_dec,
                                    uint8_t hum_int,  uint8_t hum_dec,
                                    uint16_t light_value, uint16_t soil_value,
                                    uint16_t setup_id)
{
    // Scale x10 to preserve one decimal place in integer arithmetic.
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

void device_build_heartbeat_payload(char *payload, size_t size, uint16_t setup_id)
{
    snprintf(payload, size,
             "{\"setup_id\":%u,\"status\":\"online\"}",
             setup_id);
}

void device_send_telemetry(device_context_t *ctx,
                           const mqtt_interface_t *mqtt,
                           const sensor_interface_t *sensors,
                           const logger_interface_t *logger)
{
    uint8_t  temp_int, temp_dec, hum_int, hum_dec;
    uint16_t light_value, soil_value;
    char     payload[256];
    char     topic[32];

    // The DHT11 needs at least 2 seconds between reads. A 500 ms extra margin
    // avoids collisions when display_sensor_values was called shortly before.
    _delay_ms(2500);

    sensors->read(&temp_int, &temp_dec, &hum_int, &hum_dec,
                  &light_value, &soil_value);
    device_build_telemetry_payload(payload, sizeof(payload),
                                   temp_int, temp_dec,
                                   hum_int,  hum_dec,
                                   light_value, soil_value,
                                   ctx->setup_id);

    snprintf(topic, sizeof(topic), "farm/%u/inf", ctx->setup_id);

    // Drop the reading silently if the broker connection is not active.
    // The main loop is responsible for reconnecting.
    if (!ctx->mqtt_connected) {
        char msg[128];
        snprintf(msg, sizeof(msg), "MQTT not connected - dropping telemetry: %s", payload);
        logger->write(msg);
        return;
    }

    _delay_ms(500); // brief pause to let the TCP socket settle before transmit

    if (mqtt->publish(topic, payload)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Telemetry: %s", payload);
        logger->write(msg);
    } else {
        logger->write("Telemetry publish failed");
        // Signal the main loop to reconnect on the next iteration.
        ctx->mqtt_connected = false;
    }

    _delay_ms(1000); // post-transmit pause to avoid flooding the broker
}

void device_send_heartbeat(device_context_t *ctx,
                           const mqtt_interface_t *mqtt,
                           const logger_interface_t *logger)
{
    char payload[64];
    char topic[32];

    device_build_heartbeat_payload(payload, sizeof(payload), ctx->setup_id);
    snprintf(topic, sizeof(topic), "farm/%u/status", ctx->setup_id);

    if (!ctx->mqtt_connected) return;

    _delay_ms(500);

    // Retry once: a single transient failure should not drop the connection,
    // but two consecutive failures indicate a real problem.
    for (uint8_t attempt = 0; attempt < 2; attempt++) {
        if (mqtt->publish(topic, payload)) {
            _delay_ms(500);
            return;
        }
        _delay_ms(300);
    }

    logger->write("Heartbeat failed - marking MQTT disconnected");
    ctx->mqtt_connected = false;
}