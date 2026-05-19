#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdbool.h>

#include "uart_stdio.h"
#include "led.h"
#include "system_init.h"
#include "wifi_connection.h"
#include "mqtt_client.h"
#include "device_controller.h"

// ── WiFi credentials ──────────────────────────────────────────────────────────
// Defined here so they are never buried inside a library module.
// Change these to match the local network before flashing.
#define WIFI_SSID     "iottest"
#define WIFI_PASSWORD "ArduinoTest"

// ── Timing configuration ──────────────────────────────────────────────────────
// All intervals are expressed in loop iterations. Each iteration sleeps 100 ms,
// so multiply seconds × 10 to get the iteration count.
#define TELEMETRY_INTERVAL  150  // 15 seconds between sensor publishes
#define HEARTBEAT_INTERVAL  300  // 30 seconds between status publishes
#define DISPLAY_INTERVAL     50  //  5 seconds between console sensor prints

// ── Shared state ──────────────────────────────────────────────────────────────
// Declared here and referenced via extern by mqtt_client and device_controller.
// A single global bool is acceptable in a single-threaded embedded system where
// the connection state must be visible across modules.
bool     mqtt_connected        = false;
char     mqtt_rx_buffer[256]   = {0};
bool     mqtt_command_received = false;
uint16_t setup_id              = 1;  // identifies this physical device to the backend

int main(void)
{
    system_init(); // initialise peripherals (display, sensors, pump, WiFi UART)

    // UART0 doubles as stdin/stdout so printf reaches the PC terminal.
    // Halt with LED4 on if this fails – nothing else can work without serial.
    if (uart_stdio_init(115200) != UART_OK) {
        led_on(4);
        while (1);
    }

    sei(); // enable global interrupts (required by UART RX ISRs and Timer ISRs)

    // Connect to the WiFi access point. Credentials are defined above so they
    // are never hardcoded inside a library module.
    if (!wifi_configure(WIFI_SSID, WIFI_PASSWORD)) {
        printf("WiFi configuration failed\n");
        led_on(4);
        while (1);
    }

    if (!wifi_wait_for_ip()) {
        printf("WiFi: no IP address assigned\n");
        led_on(4);
        while (1);
    }

    wifi_log_status();

    _delay_ms(2000); // allow DHCP to settle before opening a TCP socket

    if (!mqtt_connect()) {
        printf("MQTT connection failed\n");
        led_on(4);
        while (1);
    }

    char cmd_topic[32];
    snprintf(cmd_topic, sizeof(cmd_topic), "farm/%u/cmd", setup_id);
    mqtt_subscribe(cmd_topic);

    mqtt_connected = true;
    printf("Device ready. Setup ID: %u\n", setup_id);

    // ── Main loop ─────────────────────────────────────────────────────────────
    // Each iteration is ~100 ms (enforced by the _delay_ms at the bottom).
    // Counters are incremented every iteration and reset when their threshold
    // is reached so the interval drifts slightly under load but never misses.
    uint16_t telemetry_counter = 0;
    uint16_t heartbeat_counter = 0;
    uint16_t display_counter   = 0;

}
        // Drain the WiFi receive buffer. If a complete MQTT message arrived,
        // mqtt_poll_incoming() sets mqtt_command_received and fills mqtt_rx_buffer.
        mqtt_poll_incoming();
        if (mqtt_command_received) {
            device_handle_command(mqtt_rx_buffer);
            mqtt_rx_buffer[0] = '\0';
            mqtt_command_received = false;
}

        display_counter++;
        telemetry_counter++;
        heartbeat_counter++;

        if (display_counter >= DISPLAY_INTERVAL) {
            device_display_sensor_values();
            display_counter = 0;
        }

        if (telemetry_counter >= TELEMETRY_INTERVAL) {
            device_send_telemetry();
            telemetry_counter = 0;
        }

        if (heartbeat_counter >= HEARTBEAT_INTERVAL) {
            device_send_heartbeat();
            heartbeat_counter = 0;
        }

        // If a publish failed, the connection flag is cleared by the sender.
        // Wait 5 seconds before retrying to avoid hammering a lost broker.
        if (!mqtt_connected) {
            _delay_ms(5000);
            mqtt_connected = mqtt_connect();
        }

        _delay_ms(100); // base loop tick: 100 ms
    }

    return 0;
}