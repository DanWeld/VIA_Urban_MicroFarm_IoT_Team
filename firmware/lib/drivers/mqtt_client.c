#include "mqtt_client.h"
#include "wifi.h"
#include "../MQTTPacket/MQTTPacket.h"
#include <string.h>
#include <stdio.h>

#define MQTT_BUF_SIZE 256        // Buffer size for outgoing MQTT packets
#define MQTT_MESSAGE_BUF_SIZE 128 // Buffer size for incoming message payloads

static uint8_t mqtt_send_buf[MQTT_BUF_SIZE];          // Buffer used to serialize outgoing MQTT packets
static uint8_t mqtt_recv_buf[MQTT_BUF_SIZE];          // Buffer reserved for incoming MQTT packets
static char mqtt_message_buf[MQTT_MESSAGE_BUF_SIZE];  // Buffer for the incoming message payload string
static MQTT_MessageCallback_t mqtt_callback = NULL;   // User-registered callback for incoming messages

/**
 * @brief Internal callback passed to the WiFi driver.
 *        Invoked when a complete TCP message is received.
 *        Forwards the payload to the user-registered MQTT callback.
 */
static void on_message_received(void)
{
    if (mqtt_callback != NULL)
        mqtt_callback(mqtt_message_buf);
}

MQTT_ERROR_t mqtt_init(char *broker_ip, uint16_t port, char *client_id)
{
    // Establish TCP connection to the broker over WiFi
    WIFI_ERROR_MESSAGE_t wifi_error = wifi_command_create_TCP_connection(
        broker_ip, port, on_message_received, mqtt_message_buf
    );
    if (wifi_error != WIFI_OK)
        return MQTT_ERROR_CONNECT;

    // Configure MQTT CONNECT packet options
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.clientID.cstring = client_id;
    data.keepAliveInterval = 60; // Broker will disconnect if no message within 60 seconds
    data.cleansession = 1;       // Start with a clean session, no persistent state

    // Serialize the CONNECT packet into the send buffer
    int len = MQTTSerialize_connect(mqtt_send_buf, MQTT_BUF_SIZE, &data);
    if (len <= 0)
        return MQTT_ERROR_CONNECT;

    // Transmit the CONNECT packet to the broker via TCP
    WIFI_ERROR_MESSAGE_t send_error = wifi_command_TCP_transmit(mqtt_send_buf, len);
    if (send_error != WIFI_OK)
        return MQTT_ERROR_CONNECT;

    return MQTT_OK;
}

MQTT_ERROR_t mqtt_publish(const char *topic, const char *payload)
{
    // Wrap the topic string in an MQTTString structure as required by MQTTPacket
    MQTTString topic_str = MQTTString_initializer;
    topic_str.cstring = (char *)topic;

    // Serialize the PUBLISH packet into the send buffer
    int len = MQTTSerialize_publish(
        mqtt_send_buf, MQTT_BUF_SIZE,
        0,              // dup: not a duplicate
        1,              // QoS 1: guaranteed at least once delivery
        0,              // retain: broker does not retain this message
        0,              // packet id: only relevant for QoS > 0 acknowledgements
        topic_str,
        (uint8_t *)payload, strlen(payload)
    );
    if (len <= 0)
        return MQTT_ERROR_PUBLISH;

    // Transmit the PUBLISH packet to the broker via TCP
    WIFI_ERROR_MESSAGE_t error = wifi_command_TCP_transmit(mqtt_send_buf, len);
    if (error != WIFI_OK)
        return MQTT_ERROR_PUBLISH;

    return MQTT_OK;
}

MQTT_ERROR_t mqtt_subscribe(const char *topic, MQTT_MessageCallback_t callback)
{
    // Store the user callback to be invoked on incoming messages
    mqtt_callback = callback;

    // Wrap the topic string in an MQTTString structure as required by MQTTPacket
    MQTTString topic_str = MQTTString_initializer;
    topic_str.cstring = (char *)topic;
    int req_qos = 1; // Request QoS 1 for the subscription

    // Serialize the SUBSCRIBE packet into the send buffer
    int len = MQTTSerialize_subscribe(
        mqtt_send_buf, MQTT_BUF_SIZE,
        0,          // dup: not a duplicate
        1,          // packet id
        1,          // count: number of topics to subscribe to
        &topic_str,
        &req_qos
    );
    if (len <= 0)
        return MQTT_ERROR_SUBSCRIBE;

    // Transmit the SUBSCRIBE packet to the broker via TCP
    WIFI_ERROR_MESSAGE_t error = wifi_command_TCP_transmit(mqtt_send_buf, len);
    if (error != WIFI_OK)
        return MQTT_ERROR_SUBSCRIBE;

    return MQTT_OK;
}
