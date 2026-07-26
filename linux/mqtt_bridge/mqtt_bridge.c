#include "mqtt_command.h"

#include <MQTTClient.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_BROKER_URI "tcp://127.0.0.1:1883"
#define DEFAULT_CLIENT_ID "imx6ull-gateway-01"
#define DEFAULT_TOPIC_PREFIX "iot-gateway/imx6ull-01"
#define DEFAULT_STATE_FILE "/tmp/imx6ull_gateway_state.json"
#define DEFAULT_LED_DIR "/sys/class/leds/sys-led"
#define DEFAULT_PUBLISH_INTERVAL 5u
#define MAX_TOPIC_LENGTH 512
#define MAX_PATH_LENGTH 512
#define MAX_STATE_LENGTH 8192
#define MAX_RESPONSE_LENGTH 256
#define RECONNECT_MIN_SECONDS 1u
#define RECONNECT_MAX_SECONDS 30u
#define SUMMARY_LOG_SECONDS 60

typedef struct {
    const char *broker_uri;
    const char *client_id;
    const char *topic_prefix;
    const char *state_file;
    const char *led_dir;
    unsigned int publish_interval;
    int verbose;
} bridge_config_t;

typedef struct {
    char command_topic[MAX_TOPIC_LENGTH];
    char response_topic[MAX_TOPIC_LENGTH];
    char led_dir[MAX_PATH_LENGTH];
    pthread_mutex_t response_lock;
    int response_pending;
    char response[MAX_RESPONSE_LENGTH];
} bridge_context_t;

static volatile sig_atomic_t g_running = 1;

static void handle_stop_signal(int signo)
{
    (void)signo;
    g_running = 0;
}

static void sleep_ms(unsigned int milliseconds)
{
    struct timespec request;

    request.tv_sec = milliseconds / 1000u;
    request.tv_nsec = (long)(milliseconds % 1000u) * 1000000L;
    while (g_running && nanosleep(&request, &request) < 0 && errno == EINTR) {
    }
}

static const char *env_or_default(const char *name, const char *default_value)
{
    const char *value = getenv(name);

    return value != NULL && value[0] != '\0' ? value : default_value;
}

static int join_topic(char *buffer,
                      size_t buffer_size,
                      const char *prefix,
                      const char *suffix)
{
    size_t prefix_length = strlen(prefix);
    int written;

    while (prefix_length > 0u && prefix[prefix_length - 1u] == '/') {
        prefix_length--;
    }
    written = snprintf(buffer, buffer_size, "%.*s/%s",
                       (int)prefix_length, prefix, suffix);
    return written >= 0 && (size_t)written < buffer_size ? 0 : -1;
}

static int topic_matches(const char *topic_name,
                         int topic_length,
                         const char *expected)
{
    size_t expected_length = strlen(expected);

    if (topic_name == NULL) {
        return 0;
    }
    if (topic_length == 0) {
        return strcmp(topic_name, expected) == 0;
    }
    return topic_length > 0 && (size_t)topic_length == expected_length &&
           memcmp(topic_name, expected, expected_length) == 0;
}

static int write_text_file(const char *path, const char *value)
{
    size_t length = strlen(value);
    size_t offset = 0;
    int fd = open(path, O_WRONLY);

    if (fd < 0) {
        return -1;
    }
    while (offset < length) {
        ssize_t written = write(fd, value + offset, length - offset);

        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            return -1;
        }
        if (written == 0) {
            close(fd);
            errno = EIO;
            return -1;
        }
        offset += (size_t)written;
    }
    return close(fd);
}

