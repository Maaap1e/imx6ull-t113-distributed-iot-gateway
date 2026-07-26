#ifndef MQTT_COMMAND_H
#define MQTT_COMMAND_H

#include <stddef.h>

typedef enum {
    MQTT_LED_COMMAND_INVALID = -1,
    MQTT_LED_COMMAND_OFF = 0,
    MQTT_LED_COMMAND_ON = 1,
    MQTT_LED_COMMAND_HEARTBEAT = 2
} mqtt_led_command_t;

int mqtt_parse_led_command(const void *payload,
                           size_t payload_len,
                           mqtt_led_command_t *command);

#endif
