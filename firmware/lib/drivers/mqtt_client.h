#pragma once
#include <stdint.h>
#include <stdbool.h>

// These values must match the cloud broker configuration.
// MQTT_CLIENT_ID must be unique per device on the broker.
#define MQTT_CLIENT_ID   "arduino_mega_001"
#define MQTT_BROKER_IP   "20.240.208.122"
#define MQTT_BROKER_PORT 1883

// Open a TCP connection to the broker, send an MQTT CONNECT packet, and
// subscribe to the device command topic ("farm/<setup_id>/cmd").
// Returns true when the broker has accepted the connection, false otherwise.
bool mqtt_connect(void);

// Publish a UTF-8 payload to the given topic using MQTT QoS 0.
// 'topic'   – null-terminated topic string, e.g. "farm/1/inf"
// 'payload' – null-terminated JSON string
// Returns true when the TCP transmit was accepted by the WiFi module.
bool mqtt_publish(const char *topic, const char *payload);

// Check the shared receive buffer for an inbound MQTT message and set
// mqtt_command_received = true if the topic matches the device command
// pattern ("farm/…/cmd"). Must be called regularly from the main loop.
void mqtt_poll_incoming(void);