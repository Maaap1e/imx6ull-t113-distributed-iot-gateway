#include "../linux/mqtt_bridge/mqtt_command.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expect_command(const void *payload,
                           size_t payload_len,
                           int expected_result,
                           mqtt_led_command_t expected_command,
                           const char *name)
{
    mqtt_led_command_t command = MQTT_LED_COMMAND_INVALID;
    int result = mqtt_parse_led_command(payload, payload_len, &command);

    if (result != expected_result || command != expected_command) {
        fprintf(stderr,
                "%s failed: result=%d command=%d expected_result=%d "
                "expected_command=%d\n",
                name, result, (int)command, expected_result,
                (int)expected_command);
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    static const unsigned char with_nul[] = {'1', '\0'};
    static const unsigned char with_crlf[] = {'2', '\r', '\n'};
    static const unsigned char with_spaces[] = {' ', '0', ' '};
    static const unsigned char invalid_multi[] = {'1', '0'};

    expect_command("0", 1, 0, MQTT_LED_COMMAND_OFF, "off");
    expect_command("1", 1, 0, MQTT_LED_COMMAND_ON, "on");
    expect_command("2", 1, 0, MQTT_LED_COMMAND_HEARTBEAT, "heartbeat");
    expect_command(with_nul, sizeof(with_nul), 0,
                   MQTT_LED_COMMAND_ON, "trailing-nul");
    expect_command(with_crlf, sizeof(with_crlf), 0,
                   MQTT_LED_COMMAND_HEARTBEAT, "trailing-crlf");
    expect_command(with_spaces, sizeof(with_spaces), 0,
                   MQTT_LED_COMMAND_OFF, "surrounding-spaces");
    expect_command(invalid_multi, sizeof(invalid_multi), -1,
                   MQTT_LED_COMMAND_INVALID, "multiple-digits");
    expect_command("x", 1, -1, MQTT_LED_COMMAND_INVALID, "unknown-command");
    expect_command("", 0, -1, MQTT_LED_COMMAND_INVALID, "empty");

    puts("mqtt command tests passed");
    return EXIT_SUCCESS;
}