static int apply_led_command(const char *led_dir, mqtt_led_command_t command)
{
    char trigger_path[MAX_PATH_LENGTH];
    char brightness_path[MAX_PATH_LENGTH];
    int trigger_length;
    int brightness_length;

    trigger_length = snprintf(trigger_path, sizeof(trigger_path),
                              "%s/trigger", led_dir);
    brightness_length = snprintf(brightness_path, sizeof(brightness_path),
                                 "%s/brightness", led_dir);
    if (trigger_length < 0 ||
        (size_t)trigger_length >= sizeof(trigger_path) ||
        brightness_length < 0 ||
        (size_t)brightness_length >= sizeof(brightness_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    switch (command) {
    case MQTT_LED_COMMAND_HEARTBEAT:
        return write_text_file(trigger_path, "heartbeat");
    case MQTT_LED_COMMAND_ON:
        if (write_text_file(trigger_path, "none") < 0) {
            return -1;
        }
        return write_text_file(brightness_path, "1");
    case MQTT_LED_COMMAND_OFF:
        if (write_text_file(trigger_path, "none") < 0) {
            return -1;
        }
        return write_text_file(brightness_path, "0");
    default:
        errno = EINVAL;
        return -1;
    }
}

static void queue_response(bridge_context_t *context,
                           mqtt_led_command_t command,
                           int success,
                           const char *error)
{
    pthread_mutex_lock(&context->response_lock);
    snprintf(context->response, sizeof(context->response),
             "{\"command\":\"led\",\"value\":%d,\"success\":%s,"
             "\"error\":\"%s\",\"timestamp\":%ld}",
             (int)command,
             success ? "true" : "false",
             error,
             (long)time(NULL));
    context->response_pending = 1;
    pthread_mutex_unlock(&context->response_lock);
}

static int take_response(bridge_context_t *context,
                         char *buffer,
                         size_t buffer_size)
{
    int available = 0;

    pthread_mutex_lock(&context->response_lock);
    if (context->response_pending) {
        snprintf(buffer, buffer_size, "%s", context->response);
        context->response_pending = 0;
        available = 1;
    }
    pthread_mutex_unlock(&context->response_lock);
    return available;
}

static int message_arrived(void *callback_context,
                           char *topic_name,
                           int topic_length,
                           MQTTClient_message *message)
{
    bridge_context_t *context = callback_context;
    mqtt_led_command_t command = MQTT_LED_COMMAND_INVALID;
    int success = 0;
    const char *error = "invalid_payload";

    if (!topic_matches(topic_name, topic_length, context->command_topic)) {
        MQTTClient_freeMessage(&message);
        MQTTClient_free(topic_name);
        return 1;
    }

    if (message->payloadlen >= 0 &&
        mqtt_parse_led_command(message->payload,
                               (size_t)message->payloadlen,
                               &command) == 0) {
        if (apply_led_command(context->led_dir, command) == 0) {
            success = 1;
            error = "none";
        } else {
            error = "sysfs_write_failed";
        }
    }

    queue_response(context, command, success, error);
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topic_name);
    return 1;
}

static void connection_lost(void *callback_context, char *cause)
{
    (void)callback_context;
    fprintf(stderr, "MQTT connection lost: %s\n",
            cause == NULL ? "unknown" : cause);
}

static int read_state_json(const char *path, char *buffer, size_t capacity)
{
    size_t offset = 0;
    int fd = open(path, O_RDONLY);

    if (fd < 0) {
        return -1;
    }
    while (offset + 1u < capacity) {
        ssize_t received = read(fd, buffer + offset, capacity - offset - 1u);

        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            return -1;
        }
        if (received == 0) {
            break;
        }
        offset += (size_t)received;
    }
    if (offset + 1u == capacity) {
        char extra;
        ssize_t received = read(fd, &extra, 1);

        if (received > 0) {
            close(fd);
            errno = EMSGSIZE;
            return -1;
        }
    }
    close(fd);

    while (offset > 0u &&
           (buffer[offset - 1u] == '\r' || buffer[offset - 1u] == '\n' ||
            buffer[offset - 1u] == ' ' || buffer[offset - 1u] == '\t')) {
        offset--;
    }
    buffer[offset] = '\0';
    if (offset < 2u || buffer[0] != '{' || buffer[offset - 1u] != '}') {
        errno = EBADMSG;
        return -1;
    }
    return (int)offset;
}

static int publish_message(MQTTClient client,
                           const char *topic,
                           const char *payload,
                           int payload_length,
                           int qos,
                           int retained)
{
    MQTTClient_message message = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token = 0;
    int result;

    message.payload = (void *)payload;
    message.payloadlen = payload_length;
    message.qos = qos;
    message.retained = retained;
    result = MQTTClient_publishMessage(client, topic, &message,
                                       qos > 0 ? &token : NULL);
    if (result == MQTTCLIENT_SUCCESS && qos > 0) {
        result = MQTTClient_waitForCompletion(client, token, 5000L);
    }
    return result;
}

