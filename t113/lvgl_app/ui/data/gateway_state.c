#include "gateway_state.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "lvgl.h"

typedef struct {
    char path[256];
    uint32_t poll_period_ms;
} gateway_poll_context_t;

static gateway_state_t s_gateway_state;
static gateway_poll_context_t s_poll_context = {
    GATEWAY_STATE_FILE,
    500u
};
static lv_timer_t *s_poll_timer = NULL;

static int json_get_double(const char *json, const char *key, double *value)
{
    char pattern[64];
    const char *position;
    const char *colon;
    char *end;

    if (json == NULL || key == NULL || value == NULL) {
        return -1;
    }

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    position = strstr(json, pattern);
    if (position == NULL) {
        return -1;
    }

    colon = strchr(position, ':');
    if (colon == NULL) {
        return -1;
    }

    errno = 0;
    *value = strtod(colon + 1, &end);
    if (errno != 0 || end == colon + 1) {
        return -1;
    }

    return 0;
}

static int json_get_uint(const char *json, const char *key, uint32_t *value)
{
    double parsed;

    if (value == NULL || json_get_double(json, key, &parsed) < 0 || parsed < 0.0) {
        return -1;
    }

    *value = (uint32_t)parsed;
    return 0;
}

static int json_get_string(const char *json,
                           const char *key,
                           char *value,
                           size_t value_size)
{
    char pattern[64];
    const char *position;
    const char *colon;
    const char *start;
    const char *end;
    size_t length;

    if (json == NULL || key == NULL || value == NULL || value_size == 0u) {
        return -1;
    }

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    position = strstr(json, pattern);
    if (position == NULL) {
        return -1;
    }

    colon = strchr(position, ':');
    if (colon == NULL) {
        return -1;
    }

    start = colon + 1;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
        ++start;
    }
    if (*start != '"') {
        return -1;
    }

    ++start;
    end = strchr(start, '"');
    if (end == NULL) {
        return -1;
    }

    length = (size_t)(end - start);
    if (length >= value_size) {
        length = value_size - 1u;
    }
    memcpy(value, start, length);
    value[length] = '\0';
    return 0;
}

static const char *json_find_object(const char *json, const char *key)
{
    char pattern[64];
    const char *position;
    const char *colon;

    if (json == NULL || key == NULL) {
        return NULL;
    }

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    position = strstr(json, pattern);
    if (position == NULL) {
        return NULL;
    }

    colon = strchr(position, ':');
    if (colon == NULL) {
        return NULL;
    }

    ++colon;
    while (*colon == ' ' || *colon == '\t' || *colon == '\r' || *colon == '\n') {
        ++colon;
    }

    return *colon == '{' ? colon : NULL;
}

static void gateway_state_set_unavailable(gateway_state_t *state, int parse_error)
{
    memset(state, 0, sizeof(*state));
    state->parse_error = parse_error ? 1u : 0u;
    strcpy(state->stm32_app_version, "0.0");
}

