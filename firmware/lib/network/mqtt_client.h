#pragma once
#include <stdbool.h>

// Broker and device identity.
// MQTT_CLIENT_ID must be unique per device on the broker.
#define MQTT_CLIENT_ID   "arduino_mega_001"
#define MQTT_BROKER_IP   "20.240.208.122"
#define MQTT_BROKER_PORT 1883


// Opens a TCP connection to the broker and sends an MQTT CONNECT packet.
// Returns true when the connection is accepted, false on any failure.
bool mqtt_connect(void);

// Subscribes to the given topic at QoS 1.
// Must be called after mqtt_connect() succeeds.
// Returns true when the subscription was accepted, false on failure.
bool mqtt_subscribe(const char *topic);

// Publishes a UTF-8 payload to the given topic using MQTT QoS 0.
// Returns true when the WiFi module accepted the transmission.
bool mqtt_publish(const char *topic, const char *payload);

// Checks the receive buffer for an inbound MQTT message.
// If the topic matches the device command pattern ("farm/.../cmd"),
// the payload is forwarded to the command handler.
// Must be called regularly from the main loop.
void mqtt_poll_incoming(void);