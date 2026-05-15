#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <util/delay.h>
#include <stdbool.h>

#include "wifi.h"
#include "commands.h"
#include "mqtt_commands.h"

extern bool mqtt_connected;
extern char mqtt_rx_buffer[];
extern bool mqtt_command_received;
extern uint32_t millis_counter;
extern uint16_t setup_id;    
extern uint16_t telemetry_counter;

void mqtt_command_poll_mqtt_incoming(void) {
    if (!mqtt_connected) {
        return;
    }

    if (mqtt_rx_buffer[0] == '\0') {
        return;
    }

    if (strstr(mqtt_rx_buffer, "farm/") != NULL && strstr(mqtt_rx_buffer, "/cmd") != NULL) {
        commands_handle_backend_command(mqtt_rx_buffer);
        mqtt_rx_buffer[0] = '\0';
        mqtt_command_received = true;
    }
}


uint32_t mqtt_command_millis(void) {
    return millis_counter / 2;  // Assuming ~2ms per tick
}


uint8_t mqtt_command_mqtt_encode_remaining_length(uint16_t value, uint8_t *out) {
    uint8_t idx = 0;
    do {
        uint8_t encoded = value % 128;
        value /= 128;
        if (value > 0) encoded |= 0x80;
        out[idx++] = encoded;
    } while (value > 0 && idx < 4);
    return idx;
}

bool mqtt_command_mqtt_send_connect_packet(void) {
    uint8_t packet[128];
    uint8_t idx = 0;
    
    uint8_t rem_len_bytes[4];
    uint16_t len = strlen(MQTT_CLIENT_ID);
    uint16_t remaining = 10 + 2 + len;
    
    uint8_t rem_size = mqtt_command_mqtt_encode_remaining_length(remaining, rem_len_bytes);
    
    packet[idx++] = 0x10;  // CONNECT control packet
    for(uint8_t i = 0; i < rem_size; i++) packet[idx++] = rem_len_bytes[i];
    
    // MQTT protocol name
    packet[idx++] = 0x00;
    packet[idx++] = 0x04;
    packet[idx++] = 'M';
    packet[idx++] = 'Q';
    packet[idx++] = 'T';
    packet[idx++] = 'T';
    packet[idx++] = 0x04;  // Protocol level 4 (MQTT 3.1.1)
    packet[idx++] = 0x02;  // Connect flags: clean session
    packet[idx++] = 0x00;
    packet[idx++] = 60;    // Keep alive (seconds)
    
    packet[idx++] = len >> 8;
    packet[idx++] = len & 0xFF;
    
    memcpy(&packet[idx], MQTT_CLIENT_ID, len);
    idx += len;
    
    return wifi_command_TCP_transmit(packet, idx) == WIFI_OK;
}

bool mqtt_command_mqtt_publish_telemetry(const char *topic, const char *payload) {
    uint8_t packet[512];
    uint8_t idx = 0;
    
    uint16_t topic_len = strlen(topic);
    uint16_t payload_len = strlen(payload);
    uint16_t remaining = 2 + topic_len + payload_len;  // topic_len_bytes + topic + payload
    
    uint8_t rem_len_bytes[4];
    uint8_t rem_size = mqtt_command_mqtt_encode_remaining_length(remaining, rem_len_bytes);
    
    packet[idx++] = 0x30;  // PUBLISH QoS 0
    for(uint8_t i = 0; i < rem_size; i++) packet[idx++] = rem_len_bytes[i];
    
    // Topic string
    packet[idx++] = topic_len >> 8;
    packet[idx++] = topic_len & 0xFF;
    memcpy(&packet[idx], topic, topic_len);
    idx += topic_len;
    
    // Payload
    memcpy(&packet[idx], payload, payload_len);
    idx += payload_len;
    
    bool result = wifi_command_TCP_transmit(packet, idx) == WIFI_OK;
    return result;
}


bool mqtt_command_mqtt_subscribe_commands(void) {
    uint8_t packet[128];
    uint8_t idx = 0;
    
    char topic[32];
    snprintf(topic, sizeof(topic), "farm/%u/cmd", setup_id);
    uint16_t topic_len = strlen(topic);
    
    uint16_t remaining = 2 + 2 + topic_len + 1;  // packet_id + topic_len + topic + QoS
    
    uint8_t rem_len_bytes[4];
    uint8_t rem_size = mqtt_command_mqtt_encode_remaining_length(remaining, rem_len_bytes);
    
    packet[idx++] = 0x82;  // SUBSCRIBE with QoS 1 (10000010)
    for(uint8_t i = 0; i < rem_size; i++) packet[idx++] = rem_len_bytes[i];
    
    // Packet identifier
    packet[idx++] = 0x00;
    packet[idx++] = 0x01;
    
    // Topic string
    packet[idx++] = topic_len >> 8;
    packet[idx++] = topic_len & 0xFF;
    memcpy(&packet[idx], topic, topic_len);
    idx += topic_len;
    
    packet[idx++] = 0x01;  // QoS 1
    
    return wifi_command_TCP_transmit(packet, idx) == WIFI_OK;
}


bool mqtt_command_mqtt_connect(void) {
    mqtt_rx_buffer[0] = '\0';

    if (wifi_command_create_TCP_connection(MQTT_BROKER_IP, MQTT_BROKER_PORT, NULL, mqtt_rx_buffer) != WIFI_OK) {
        printf("Failed to connect to MQTT broker\n");
        return false;
    }
    
    _delay_ms(1000);  // Wait for connection to fully establish
    
    if (!mqtt_command_mqtt_send_connect_packet()) {
        printf("Failed to send MQTT CONNECT\n");
        wifi_command_close_TCP_connection();
        return false;
    }
    
    _delay_ms(1000);  // Wait for CONNECT response
    
    if (!mqtt_command_mqtt_subscribe_commands()) {
        printf("Failed to subscribe to commands\n");
    }
    
    _delay_ms(500);  // Wait for SUBSCRIBE response
    
    printf("MQTT connected\n");
    return true;
}