static int gateway_state_parse_json(const char *json, gateway_state_t *state)
{
    const char *payload_json;
    const char *stm32_json;
    uint32_t parsed_u;
    double parsed;
    int has_envelope;
    int parsed_fields = 0;

    if (json == NULL || state == NULL) {
        return -1;
    }

    gateway_state_set_unavailable(state, 0);
    payload_json = json_find_object(json, "payload");
    has_envelope = payload_json != NULL || strstr(json, "\"status\"") != NULL;
    if (payload_json == NULL) {
        payload_json = json;
    }

    state->gateway_online = 1u;
    state->tcp_connected = 1u;

    if (json_get_uint(json, "timestamp", &parsed_u) == 0) {
        state->timestamp = parsed_u;
        ++parsed_fields;
    }
    if (has_envelope && json_get_uint(json, "online", &parsed_u) == 0) {
        state->gateway_online = (uint8_t)parsed_u;
        ++parsed_fields;
    }
    if (has_envelope && json_get_uint(json, "connected", &parsed_u) == 0) {
        state->tcp_connected = (uint8_t)parsed_u;
        ++parsed_fields;
    }

    if (json_get_uint(payload_json, "timestamp", &parsed_u) == 0) {
        state->timestamp = parsed_u;
        ++parsed_fields;
    }
    if (json_get_uint(payload_json, "simulated", &parsed_u) == 0) {
        state->simulated = (uint8_t)parsed_u;
    }
    if (json_get_uint(payload_json, "ap3216c_ok", &parsed_u) == 0) {
        state->ap3216c_ok = (uint8_t)parsed_u;
        ++parsed_fields;
    }
    if (json_get_double(payload_json, "als", &parsed) == 0) {
        state->als = (int)parsed;
    }
    if (json_get_double(payload_json, "ir", &parsed) == 0) {
        state->ir = (int)parsed;
    }
    if (json_get_double(payload_json, "ps", &parsed) == 0) {
        state->ps = (int)parsed;
    }

    if (json_get_uint(payload_json, "icm20608_ok", &parsed_u) == 0) {
        state->icm20608_ok = (uint8_t)parsed_u;
        ++parsed_fields;
    }
    if (json_get_double(payload_json, "gyro_x", &parsed) == 0) {
        state->gyro_x = (float)parsed;
    }
    if (json_get_double(payload_json, "gyro_y", &parsed) == 0) {
        state->gyro_y = (float)parsed;
    }
    if (json_get_double(payload_json, "gyro_z", &parsed) == 0) {
        state->gyro_z = (float)parsed;
    }
    if (json_get_double(payload_json, "accel_x", &parsed) == 0) {
        state->accel_x = (float)parsed;
    }
    if (json_get_double(payload_json, "accel_y", &parsed) == 0) {
        state->accel_y = (float)parsed;
    }
    if (json_get_double(payload_json, "accel_z", &parsed) == 0) {
        state->accel_z = (float)parsed;
    }
    if (json_get_double(payload_json, "temp", &parsed) == 0) {
        state->icm_temperature = (float)parsed;
    }

    if (json_get_uint(payload_json, "stm32_can_ok", &parsed_u) == 0) {
        state->stm32_can_ok = (uint8_t)parsed_u;
        ++parsed_fields;
    }

    stm32_json = json_find_object(payload_json, "stm32_can");
    if (stm32_json == NULL) {
        stm32_json = json_find_object(json, "stm32_can");
    }
    if (stm32_json != NULL) {
        if (json_get_uint(stm32_json, "online", &parsed_u) == 0) {
            state->stm32_online = (uint8_t)parsed_u;
            ++parsed_fields;
        }
        if (json_get_uint(stm32_json, "dht_ok", &parsed_u) == 0) {
            state->dht11_ok = (uint8_t)parsed_u;
        }
        if (json_get_double(stm32_json, "temperature", &parsed) == 0) {
            state->dht11_temperature = (int)parsed;
        }
        if (json_get_double(stm32_json, "humidity", &parsed) == 0) {
            state->dht11_humidity = (int)parsed;
        }
        (void)json_get_string(stm32_json, "app_version",
                              state->stm32_app_version,
                              sizeof(state->stm32_app_version));
        if (json_get_uint(stm32_json, "counter", &parsed_u) == 0) {
            state->stm32_counter = parsed_u;
        }
        if (json_get_uint(stm32_json, "frames", &parsed_u) == 0) {
            state->stm32_frames = parsed_u;
        }
        if (json_get_uint(stm32_json, "checksum_errors", &parsed_u) == 0) {
            state->stm32_checksum_errors = parsed_u;
        }
        if (json_get_uint(stm32_json, "last_seen", &parsed_u) == 0) {
            state->stm32_last_seen = parsed_u;
        }
    }

    if (parsed_fields == 0) {
        gateway_state_set_unavailable(state, 1);
        return -1;
    }

    state->valid = 1u;
    if (!state->gateway_online || !state->tcp_connected) {
        state->ap3216c_ok = 0u;
        state->icm20608_ok = 0u;
    }
    return 0;
}

int gateway_state_poll_now(void)
{
    char json[4096];
    gateway_state_t next_state;
    struct stat state_file_stat;
    time_t now;
    FILE *state_file;
    size_t bytes_read;

    state_file = fopen(s_poll_context.path, "r");
    if (state_file == NULL) {
        gateway_state_set_unavailable(&s_gateway_state, 1);
        return -1;
    }

    bytes_read = fread(json, 1, sizeof(json) - 1u, state_file);
    fclose(state_file);
    if (bytes_read == 0u) {
        gateway_state_set_unavailable(&s_gateway_state, 1);
        return -1;
    }

    json[bytes_read] = '\0';
    if (gateway_state_parse_json(json, &next_state) < 0) {
        gateway_state_set_unavailable(&s_gateway_state, 1);
        return -1;
    }

    if (stat(s_poll_context.path, &state_file_stat) == 0) {
        time(&now);
        if (now >= state_file_stat.st_mtime) {
            next_state.state_age_seconds =
                (uint32_t)(now - state_file_stat.st_mtime);
            if (next_state.state_age_seconds > GATEWAY_STATE_STALE_SECONDS) {
                next_state.stale = 1u;
                next_state.gateway_online = 0u;
                next_state.tcp_connected = 0u;
            }
        }
    }

    s_gateway_state = next_state;
    return 0;
}

static void gateway_poll_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    (void)gateway_state_poll_now();
}

void gateway_state_init(const char *state_path, uint32_t poll_period_ms)
{
    if (state_path != NULL && state_path[0] != '\0') {
        strncpy(s_poll_context.path, state_path, sizeof(s_poll_context.path) - 1u);
        s_poll_context.path[sizeof(s_poll_context.path) - 1u] = '\0';
    }
    if (poll_period_ms == 0u) {
        poll_period_ms = 500u;
    }
    s_poll_context.poll_period_ms = poll_period_ms;

    (void)gateway_state_poll_now();
    if (s_poll_timer == NULL) {
        s_poll_timer = lv_timer_create(gateway_poll_timer_cb, poll_period_ms, NULL);
    } else {
        lv_timer_set_period(s_poll_timer, poll_period_ms);
    }
}

void gateway_state_deinit(void)
{
    if (s_poll_timer != NULL) {
        lv_timer_del(s_poll_timer);
        s_poll_timer = NULL;
    }
}

const gateway_state_t *gateway_state_get(void)
{
    return &s_gateway_state;
}

int gateway_state_imx6ull_online(const gateway_state_t *state)
{
    return state != NULL && state->valid && !state->parse_error && !state->stale &&
           state->gateway_online && state->tcp_connected;
}

int gateway_state_stm32_online(const gateway_state_t *state)
{
    return gateway_state_imx6ull_online(state) && state->stm32_online;
}
