#include "sensors.h"

#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int read_int_from_file(const char *path, int *value)
{
    char buf[32];
    char *end = NULL;
    ssize_t n;
    long parsed;
    int fd;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    n = read(fd, buf, sizeof(buf) - 1u);
    close(fd);
    if (n <= 0) {
        return -1;
    }

    buf[n] = '\0';
    errno = 0;
    parsed = strtol(buf, &end, 10);
    if (errno != 0 || end == buf) {
        return -1;
    }

    *value = (int)parsed;
    return 0;
}

static void set_default_stm32_can_state(sensor_snapshot_t *snapshot)
{
    snprintf(snapshot->stm32_can_json,
             sizeof(snapshot->stm32_can_json),
             "{"
             "\"node\":\"stm32f103\","
             "\"online\":0,"
             "\"dht_ok\":0,"
             "\"temperature\":0,"
             "\"humidity\":0,"
             "\"app_version\":\"0.0\","
             "\"counter\":0,"
             "\"frames\":0,"
             "\"checksum_errors\":0,"
             "\"last_seen\":0"
             "}");
    snapshot->stm32_can_ok = 0;
}

static int read_stm32_can_state(const char *path, sensor_snapshot_t *snapshot)
{
    char buf[sizeof(snapshot->stm32_can_json)];
    char *start;
    ssize_t n;
    int fd;

    set_default_stm32_can_state(snapshot);

    if (path == NULL || path[0] == '\0') {
        return -1;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    n = read(fd, buf, sizeof(buf) - 1u);
    close(fd);
    if (n <= 0) {
        return -1;
    }

    buf[n] = '\0';
    while (n > 0 && isspace((unsigned char)buf[n - 1])) {
        buf[--n] = '\0';
    }

    start = buf;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        start++;
    }

    if (start[0] != '{' || n == 0) {
        return -1;
    }

    snprintf(snapshot->stm32_can_json,
             sizeof(snapshot->stm32_can_json),
             "%s",
             start);
    snapshot->stm32_can_ok = strstr(start, "\"online\":1") != NULL ? 1 : 0;
    return 0;
}

static int read_ap3216c_sysfs(const char *dir, ap3216c_data_t *data)
{
    char path[256];

    snprintf(path, sizeof(path), "%s/als", dir);
    if (read_int_from_file(path, &data->als) < 0) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s/ir", dir);
    if (read_int_from_file(path, &data->ir) < 0) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s/ps", dir);
    if (read_int_from_file(path, &data->ps) < 0) {
        return -1;
    }

    return 0;
}

static int read_ap3216c_dev(const char *dev, ap3216c_data_t *data)
{
    int fd;
    int ret;
    unsigned short raw[3];

    fd = open(dev, O_RDWR);
    if (fd < 0) {
        return -1;
    }

    memset(raw, 0, sizeof(raw));
    ret = (int)read(fd, raw, sizeof(raw));
    close(fd);

    /*
     * Common ALIENTEK AP3216C char drivers fill ir/als/ps and return 0.
     * Some drivers return sizeof(raw). Accept both forms.
     */
    if (ret != 0 && ret != (int)sizeof(raw)) {
        return -1;
    }

    data->ir = raw[0];
    data->als = raw[1];
    data->ps = raw[2];
    return 0;
}

static int read_icm20608(const char *dev, icm20608_data_t *data)
{
    int fd;
    int ret;
    signed int raw[7];

    fd = open(dev, O_RDWR);
    if (fd < 0) {
        return -1;
    }

    memset(raw, 0, sizeof(raw));
    ret = (int)read(fd, raw, sizeof(raw));
    close(fd);

    /*
     * The ALIENTEK sample driver returns 0 after filling the user buffer.
     * Some drivers return sizeof(raw). Accept both forms.
     */
    if (ret != 0 && ret != (int)sizeof(raw)) {
        return -1;
    }

    data->gyro_x = (float)raw[0] / 16.4f;
    data->gyro_y = (float)raw[1] / 16.4f;
    data->gyro_z = (float)raw[2] / 16.4f;
    data->accel_x = (float)raw[3] / 2048.0f;
    data->accel_y = (float)raw[4] / 2048.0f;
    data->accel_z = (float)raw[5] / 2048.0f;
    data->temp = ((float)raw[6] - 25.0f) / 326.8f + 25.0f;

    return 0;
}

static void fill_simulated_snapshot(sensor_snapshot_t *snapshot)
{
    int drift = rand() % 100;

    snapshot->ap3216c.als = 300 + drift;
    snapshot->ap3216c.ir = 80 + (drift / 2);
    snapshot->ap3216c.ps = 120 + (drift / 3);
    snapshot->ap3216c_ok = 1;

    snapshot->icm20608.gyro_x = ((float)(rand() % 200) - 100.0f) / 10.0f;
    snapshot->icm20608.gyro_y = ((float)(rand() % 200) - 100.0f) / 10.0f;
    snapshot->icm20608.gyro_z = ((float)(rand() % 200) - 100.0f) / 10.0f;
    snapshot->icm20608.accel_x = ((float)(rand() % 200) - 100.0f) / 100.0f;
    snapshot->icm20608.accel_y = ((float)(rand() % 200) - 100.0f) / 100.0f;
    snapshot->icm20608.accel_z = 1.0f + ((float)(rand() % 40) - 20.0f) / 100.0f;
    snapshot->icm20608.temp = 25.0f + ((float)(rand() % 80) / 10.0f);
    snapshot->icm20608_ok = 1;

    snprintf(snapshot->stm32_can_json,
             sizeof(snapshot->stm32_can_json),
             "{"
             "\"node\":\"stm32f103\","
             "\"online\":1,"
             "\"dht_ok\":1,"
             "\"temperature\":26,"
             "\"humidity\":55,"
             "\"app_version\":\"1.0\","
             "\"counter\":%d,"
             "\"frames\":%d,"
             "\"checksum_errors\":0,"
             "\"last_seen\":%ld"
             "}",
             rand() % 255,
             1000 + rand() % 1000,
             (long)time(NULL));
    snapshot->stm32_can_ok = 1;

    snapshot->simulated = 1;
}

void sensor_snapshot_init(sensor_snapshot_t *snapshot)
{
    memset(snapshot, 0, sizeof(*snapshot));
}

int sensors_read_snapshot(const sensor_config_t *cfg, sensor_snapshot_t *snapshot)
{
    sensor_snapshot_init(snapshot);

    if (cfg->simulate) {
        fill_simulated_snapshot(snapshot);
        return 0;
    }

    if (read_ap3216c_sysfs(cfg->ap3216c_dir, &snapshot->ap3216c) == 0 ||
        read_ap3216c_dev(cfg->ap3216c_dev, &snapshot->ap3216c) == 0) {
        snapshot->ap3216c_ok = 1;
    }

    if (read_icm20608(cfg->icm20608_dev, &snapshot->icm20608) == 0) {
        snapshot->icm20608_ok = 1;
    }

    read_stm32_can_state(cfg->stm32_can_state_path, snapshot);

    snapshot->simulated = 0;
    return snapshot->ap3216c_ok && snapshot->icm20608_ok ? 0 : -1;
}
