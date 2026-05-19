#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdbool.h>

#include "device_context.h"
#include "interfaces.h"
#include "uart_stdio.h"
#include "led.h"
#include "system_init.h"
#include "wifi_connection.h"
#include "mqtt_client.h"
#include "device_controller.h"
#include "wpump_controller.h"

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

// ── Logger implementation ─────────────────────────────────────────────────────
// Wraps printf so the logger interface can be swapped for a silent mock in tests.
static void logger_write(const char *msg)
{
    printf("%s\n", msg);
}

int main(void)
{
    // ── Shared application state ──────────────────────────────────────────────
    device_context_t ctx = {
        .mqtt_connected        = false,
        .mqtt_command_received = false,
        .setup_id              = 1,
        .mqtt_rx_buffer        = {0},
    };

    // ── Wire up concrete interface implementations ────────────────────────────
    // Each interface binds a concern to its real implementation.
    // To unit-test any module, replace the relevant interface with a mock here
    // without changing any other code.

    // Real sensor implementation reads from dht11, light and soil drivers.
    const sensor_interface_t sensors = {
        .read = device_read_sensors,
    };

    // Real pump implementation drives the relay via wpump_controller.
    const pump_interface_t pump = {
        .dispense = wpump_controller_dispense,
    };

    // Real MQTT implementation communicates over WiFi/TCP.
    const mqtt_interface_t mqtt = {
        .connect   = mqtt_connect,
        .subscribe = mqtt_subscribe,
        .publish   = mqtt_publish,
        .poll      = mqtt_poll_incoming,
    };

    // Real logger writes to UART0 via printf.
    const logger_interface_t logger = {
        .write = logger_write,
    };

    // ── Hardware initialisation ───────────────────────────────────────────────
    system_init();

    if (uart_stdio_init(115200) != UART_OK) {
        led_on(4);
        while (1);
    }

    sei(); // enable global interrupts (required by UART RX ISRs and timer ISRs)

    if (!wifi_configure(WIFI_SSID, WIFI_PASSWORD)) {
        logger.write("WiFi configuration failed");
        led_on(4);
        while (1);
    }

    if (!wifi_wait_for_ip()) {
        logger.write("WiFi: no IP address assigned");
        led_on(4);
        while (1);
    }

    wifi_log_status();
    _delay_ms(2000); // allow DHCP to settle before opening a TCP socket

    if (!mqtt.connect(&ctx)) {
        logger.write("MQTT connection failed");
        led_on(4);
        while (1);
    }

    char cmd_topic[32];
    snprintf(cmd_topic, sizeof(cmd_topic), "farm/%u/cmd", ctx.setup_id);
    mqtt.subscribe(cmd_topic);

    ctx.mqtt_connected = true;
    logger.write("Device ready");

    // ── Main loop ─────────────────────────────────────────────────────────────
    // Each iteration is ~100 ms. Counters are incremented every iteration and
    // reset when their threshold is reached.
    uint16_t telemetry_counter = 0;
    uint16_t heartbeat_counter = 0;
    uint16_t display_counter   = 0;

    while (1)
    {
        // Drain the WiFi receive buffer. If a complete MQTT PUBLISH arrived,
        // poll sets ctx.mqtt_command_received so the handler below can act on it.
        mqtt.poll(&ctx);
        if (ctx.mqtt_command_received)
        {
            device_handle_command(ctx.mqtt_rx_buffer, &pump, &logger);
            ctx.mqtt_rx_buffer[0]     = '\0';
            ctx.mqtt_command_received = false;
        }

        display_counter++;
        telemetry_counter++;
        heartbeat_counter++;

        if (display_counter >= DISPLAY_INTERVAL)
        {
            device_display_sensor_values(&sensors, &logger);
            display_counter = 0;
        }

        if (telemetry_counter >= TELEMETRY_INTERVAL)
        {
            device_send_telemetry(&ctx, &mqtt, &sensors, &logger);
            telemetry_counter = 0;
        }

        if (heartbeat_counter >= HEARTBEAT_INTERVAL)
        {
            device_send_heartbeat(&ctx, &mqtt, &logger);
            heartbeat_counter = 0;
        }

        // If a publish failed, ctx.mqtt_connected was cleared by the sender.
        // Wait 5 seconds before retrying to avoid hammering a lost broker.
        if (!ctx.mqtt_connected)
        {
            _delay_ms(5000);
            ctx.mqtt_connected = mqtt.connect(&ctx);
        }

        _delay_ms(100); // base loop tick: 100 ms
    }

    return 0;
}