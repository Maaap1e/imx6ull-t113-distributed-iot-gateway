#ifndef IMX6ULL_SENSORS_H
#define IMX6ULL_SENSORS_H

typedef struct {
    int als;
    int ir;
    int ps;
} ap3216c_data_t;

typedef struct {
    float gyro_x;
    float gyro_y;
    float gyro_z;
    float accel_x;
    float accel_y;
    float accel_z;
    float temp;
} icm20608_data_t;

typedef struct {
    const char *ap3216c_dir;
    const char *ap3216c_dev;
    const char *icm20608_dev;
    const char *stm32_can_state_path;
    int simulate;
} sensor_config_t;

typedef struct {
    ap3216c_data_t ap3216c;
    icm20608_data_t icm20608;
    char stm32_can_json[512];
    int ap3216c_ok;
    int icm20608_ok;
    int stm32_can_ok;
    int simulated;
} sensor_snapshot_t;

void sensor_snapshot_init(sensor_snapshot_t *snapshot);
int sensors_read_snapshot(const sensor_config_t *cfg, sensor_snapshot_t *snapshot);

#endif