static int connect_bridge(MQTTClient client,
                          const bridge_config_t *config,
                          bridge_context_t *context,
                          const char *status_topic)
{
    static const char offline_message[] =
        "{\"status\":\"offline\",\"reason\":\"unexpected_disconnect\"}";
    static const char online_message[] = "{\"status\":\"online\"}";
    MQTTClient_connectOptions options = MQTTClient_connectOptions_initializer;
    MQTTClient_willOptions will = MQTTClient_willOptions_initializer;
    const char *username = getenv("MQTT_USERNAME");
    const char *password = getenv("MQTT_PASSWORD");
    int result;

    will.topicName = status_topic;
    will.message = offline_message;
    will.retained = 1;
    will.qos = 1;

    options.keepAliveInterval = 30;
    options.cleansession = 1;
    options.connectTimeout = 5;
    options.MQTTVersion = MQTTVERSION_3_1_1;
    options.will = &will;
    if (username != NULL && username[0] != '\0') {
        options.username = username;
    }
    if (password != NULL && password[0] != '\0') {
        options.password = password;
    }

    result = MQTTClient_connect(client, &options);
    if (result != MQTTCLIENT_SUCCESS) {
        return result;
    }
    result = MQTTClient_subscribe(client, context->command_topic, 1);
    if (result != MQTTCLIENT_SUCCESS) {
        MQTTClient_disconnect(client, 1000);
        return result;
    }
    result = publish_message(client, status_topic, online_message,
                             (int)strlen(online_message), 1, 1);
    if (result != MQTTCLIENT_SUCCESS) {
        MQTTClient_disconnect(client, 1000);
        return result;
    }

    printf("MQTT connected: broker=%s client_id=%s command_topic=%s\n",
           config->broker_uri, config->client_id, context->command_topic);
    return MQTTCLIENT_SUCCESS;
}

static void usage(const char *program)
{
    printf("Usage: %s [-b broker_uri] [-c client_id] [-t topic_prefix] "
           "[-f state_json] [-i seconds] [-L led_sysfs_dir] [-v]\n",
           program);
}

