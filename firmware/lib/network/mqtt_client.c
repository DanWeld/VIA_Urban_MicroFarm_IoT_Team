#include "mqtt_client.h"
#include "wifi.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <util/delay.h>

// MQTT uses a variable-length encoding for the "remaining length" field in
// every packet header. Each byte stores 7 bits of the value; if the value
// exceeds 127 the MSB is set to signal that another byte follows.
// This allows lengths up to 268 MB using at most 4 bytes.
//
// Examples:
//   value = 20  → [0x14]        (fits in 7 bits, 1 byte)
//   value = 200 → [0xC8, 0x01]  (MSB set on first byte, 2 bytes total)
//
// Writes the encoded bytes into 'out' and returns the number of bytes written.
static uint8_t encode_remaining_length(uint16_t value, uint8_t *out)
{
    uint8_t idx = 0;
    do {
        uint8_t encoded = value % 128;
        value /= 128;
        if (value > 0) encoded |= 0x80; // set MSB to signal more bytes follow
        out[idx++] = encoded;
    } while (value > 0 && idx < 4);
    return idx;
}

// Builds and transmits an MQTT CONNECT packet to the broker.
//
// Packet structure:
//   [0x10]          – packet type: CONNECT
//   [remaining len] – variable-length encoded size of everything that follows
//   [0x00 0x04]     – length prefix for the protocol name (4 bytes)
//   [M Q T T]       – protocol name
//   [0x04]          – protocol level 4 = MQTT version 3.1.1
//   [0x02]          – connect flags: clean session (no persistent state on broker)
//   [0x00 0x3C]     – keep-alive: 60 seconds (broker disconnects if silent longer)
//   [len high/low]  – length prefix for the client ID
//   [client ID]     – unique identifier for this device on the broker
//
// No username, password, or will message are used — anonymous connection.
// Returns true if the packet was transmitted successfully.
static bool send_connect_packet(void)
{
    uint8_t  packet[128];
    uint8_t  idx       = 0;
    uint8_t  rem_bytes[4];
    uint16_t id_len    = strlen(MQTT_CLIENT_ID);
    // Remaining length: 10 bytes fixed header + 2 byte length prefix + client ID
    uint16_t remaining = 10 + 2 + id_len;
    uint8_t  rem_size  = encode_remaining_length(remaining, rem_bytes);

    packet[idx++] = 0x10; // CONNECT packet type
    for (uint8_t i = 0; i < rem_size; i++) packet[idx++] = rem_bytes[i];

    // Protocol name "MQTT" followed by protocol level 4
    packet[idx++] = 0x00; packet[idx++] = 0x04;
    packet[idx++] = 'M';  packet[idx++] = 'Q';
    packet[idx++] = 'T';  packet[idx++] = 'T';
    packet[idx++] = 0x04; // protocol level: MQTT 3.1.1
    packet[idx++] = 0x02; // connect flags: clean session only
    packet[idx++] = 0x00; packet[idx++] = 60; // keep-alive: 60 seconds

    // Client identifier: length prefix followed by the ID string
    packet[idx++] = id_len >> 8;
    packet[idx++] = id_len & 0xFF;
    memcpy(&packet[idx], MQTT_CLIENT_ID, id_len);
    idx += id_len;

    return wifi_command_TCP_transmit(packet, idx) == WIFI_OK;
}

// Opens a TCP connection to the broker and performs the MQTT handshake.
//
// Step 1: Open a TCP socket to the broker. ctx->mqtt_rx_buffer is passed to
//         the WiFi driver so any bytes the broker sends back (CONNACK, incoming
//         publishes) are stored there and available to mqtt_poll_incoming().
// Step 2: Send an MQTT CONNECT packet identifying this device.
// Step 3: Wait 1 second for CONNACK. We use a fixed delay rather than parsing
//         the CONNACK byte-by-byte to keep the implementation simple.
//
// Returns true if the full handshake succeeded, false on any failure.
// On failure the TCP socket is closed to avoid leaving it half-open.
bool mqtt_connect(device_context_t *ctx)
{
    // Clear the buffer before connecting so stale data does not confuse
    // mqtt_poll_incoming() during the handshake.
    ctx->mqtt_rx_buffer[0] = '\0';

    if (wifi_command_create_TCP_connection(
            MQTT_BROKER_IP, MQTT_BROKER_PORT,
            NULL, ctx->mqtt_rx_buffer, MQTT_RX_BUFFER_SIZE) != WIFI_OK) {
        printf("TCP connection to broker failed\n");
        return false;
    }

    _delay_ms(1000); // wait for the TCP socket to fully establish

    if (!send_connect_packet()) {
        printf("MQTT CONNECT packet failed\n");
        // Close the socket so the ESP8266 does not keep a dangling connection.
        wifi_command_close_TCP_connection();
        return false;
    }

    _delay_ms(1000); // wait for CONNACK from the broker
    printf("MQTT connected to %s:%u\n", MQTT_BROKER_IP, MQTT_BROKER_PORT);
    return true;
}

