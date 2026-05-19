#pragma once
#include <stdbool.h>
#include "device_context.h"

// Broker connection parameters. MQTT_CLIENT_ID must be unique per device
// on the broker to avoid session conflicts.
#define MQTT_CLIENT_ID   "arduino_mega_001"
#define MQTT_BROKER_IP   "20.240.208.122"
#define MQTT_BROKER_PORT 1883

// Opens a TCP connection to the broker and sends an MQTT CONNECT packet.
// Uses ctx->mqtt_rx_buffer as the receive buffer so that inbound broker
// messages (CONNACK, PUBLISH) are available to mqtt_poll_incoming().
// Returns true when the broker accepted the connection, false on any failure.
bool mqtt_connect(device_context_t *ctx);

// Sends an MQTT SUBSCRIBE packet for the given topic at QoS 1.
// QoS 1 means the broker retries delivery until it receives an acknowledgement,
// so commands are not silently lost on a noisy link.
// Must be called after mqtt_connect() succeeds.
// Returns true when the packet was transmitted, false on failure.
bool mqtt_subscribe(const char *topic);

// Publishes a UTF-8 payload to the given topic using MQTT QoS 0 (fire-and-forget).
// Suitable for sensor telemetry where an occasional missed reading is acceptable.
// Returns true when the WiFi module accepted the transmission.
bool mqtt_publish(const char *topic, const char *payload);

// Inspects ctx->mqtt_rx_buffer for an inbound MQTT message written by the
// WiFi driver. If the topic matches the device command pattern ("farm/.../cmd"),
// sets ctx->mqtt_command_received to true so the main loop can dispatch it.
// All other inbound packets (CONNACK, SUBACK, PINGRESP) are silently discarded.
// Must be called regularly from the main loop.
void mqtt_poll_incoming(device_context_t *ctx);