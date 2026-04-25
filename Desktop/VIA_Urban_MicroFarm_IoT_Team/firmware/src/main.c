/*****************************************************************************
 * main.c
 *  Main application file for the IoT hardware drivers demo.
 *  This file initializes all the hardware drivers and demonstrates their
 *  functionality.
 *  Push button 2 on the shield during reset to enter continious sensor
 *  reading mode. Otherwise the program will run an interactive demo that
 *  allows you to test each driver individually by sending commands over UART.
 *  See interactive.c for details.
 * 
 *****************************************************************************/
#ifndef WINDOWS_TEST
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#endif
#include <stdio.h>
#include "interactive.h"
#include "button.h"
#include "uart_stdio.h"
#include "led.h"
#include "pir.h"
#include "display.h"
#include "wifi.h"
#include "buzzer.h"
#include "dht11.h"
#include "proximity.h"
#include "servo.h"
#include "adc.h"
#include "light.h"
#include "soil.h"
#include "tone.h"
#include "timer.h"

//#include "adxl345.h"

uint8_t humidity_integer, humidity_decimal, temperature_integer, temperature_decimal;
static int8_t _led_no = 0;
//static int16_t _x, _y, _z;

// Reads all sensor values and updates the 7-segment display
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

// Builds the telemetry payload in the agreed JSON contract format
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

// Sends the payload through UART for now and can later be replaced by Wi-Fi/MQTT sending
void send_payload(const char *payload)
{
    printf("%s\n", payload); // For now, just print to UART. Replace with Wi-Fi/MQTT sending later.
}

void timer_callback(uint8_t id)
{
    led_toggle((_led_no%4) + 1); // Toggle LEDs in sequence 1-4
    _led_no++;
}

int main(void)
{
    uint16_t setup_id = 1;
    uint8_t temp_int, temp_dec, hum_int, hum_dec;
    uint16_t light_value, soil_value;
    char payload[150];

    // Initialize UART first for debug output
    if (UART_OK != uart_stdio_init(115200))
    {
        led_on(4); // Turn on LED4 to indicate error
        while (1)
            ;
    }
    sei(); // Enable global interrupts
    printf("VIA UNIVERSITY COLLEGE SEP4 IoT Hardware DRIVERS DEMO\n");

    // Initialize all hardware
    led_init();
    button_init();
    display_init();
    proximity_init();
    light_init();
    soil_init(ADC_PK0);
    pir_init(pir_callback);
    wifi_init();

    // WiFi module needs time to initialize (typically 2-4 seconds)
    printf("Initializing WiFi module...\n");
    _delay_ms(4000);

    // Verify WiFi module is responsive
    printf("Testing WiFi module communication...\n");
    if (wifi_command_AT() == WIFI_OK) {
        printf("WiFi module is responsive.\n");
    } else {
        printf("Warning: WiFi module not responding to AT command.\n");
    }

    // Disable echo for cleaner responses
    printf("Disabling WiFi echo...\n");
    if (wifi_command_disable_echo() == WIFI_OK) {
        printf("Echo disabled.\n");
    } else {
        printf("Warning: Failed to disable echo.\n");
    }

    // Set to station mode (mode 1)
    printf("Setting WiFi to station mode...\n");
    if (wifi_command_set_mode_to_1() == WIFI_OK) {
        printf("WiFi mode set to station.\n");
    } else {
        printf("Failed to set WiFi mode.\n");
    }

    // Set to single connection mode (recommended)
    printf("Setting WiFi to single connection mode...\n");
    if (wifi_command_set_to_single_Connection() == WIFI_OK) {
        printf("WiFi set to single connection mode.\n");
    } else {
        printf("Warning: Failed to set single connection mode.\n");
    }

    // Connect to WiFi hotspot
    printf("Connecting to WiFi hotspot 'iPhone'...\n");
    WIFI_ERROR_MESSAGE_t join_result = wifi_command_join_AP("iPhone", "Mita1234");
    if (join_result == WIFI_OK) {
        printf("Connected to WiFi hotspot 'iPhone'.\n");
    } else {
        printf("Failed to connect to WiFi hotspot 'iPhone'. Error code: %d\n", join_result);
    }

    servo_init(PWM_NORMAL);

    // Check if button 2 is pressed during startup for interactive demo
    if(!button_get(2))
    {
        interactive_demo();
    }

    timer_create_sw(timer_callback, 1000); // Create a timer that toggles an LED every 1 second
    tone_play_starwars();

    // Test servo by sweeping from -90 to +90 degrees and back
    servo_start();
    for(int i=-90; i<=90; i+=10)
    {
        servo_setAngle(PWM_A, (int8_t)i);
        printf("Servo set to %d degrees.\n", i);
        _delay_ms(100);
    }
    servo_stop();

    // Test WiFi by sending AT command and printing response
    if(WIFI_OK == wifi_command_AT())
    {
        printf("WiFi module responded to AT command.\n");
    }
    else
    {
        printf("WiFi module did not respond to AT command.\n");
    }

    buzzer_beep();

    // Continuous sensor readings
    while (1)
    {
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

    return 0;
}
