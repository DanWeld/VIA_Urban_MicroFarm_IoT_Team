/*****************************************************************************
 *  Main application file for the IoT hardware drivers.
 *  This file initializes all the hardware drivers and demonstrates their functionality.
 *****************************************************************************/
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include "uart_stdio.h"
#include "led.h"
#include "display.h"
#include "wifi.h"
#include "dht11.h"
#include "adc.h"
#include "light.h"
#include "soil.h"
#include "wpump.h"

// Reads all sensor values and updates the 7-segment display. read_sensors() = get data
void read_sensors(uint8_t *temp_int, uint8_t *temp_dec,
                  uint8_t *hum_int, uint8_t *hum_dec,
                  uint16_t *light_value, uint16_t *soil_value)
{
    dht11_get(hum_int, hum_dec, temp_int, temp_dec);
    *light_value = light_measure_raw();
    *soil_value = soil_measure_raw(ADC_PK0);

    display_setDecimals(1);
    display_int((*temp_int) * 10 + (*temp_dec));
}

// Builds the telemetry payload in the agreed JSON contract format. build_payload() = package data
void build_payload(char *payload, size_t size,
                   uint16_t setup_id,
                   uint8_t temp_int, uint8_t temp_dec,
                   uint8_t hum_int, uint8_t hum_dec,
                   uint16_t light_value, uint16_t soil_value)
{
    uint16_t temperature_x10 = temp_int * 10 + temp_dec;
    uint16_t humidity_x10 = hum_int * 10 + hum_dec;

    snprintf(payload, size,
             "{\"setup_id\":%u,\"sensor_id\":null,\"temperature\":%u,\"humidity\":%u,\"light\":%u,\"soil_moisture\":%u}",
             setup_id, temperature_x10, humidity_x10, light_value, soil_value);
}

// Sends the payload through UART for now and can later be replaced by Wi-Fi/MQTT sending. send_payload() = publish data to MQTT broker
void send_payload(const char *payload)
{
    printf("%s\n", payload); // For now, just print to UART. Replace with Wi-Fi/MQTT sending later.
}

int main(void)
{
    uint16_t setup_id = 1;

    display_init();
    light_init();
    soil_init(ADC_PK0);
    wifi_init();

    if (UART_OK != uart_stdio_init(115200))
    {
        led_on(4); // Turn on LED4 to indicate error
        while (1)
            ;
    }
    sei(); // Enable global interrupts

    // Continuous sensor readings
    while (1)
    {
        // Read all sensors, update display, and print payload to UART every 2 seconds
        uint8_t temp_int, temp_dec, hum_int, hum_dec;
        uint16_t light_value, soil_value;
        char payload[150];

        // Read sensors and update display with temperature
        read_sensors(&temp_int, &temp_dec,
                     &hum_int, &hum_dec,
                     &light_value, &soil_value);

        // Build payload string and print to UART/Wi-Fi/MQTT
        build_payload(payload, sizeof(payload),
                      setup_id,
                      temp_int, temp_dec,
                      hum_int, hum_dec,
                      light_value, soil_value);

        send_payload(payload);

        _delay_ms(2000);
    }
}