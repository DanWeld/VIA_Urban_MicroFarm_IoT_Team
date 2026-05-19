#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <stdint.h>

/**
 * @brief Callback function type invoked when a message is received on a subscribed topic.
 * @param payload The received message payload as a null-terminated string.
 */
typedef void (*MQTT_MessageCallback_t)(const char *payload);

/**
 * @brief Return codes for MQTT operations.
 */
typedef enum {
    MQTT_OK,                  // Operation successful
    MQTT_ERROR_CONNECT,       // Failed to connect to broker
    MQTT_ERROR_PUBLISH,       // Failed to publish message
    MQTT_ERROR_SUBSCRIBE      // Failed to subscribe to topic
} MQTT_ERROR_t;

/**
 * @brief Initializes the MQTT client and establishes a TCP connection to the broker.
 * @param broker_ip IP address of the MQTT broker.
 * @param port Port number of the MQTT broker (default: 1883).
 * @param client_id Unique client identifier for this device.
 * @return MQTT_OK on success, MQTT_ERROR_CONNECT on failure.
 */
MQTT_ERROR_t mqtt_init(char *broker_ip, uint16_t port, char *client_id);

/**
 * @brief Publishes a message to the specified MQTT topic.
 * @param topic The topic to publish to (e.g. "farm/1/inf").
 * @param payload The message payload as a null-terminated JSON string.
 * @return MQTT_OK on success, MQTT_ERROR_PUBLISH on failure.
 */
MQTT_ERROR_t mqtt_publish(const char *topic, const char *payload);

/**
 * @brief Subscribes to the specified MQTT topic.
 * @param topic The topic to subscribe to (e.g. "farm/1/cmd").
 * @param callback Function to call when a message is received on this topic.
 * @return MQTT_OK on success, MQTT_ERROR_SUBSCRIBE on failure.
 */
MQTT_ERROR_t mqtt_subscribe(const char *topic, MQTT_MessageCallback_t callback);

#endif
