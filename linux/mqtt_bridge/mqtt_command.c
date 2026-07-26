#include "mqtt_command.h"

static int is_ignored_byte(unsigned char value)
{
    return value == '\0' || value == ' ' || value == '\t' ||
           value == '\r' || value == '\n';
}

int mqtt_parse_led_command(const void *payload,
                           size_t payload_len,
                           mqtt_led_command_t *command)
{
    const unsigned char *bytes = payload;
    size_t begin = 0;
    size_t end = payload_len;

    if (bytes == NULL || command == NULL) {
        return -1;
    }

    while (begin < end && is_ignored_byte(bytes[begin])) {
        begin++;
    }
    while (end > begin && is_ignored_byte(bytes[end - 1])) {
        end--;
    }
    if (end - begin != 1u) {
        *command = MQTT_LED_COMMAND_INVALID;
        return -1;
    }

    switch (bytes[begin]) {
    case '0':
        *command = MQTT_LED_COMMAND_OFF;
        return 0;
    case '1':
        *command = MQTT_LED_COMMAND_ON;
        return 0;
    case '2':
        *command = MQTT_LED_COMMAND_HEARTBEAT;
        return 0;
    default:
        *command = MQTT_LED_COMMAND_INVALID;
        return -1;
    }
}