int main(int argc, char **argv)
{
    bridge_config_t config;
    bridge_context_t context;
    MQTTClient client;
    char telemetry_topic[MAX_TOPIC_LENGTH];
    char status_topic[MAX_TOPIC_LENGTH];
    char state_json[MAX_STATE_LENGTH];
    unsigned int reconnect_delay = RECONNECT_MIN_SECONDS;
    unsigned long publish_count = 0;
    time_t next_publish = 0;
    time_t last_warning = 0;
    time_t last_summary = 0;
    int option;
    int result;

    config.broker_uri = env_or_default("MQTT_BROKER_URI", DEFAULT_BROKER_URI);
    config.client_id = env_or_default("MQTT_CLIENT_ID", DEFAULT_CLIENT_ID);
    config.topic_prefix =
        env_or_default("MQTT_TOPIC_PREFIX", DEFAULT_TOPIC_PREFIX);
    config.state_file =
        env_or_default("MQTT_STATE_FILE", DEFAULT_STATE_FILE);
    config.led_dir = env_or_default("MQTT_LED_DIR", DEFAULT_LED_DIR);
    config.publish_interval = DEFAULT_PUBLISH_INTERVAL;
    config.verbose = 0;

    while ((option = getopt(argc, argv, "b:c:t:f:i:L:vh")) != -1) {
        switch (option) {
        case 'b':
            config.broker_uri = optarg;
            break;
        case 'c':
            config.client_id = optarg;
            break;
        case 't':
            config.topic_prefix = optarg;
            break;
        case 'f':
            config.state_file = optarg;
            break;
        case 'i':
            config.publish_interval = (unsigned int)strtoul(optarg, NULL, 10);
            break;
        case 'L':
            config.led_dir = optarg;
            break;
        case 'v':
            config.verbose = 1;
            break;
        case 'h':
        default:
            usage(argv[0]);
            return option == 'h' ? 0 : 2;
        }
    }
    if (config.publish_interval == 0u ||
        config.publish_interval > 3600u) {
        fprintf(stderr, "Invalid MQTT publish interval\n");
        return 2;
    }

    memset(&context, 0, sizeof(context));
    if (join_topic(telemetry_topic, sizeof(telemetry_topic),
                   config.topic_prefix, "telemetry") < 0 ||
        join_topic(status_topic, sizeof(status_topic),
                   config.topic_prefix, "status") < 0 ||
        join_topic(context.command_topic, sizeof(context.command_topic),
                   config.topic_prefix, "cmd/led") < 0 ||
        join_topic(context.response_topic, sizeof(context.response_topic),
                   config.topic_prefix, "cmd/response") < 0 ||
        snprintf(context.led_dir, sizeof(context.led_dir), "%s",
                 config.led_dir) >= (int)sizeof(context.led_dir)) {
        fprintf(stderr, "MQTT topic prefix or LED path is too long\n");
        return 2;
    }
    result = pthread_mutex_init(&context.response_lock, NULL);
    if (result != 0) {
        fprintf(stderr, "MQTT response mutex initialization failed: %s\n",
                strerror(result));
        return 1;
    }

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handle_stop_signal);
    signal(SIGTERM, handle_stop_signal);

    result = MQTTClient_create(&client, config.broker_uri, config.client_id,
                               MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (result != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "MQTT client creation failed: rc=%d\n", result);
        pthread_mutex_destroy(&context.response_lock);
        return 1;
    }
    result = MQTTClient_setCallbacks(client, &context, connection_lost,
                                     message_arrived, NULL);
    if (result != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "MQTT callback setup failed: rc=%d\n", result);
        MQTTClient_destroy(&client);
        pthread_mutex_destroy(&context.response_lock);
        return 1;
    }

    while (g_running) {
        time_t now = time(NULL);

        if (!MQTTClient_isConnected(client)) {
            result = connect_bridge(client, &config, &context, status_topic);
            if (result != MQTTCLIENT_SUCCESS) {
                fprintf(stderr,
                        "MQTT connect failed: rc=%d; retry in %u seconds\n",
                        result, reconnect_delay);
                sleep_ms(reconnect_delay * 1000u);
                if (reconnect_delay < RECONNECT_MAX_SECONDS) {
                    reconnect_delay *= 2u;
                    if (reconnect_delay > RECONNECT_MAX_SECONDS) {
                        reconnect_delay = RECONNECT_MAX_SECONDS;
                    }
                }
                continue;
            }
            reconnect_delay = RECONNECT_MIN_SECONDS;
            next_publish = 0;
        }

        {
            char response[MAX_RESPONSE_LENGTH];

            if (take_response(&context, response, sizeof(response))) {
                result = publish_message(client, context.response_topic,
                                         response, (int)strlen(response),
                                         1, 0);
                if (result != MQTTCLIENT_SUCCESS) {
                    fprintf(stderr,
                            "MQTT command response publish failed: rc=%d\n",
                            result);
                }
            }
        }

        if (now >= next_publish) {
            int state_length =
                read_state_json(config.state_file, state_json,
                                sizeof(state_json));

            if (state_length < 0) {
                if (config.verbose ||
                    now - last_warning >= SUMMARY_LOG_SECONDS) {
                    fprintf(stderr, "MQTT state read failed: %s: %s\n",
                            config.state_file, strerror(errno));
                    last_warning = now;
                }
            } else {
                result = publish_message(client, telemetry_topic, state_json,
                                         state_length, 0, 0);
                if (result != MQTTCLIENT_SUCCESS) {
                    fprintf(stderr,
                            "MQTT telemetry publish failed: rc=%d\n", result);
                } else {
                    publish_count++;
                    if (config.verbose ||
                        now - last_summary >= SUMMARY_LOG_SECONDS) {
                        printf("MQTT telemetry published: count=%lu bytes=%d\n",
                               publish_count, state_length);
                        last_summary = now;
                    }
                }
            }
            next_publish = now + (time_t)config.publish_interval;
        }
        sleep_ms(200);
    }

    if (MQTTClient_isConnected(client)) {
        static const char offline_message[] =
            "{\"status\":\"offline\",\"reason\":\"clean_shutdown\"}";

        (void)publish_message(client, status_topic, offline_message,
                              (int)strlen(offline_message), 1, 1);
        (void)MQTTClient_disconnect(client, 2000);
    }
    MQTTClient_destroy(&client);
    pthread_mutex_destroy(&context.response_lock);
    printf("MQTT bridge stopped: telemetry_published=%lu\n", publish_count);
    return 0;
}
