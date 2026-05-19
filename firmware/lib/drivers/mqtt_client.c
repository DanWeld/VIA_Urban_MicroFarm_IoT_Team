#include "mqtt_client.h"
#include "wifi.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <util/delay.h>

// Shared with main.c – declared there, referenced here for connection state
// and the receive buffer that the WiFi driver writes inbound bytes into.
extern bool     mqtt_connected;
extern char     mqtt_rx_buffer[];
extern bool     mqtt_command_received;
extern uint16_t setup_id;

// ── Internal helpers ──────────────────────────────────────────────────────────

// Encode 'value' using the MQTT variable-length encoding scheme (7 bits per
// byte, MSB=1 means another byte follows). Writes encoded bytes into 'out'
// and returns the number of bytes written (1–4).
static uint8_t encode_remaining_length(uint16_t value, uint8_t *out)
{
    uint8_t idx = 0;
    do {
        uint8_t encoded = value % 128;
        value /= 128;
        if (value > 0) encoded |= 0x80; // signal that more bytes follow
        out[idx++] = encoded;
    } while (value > 0 && idx < 4);
    return idx;
}

// Build and transmit an MQTT CONNECT packet.
// Uses protocol level 4 (MQTT 3.1.1), clean-session flag, 60-second keep-alive.
// No username, password, or will message – plain anonymous connection.
static bool send_connect_packet(void)
{
    uint8_t packet[128];
    uint8_t idx = 0;

    uint8_t  rem_bytes[4];
    uint16_t client_id_len = strlen(MQTT_CLIENT_ID);
    // Remaining length = fixed header (10) + 2-byte client-id length prefix + client-id
    uint16_t remaining = 10 + 2 + client_id_len;
    uint8_t  rem_size  = encode_remaining_length(remaining, rem_bytes);

    packet[idx++] = 0x10; // CONNECT control packet type
    for (uint8_t i = 0; i < rem_size; i++) packet[idx++] = rem_bytes[i];

    // Protocol name "MQTT" (4 bytes) + level 4
    packet[idx++] = 0x00; packet[idx++] = 0x04;
    packet[idx++] = 'M';  packet[idx++] = 'Q';
    packet[idx++] = 'T';  packet[idx++] = 'T';
    packet[idx++] = 0x04; // protocol level: MQTT 3.1.1
    packet[idx++] = 0x02; // connect flags: clean session only
    packet[idx++] = 0x00; packet[idx++] = 60; // keep-alive: 60 seconds

    // Client identifier
    packet[idx++] = client_id_len >> 8;
    packet[idx++] = client_id_len & 0xFF;
    memcpy(&packet[idx], MQTT_CLIENT_ID, client_id_len);
    idx += client_id_len;

    return wifi_command_TCP_transmit(packet, idx) == WIFI_OK;
}

// Build and transmit an MQTT SUBSCRIBE packet for the device command topic.
// Topic pattern: "farm/<setup_id>/cmd"  –  QoS 1 so commands are not lost.
// Packet identifier 0x0001 is fixed; acceptable for a single active subscription.
static bool send_subscribe_packet(void)
{
    uint8_t packet[128];
    uint8_t idx = 0;

    char     topic[32];
    snprintf(topic, sizeof(topic), "farm/%u/cmd", setup_id);
    uint16_t topic_len = strlen(topic);

    // Remaining = 2 (packet id) + 2 (topic len prefix) + topic + 1 (requested QoS)
    uint16_t remaining = 2 + 2 + topic_len + 1;

    uint8_t rem_bytes[4];
    uint8_t rem_size = encode_remaining_length(remaining, rem_bytes);

    packet[idx++] = 0x82; // SUBSCRIBE control packet (0x80 | 0x02)
    for (uint8_t i = 0; i < rem_size; i++) packet[idx++] = rem_bytes[i];

    // Fixed packet identifier
    packet[idx++] = 0x00;
    packet[idx++] = 0x01;

    // Topic filter
    packet[idx++] = topic_len >> 8;
    packet[idx++] = topic_len & 0xFF;
    memcpy(&packet[idx], topic, topic_len);
    idx += topic_len;

    packet[idx++] = 0x01; // subscribe at QoS 1

    return wifi_command_TCP_transmit(packet, idx) == WIFI_OK;
}

// ── Public API ────────────────────────────────────────────────────────────────

bool mqtt_connect(void)
{
    mqtt_rx_buffer[0] = '\0';

    // Open the TCP socket to the broker; the rx buffer receives all inbound bytes.
    if (wifi_command_create_TCP_connection(MQTT_BROKER_IP, MQTT_BROKER_PORT,
                                           NULL, mqtt_rx_buffer) != WIFI_OK) {
        printf("TCP connect to broker failed\n");
        return false;
    }

    _delay_ms(1000); // give the socket time to fully establish before sending CONNECT

    if (!send_connect_packet()) {
        printf("MQTT CONNECT packet failed\n");
        wifi_command_close_TCP_connection();
        return false;
    }

    _delay_ms(1000); // wait for CONNACK before subscribing

    if (!send_subscribe_packet()) {
        // Non-fatal: telemetry still works without a command subscription
        printf("MQTT SUBSCRIBE failed – command reception disabled\n");
    }

    _delay_ms(500); // wait for SUBACK

    printf("MQTT connected to %s:%u\n", MQTT_BROKER_IP, MQTT_BROKER_PORT);
    return true;
}

bool mqtt_publish(const char *topic, const char *payload)
{
    uint8_t packet[512];
    uint8_t idx = 0;

    uint16_t topic_len   = strlen(topic);
    uint16_t payload_len = strlen(payload);
    // Remaining = 2-byte topic length prefix + topic + payload (QoS 0 has no packet id)
    uint16_t remaining   = 2 + topic_len + payload_len;

    uint8_t rem_bytes[4];
    uint8_t rem_size = encode_remaining_length(remaining, rem_bytes);

    packet[idx++] = 0x30; // PUBLISH, QoS 0, no retain
    for (uint8_t i = 0; i < rem_size; i++) packet[idx++] = rem_bytes[i];

    packet[idx++] = topic_len >> 8;
    packet[idx++] = topic_len & 0xFF;
    memcpy(&packet[idx], topic, topic_len);
    idx += topic_len;

    memcpy(&packet[idx], payload, payload_len);
    idx += payload_len;

    return wifi_command_TCP_transmit(packet, idx) == WIFI_OK;
}

void mqtt_poll_incoming(void)
{
    if (!mqtt_connected) return;

    // The WiFi driver writes raw bytes into mqtt_rx_buffer; an empty first byte
    // means nothing has arrived since the last time we cleared the buffer.
    if (mqtt_rx_buffer[0] == '\0') return;

    // Set the flag if the incoming message looks like a device command.
    // main.c checks this flag each iteration and calls device_handle_command().
    if (strstr(mqtt_rx_buffer, "farm/") != NULL &&
        strstr(mqtt_rx_buffer, "/cmd")  != NULL)
    {
        mqtt_command_received = true;
    }

    // Clear buffer so the next poll sees fresh data.
    mqtt_rx_buffer[0] = '\0';
}