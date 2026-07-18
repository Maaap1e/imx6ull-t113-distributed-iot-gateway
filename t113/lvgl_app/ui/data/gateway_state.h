#ifndef _GATEWAY_STATE_H_
#define _GATEWAY_STATE_H_

#include <stdint.h>

#define GATEWAY_STATE_FILE "/tmp/t113_sensor_state.json"
#define GATEWAY_STATE_STALE_SECONDS 3u

typedef struct {
    uint8_t valid;
    uint8_t parse_error;
    uint8_t stale;
    uint32_t state_age_seconds;
    uint32_t timestamp;
    uint8_t simulated;

    uint8_t gateway_online;
    uint8_t tcp_connected;

    uint8_t ap3216c_ok;
    int als;
    int ir;
    int ps;

    uint8_t icm20608_ok;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    float accel_x;
    float accel_y;
    float accel_z;
    float icm_temperature;

    uint8_t stm32_can_ok;
    uint8_t stm32_online;
    uint8_t dht11_ok;
    int dht11_temperature;
    int dht11_humidity;
    char stm32_app_version[16];
    uint32_t stm32_counter;
    uint32_t stm32_frames;
    uint32_t stm32_checksum_errors;
    uint32_t stm32_last_seen;
} gateway_state_t;

void gateway_state_init(const char *state_path, uint32_t poll_period_ms);
void gateway_state_deinit(void);
int gateway_state_poll_now(void);
const gateway_state_t *gateway_state_get(void);
int gateway_state_imx6ull_online(const gateway_state_t *state);
int gateway_state_stm32_online(const gateway_state_t *state);

#endif
