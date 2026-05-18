#include <stdint.h>
#include <stdbool.h>

#define MQTT_CLIENT_ID            "arduino_mega_001"
#define MQTT_BROKER_IP            "20.240.208.122"
#define MQTT_BROKER_PORT          1883


void mqtt_command_poll_mqtt_incoming(void);


uint32_t mqtt_command_millis(void);

// MQTT: Encode remaining length for MQTT packet
uint8_t mqtt_command_mqtt_encode_remaining_length(uint16_t value, uint8_t *out);

// MQTT: Send CONNECT packet
bool mqtt_command_mqtt_send_connect_packet(void);

// MQTT: Send PUBLISH packet for telemetry
bool mqtt_command_mqtt_publish_telemetry(const char *topic, const char *payload);

// MQTT: Send SUBSCRIBE packet for commands
bool mqtt_command_mqtt_subscribe_commands(void);

// MQTT: Connections
bool mqtt_command_mqtt_connect(void);