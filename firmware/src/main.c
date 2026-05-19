#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdbool.h>

#include "device_context.h"
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

int main(void)
{
    // Initialise shared application state. Passed by pointer to every module
    // that needs connection status, the receive buffer, or the device identity.
    // Using a context struct instead of extern globals makes all dependencies
    // explicit and keeps the modules independently testable.
    device_context_t ctx = {
        .mqtt_connected        = false,
        .mqtt_command_received = false,
        .setup_id              = 1,    // identifies this physical device to the backend
        .mqtt_rx_buffer        = {0},
    };

    system_init(); // initialise peripherals (display, sensors, pump, WiFi UART)

    // UART0 doubles as stdin/stdout so printf reaches the PC terminal.
    // Halt with LED4 on if this fails – nothing else can work without serial.
    if (uart_stdio_init(115200) != UART_OK) {
        led_on(4);
        while (1);
    }

    sei(); // enable global interrupts (required by UART RX ISRs and timer ISRs)

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

    // Open a TCP connection to the broker and complete the MQTT handshake.
    // ctx->mqtt_rx_buffer is registered with the WiFi driver here so all
    // subsequent inbound bytes land in the context buffer.
    if (!mqtt_connect(&ctx)) {
        printf("MQTT connection failed\n");
        led_on(4);
        while (1);
    }

    // Subscribe to the command topic for this device so the broker forwards
    // actuator commands (e.g. water pump) to us.
    char cmd_topic[32];
    snprintf(cmd_topic, sizeof(cmd_topic), "farm/%u/cmd", ctx.setup_id);
    mqtt_subscribe(cmd_topic);

    ctx.mqtt_connected = true;
    printf("Device ready. Setup ID: %u\n", ctx.setup_id);

    // ── Main loop ─────────────────────────────────────────────────────────────
    // Each iteration is ~100 ms (enforced by the _delay_ms at the bottom).
    // Counters are incremented every iteration and reset when their threshold
    // is reached so the interval drifts slightly under load but never misses.
    uint16_t telemetry_counter = 0;
    uint16_t heartbeat_counter = 0;
    uint16_t display_counter   = 0;

    while (1)
    {
        // Drain the WiFi receive buffer. If a complete MQTT PUBLISH arrived,
        // mqtt_poll_incoming sets ctx.mqtt_command_received and the payload
        // sits in ctx.mqtt_rx_buffer ready for the command handler below.
        mqtt_poll_incoming(&ctx);
        if (ctx.mqtt_command_received)
        {
            device_handle_command(ctx.mqtt_rx_buffer);
            ctx.mqtt_rx_buffer[0]     = '\0';  // clear buffer for next message
            ctx.mqtt_command_received = false;
        }

        display_counter++;
        telemetry_counter++;
        heartbeat_counter++;

        if (display_counter >= DISPLAY_INTERVAL)
        {
            device_display_sensor_values();
            display_counter = 0;
        }

        if (telemetry_counter >= TELEMETRY_INTERVAL)
        {
            device_send_telemetry(&ctx);
            telemetry_counter = 0;
        }

        if (heartbeat_counter >= HEARTBEAT_INTERVAL)
        {
            device_send_heartbeat(&ctx);
            heartbeat_counter = 0;
        }

        // If a publish failed, ctx.mqtt_connected was cleared by the sender.
        // Wait 5 seconds before retrying to avoid hammering a lost broker.
        if (!ctx.mqtt_connected)
        {
            _delay_ms(5000);
            ctx.mqtt_connected = mqtt_connect(&ctx);
        }

        _delay_ms(100); // base loop tick: 100 ms
    }

    return 0;
}