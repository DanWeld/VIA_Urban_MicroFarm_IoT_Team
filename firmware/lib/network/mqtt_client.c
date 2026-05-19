#include "mqtt_client.h"
#include "wifi.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <util/delay.h>

// ── Shared state ──────────────────────────────────────────────────────────────
// These variables are declared in main.c and shared across modules via extern.
// mqtt_connected   – tracks whether the broker connection is currently active.
//                    Set to false by publish functions on failure, so main.c
//                    knows to reconnect.
// mqtt_rx_buffer   – shared receive buffer. The WiFi driver writes every byte
//                    it receives from the ESP8266 into this buffer. When the
//                    broker sends a message to the device, it arrives here.
//                    mqtt_poll_incoming() reads and clears this buffer each loop.
// mqtt_command_received – flag set to true when an inbound command has been
//                    processed, so main.c can react if needed.
extern bool  mqtt_connected;
extern char  mqtt_rx_buffer[];
extern bool  mqtt_command_received;

// ── Internal helpers ──────────────────────────────────────────────────────────

// MQTT uses a variable-length encoding for the "remaining length" field in
// every packet header. Each byte stores 7 bits of the value. If the value
// is larger than 127, the MSB of the current byte is set to 1 to signal
// that another byte follows. This allows lengths up to 268 MB using 4 bytes.
//
// Examples:
//   value = 20  → [0x14]           (1 byte, fits in 7 bits)
//   value = 200 → [0xC8, 0x01]     (2 bytes, MSB set on first byte)
//
// Writes encoded bytes into 'out' and returns how many bytes were written.
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
// The CONNECT packet tells the broker who we are and how we want to connect.
// Packet structure:
//   [0x10]          – packet type: CONNECT
//   [remaining len] – encoded length of everything that follows
//   [0x00 0x04]     – length of protocol name (4 bytes)
//   [M Q T T]       – protocol name
//   [0x04]          – protocol level: 4 = MQTT version 3.1.1
//   [0x02]          – connect flags: clean session (no persistent state on broker)
//   [0x00 0x3C]     – keep-alive: 60 seconds (broker disconnects if silent longer)
//   [len high/low]  – length of client ID
//   [client ID]     – unique identifier for this device on the broker
//
// No username, password, or will message are used – anonymous connection.
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

// ── Public API ────────────────────────────────────────────────────────────────

// Opens a TCP connection to the broker and performs the MQTT handshake.
//
// Step 1: Open a TCP socket to the broker IP and port. The WiFi driver is
//         given mqtt_rx_buffer so that any bytes the broker sends back
//         (CONNACK, incoming publishes) are stored there automatically.
// Step 2: Send an MQTT CONNECT packet identifying this device.
// Step 3: Wait for CONNACK (broker confirmation). We wait 1 second rather
//         than parsing the CONNACK byte-by-byte for simplicity.
//
// Returns true if the full handshake succeeded, false on any failure.
// On failure the TCP socket is closed to avoid leaving it half-open.
bool mqtt_connect(void)
{
    // Clear the buffer before connecting so old data does not confuse
    // mqtt_poll_incoming() during the handshake.
    mqtt_rx_buffer[0] = '\0';

    if (wifi_command_create_TCP_connection(
            MQTT_BROKER_IP, MQTT_BROKER_PORT,
            NULL, mqtt_rx_buffer, 256) != WIFI_OK) {
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
// Subscribing tells the broker to forward any message published on this
// topic to our device. QoS 1 means the broker retries delivery until it
// receives an acknowledgement, so commands are not silently lost.
//
// Packet structure:
//   [0x82]          – packet type: SUBSCRIBE (0x80) + reserved bit (0x02)
//   [remaining len] – encoded length of everything that follows
//   [0x00 0x01]     – packet identifier (fixed at 1, we have one subscription)
//   [len high/low]  – length of topic string
//   [topic]         – topic filter to subscribe to
//   [0x01]          – requested QoS level: 1
//
// Must be called after mqtt_connect() succeeds.
// Returns true when the packet was transmitted, false on failure.
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

// Publishes a message to the given topic using MQTT QoS 0.
//
// QoS 0 means fire-and-forget: the packet is sent once with no
// acknowledgement or retry. Suitable for sensor telemetry where an
// occasional missed reading is acceptable.
//
// Packet structure:
//   [0x30]          – packet type: PUBLISH, QoS 0, no retain flag
//   [remaining len] – encoded length of everything that follows
//   [len high/low]  – length of topic string
//   [topic]         – destination topic
//   [payload]       – raw UTF-8 message body (no length prefix at QoS 0)
//
// Returns true when the WiFi module accepted the transmission.
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

// Checks the shared receive buffer for an inbound MQTT message from the broker.
//
// The WiFi driver writes incoming bytes into mqtt_rx_buffer as they arrive
// over the TCP connection. This function inspects the buffer each loop cycle.
//
// Only messages on the device command topic ("farm/.../cmd") are acted upon.
// All other inbound packets – CONNACK, SUBACK, PINGRESP – are silently
// discarded. They are not needed because we use fixed delays after connect
// and subscribe instead of parsing acknowledgements byte-by-byte.
//
// When a command message is detected, it is forwarded to the command handler
// in device_controller and the buffer is cleared for the next message.
void mqtt_poll_incoming(void)
{
    if (!mqtt_connected)           return;
     if (wifi_ipd_received) {
        printf("IPD ontvangen! buffer: [%s]\n", mqtt_rx_buffer);
        wifi_ipd_received = 0;
    }
    if (mqtt_rx_buffer[0] == '\0') return; // nothing received since last poll

    printf("RX buffer: [%s]\n", mqtt_rx_buffer);  // tijdelijk

    if (strstr(mqtt_rx_buffer, "farm/") != NULL &&
        strstr(mqtt_rx_buffer, "/cmd")  != NULL)
    {
        mqtt_command_received = true;
    }

    // Clear the buffer so the next poll starts with fresh data.
    mqtt_rx_buffer[0] = '\0';
}