// Sends an MQTT SUBSCRIBE packet for the given topic at QoS 1.
//
// Packet structure:
//   [0x82]          – packet type: SUBSCRIBE (0x80) + reserved bit (0x02)
//   [remaining len] – variable-length encoded size of everything that follows
//   [0x00 0x01]     – packet identifier (fixed at 1; we have one subscription)
//   [len high/low]  – length prefix for the topic string
//   [topic]         – topic filter to subscribe to
//   [0x01]          – requested QoS level: 1
bool mqtt_subscribe(const char *topic)
{
    uint8_t  packet[128];
    uint8_t  idx       = 0;
    uint16_t topic_len = strlen(topic);
    // Remaining: 2 (packet id) + 2 (topic length prefix) + topic + 1 (QoS byte)
    uint16_t remaining = 2 + 2 + topic_len + 1;
    uint8_t  rem_bytes[4];
    uint8_t  rem_size  = encode_remaining_length(remaining, rem_bytes);

    packet[idx++] = 0x82; // SUBSCRIBE packet type
    for (uint8_t i = 0; i < rem_size; i++) packet[idx++] = rem_bytes[i];

    // Packet identifier – used by the broker to match SUBACK to this request
    packet[idx++] = 0x00; packet[idx++] = 0x01;

    // Topic filter: length prefix followed by the topic string
    packet[idx++] = topic_len >> 8;
    packet[idx++] = topic_len & 0xFF;
    memcpy(&packet[idx], topic, topic_len);
    idx += topic_len;

    packet[idx++] = 0x00; // requested QoS: 1

    if (wifi_command_TCP_transmit(packet, idx) != WIFI_OK) {
        printf("MQTT SUBSCRIBE failed for topic '%s'\n", topic);
        return false;
    }

    _delay_ms(500); // wait for SUBACK from the broker
    printf("Subscribed to '%s'\n", topic);
    return true;
}

// Publishes a message to the given topic using MQTT QoS 0 (fire-and-forget).
//
// Packet structure:
//   [0x30]          – packet type: PUBLISH, QoS 0, no retain flag
//   [remaining len] – variable-length encoded size of everything that follows
//   [len high/low]  – length prefix for the topic string
//   [topic]         – destination topic
//   [payload]       – raw UTF-8 message body (no length prefix at QoS 0)
bool mqtt_publish(const char *topic, const char *payload)
{
    uint8_t  packet[512];
    uint8_t  idx         = 0;
    uint16_t topic_len   = strlen(topic);
    uint16_t payload_len = strlen(payload);
    // QoS 0 has no packet identifier field
    uint16_t remaining   = 2 + topic_len + payload_len;
    uint8_t  rem_bytes[4];
    uint8_t  rem_size    = encode_remaining_length(remaining, rem_bytes);

    packet[idx++] = 0x30; // PUBLISH, QoS 0, no retain
    for (uint8_t i = 0; i < rem_size; i++) packet[idx++] = rem_bytes[i];

    // Topic: length prefix followed by the topic string
    packet[idx++] = topic_len >> 8;
    packet[idx++] = topic_len & 0xFF;
    memcpy(&packet[idx], topic, topic_len);
    idx += topic_len;

    // Payload: raw bytes, no length prefix needed at QoS 0
    memcpy(&packet[idx], payload, payload_len);
    idx += payload_len;

    return wifi_command_TCP_transmit(packet, idx) == WIFI_OK;
}

// Checks ctx->mqtt_rx_buffer for an inbound MQTT message from the broker.
//
// The WiFi driver writes incoming bytes into ctx->mqtt_rx_buffer as they
// arrive over the TCP connection. This function inspects the buffer each
// loop cycle and sets ctx->mqtt_command_received when a command message
// is detected so the main loop can dispatch it to device_handle_command().
//
// Only messages on the device command topic ("farm/.../cmd") are acted upon.
// All other inbound packets — CONNACK, SUBACK, PINGRESP — are silently
// discarded because we use fixed delays after connect and subscribe instead
// of parsing acknowledgements byte-by-byte.
void mqtt_poll_incoming(device_context_t *ctx)
{
    if (!ctx->mqtt_connected) return;
    if (ctx->mqtt_rx_buffer[0] == '\0') return;

    if (strstr(ctx->mqtt_rx_buffer, "farm/") != NULL &&
        strstr(ctx->mqtt_rx_buffer, "/cmd")  != NULL)
    {
        ctx->mqtt_command_received = true;
    }
}