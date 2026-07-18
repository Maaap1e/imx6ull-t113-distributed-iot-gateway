#include <errno.h>
#include <fcntl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_CAN_IFACE       "can0"
#define DEFAULT_STATE_PATH      "/tmp/stm32_can_state.json"
#define DEFAULT_CSV_PATH        "/tmp/stm32_can_data.csv"

#define CAN_ID_STM32_HEARTBEAT  0x101u
#define CAN_ID_STM32_DHT11      0x102u
#define CAN_ID_IMX6ULL_CONTROL  0x201u
#define CAN_ID_STM32_ACK        0x202u

#define CTRL_CMD_LED0           0x01u
#define CTRL_CMD_LED1           0x02u
#define CTRL_CMD_ENTER_BOOT     0xA5u

#define STATE_WRITE_SECONDS     1
#define CSV_WRITE_SECONDS       5
#define SUMMARY_LOG_SECONDS     10

typedef struct {
    int online;
    int dht_ok;
    int temperature;
    int humidity;
    unsigned int app_major;
    unsigned int app_minor;
    unsigned int counter;
    unsigned long frames;
    unsigned long checksum_errors;
    time_t last_seen;
} stm32_state_t;

static volatile sig_atomic_t g_running = 1;
static int g_verbose = 0;
static time_t g_last_state_write = 0;
static time_t g_last_csv_write = 0;
static time_t g_last_summary_log = 0;

static void usage(const char *prog)
{
    printf("Usage: %s [-i can0] [-s state_json] [-c csv_path] [-L led:value] [-b] [-v]\n", prog);
    printf("  -L led:value  send LED control, e.g. -L 0:1 turns LED0 on\n");
    printf("  -b            ask STM32 App to reset into Bootloader\n");
    printf("  -v            print every heartbeat and DHT11 frame\n");
}

static void handle_signal(int signo)
{
    (void)signo;
    g_running = 0;
}

static uint8_t checksum8(const uint8_t *data, uint8_t len)
{
    uint8_t i;
    uint8_t sum = 0;

    for (i = 0; i < len; i++) {
        sum += data[i];
    }

    return sum;
}

static int open_can_socket(const char *iface)
{
    int fd;
    struct ifreq ifr;
    struct sockaddr_can addr;
    struct can_filter filters[] = {
        { CAN_ID_STM32_HEARTBEAT, CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG },
        { CAN_ID_STM32_DHT11, CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG },
        { CAN_ID_STM32_ACK, CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG }
    };

    fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd < 0) {
        perror("socket CAN");
        return -1;
    }

    if (setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER,
                   filters, sizeof(filters)) < 0) {
        perror("setsockopt CAN_RAW_FILTER");
        close(fd);
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", iface);
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl SIOCGIFINDEX");
        close(fd);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind CAN");
        close(fd);
        return -1;
    }

    return fd;
}

static int send_can_frame(int fd, uint32_t id, const uint8_t *data, uint8_t len)
{
    struct can_frame frame;

    memset(&frame, 0, sizeof(frame));
    frame.can_id = id;
    frame.can_dlc = len;
    if (data != NULL && len > 0) {
        memcpy(frame.data, data, len);
    }

    if (write(fd, &frame, sizeof(frame)) != (ssize_t)sizeof(frame)) {
        perror("write CAN");
        return -1;
    }

    return 0;
}

static int send_led_control(int fd, int led, int value)
{
    uint8_t data[8];

    memset(data, 0, sizeof(data));
    data[0] = led == 0 ? CTRL_CMD_LED0 : CTRL_CMD_LED1;
    data[1] = value ? 1 : 0;

    return send_can_frame(fd, CAN_ID_IMX6ULL_CONTROL, data, 8);
}

static int send_enter_bootloader(int fd)
{
    uint8_t data[8];

    memset(data, 0, sizeof(data));
    data[0] = CTRL_CMD_ENTER_BOOT;

    return send_can_frame(fd, CAN_ID_IMX6ULL_CONTROL, data, 8);
}

static void write_state_json(const char *path, const stm32_state_t *state)
{
    char tmp_path[512];
    FILE *fp;

    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    fp = fopen(tmp_path, "w");
    if (fp == NULL) {
        perror("fopen state tmp");
        return;
    }

    fprintf(fp,
            "{"
            "\"node\":\"stm32f103\","
            "\"online\":%d,"
            "\"dht_ok\":%d,"
            "\"temperature\":%d,"
            "\"humidity\":%d,"
            "\"app_version\":\"%u.%u\","
            "\"counter\":%u,"
            "\"frames\":%lu,"
            "\"checksum_errors\":%lu,"
            "\"last_seen\":%ld"
            "}\n",
            state->online,
            state->dht_ok,
            state->temperature,
            state->humidity,
            state->app_major,
            state->app_minor,
            state->counter,
            state->frames,
            state->checksum_errors,
            (long)state->last_seen);

    if (fclose(fp) != 0) {
        perror("fclose state tmp");
        unlink(tmp_path);
        return;
    }

    if (rename(tmp_path, path) < 0) {
        perror("rename state");
        unlink(tmp_path);
    }
}

static void append_csv(const char *path, const stm32_state_t *state)
{
    FILE *fp;
    struct stat st;
    int need_header = 0;

    if (stat(path, &st) < 0 || st.st_size == 0) {
        need_header = 1;
    }

    fp = fopen(path, "a");
    if (fp == NULL) {
        perror("fopen csv");
        return;
    }

    if (need_header) {
        fprintf(fp, "timestamp,online,dht_ok,temperature,humidity,app_version,counter,frames,checksum_errors\n");
    }

    fprintf(fp, "%ld,%d,%d,%d,%d,%u.%u,%u,%lu,%lu\n",
            (long)state->last_seen,
            state->online,
            state->dht_ok,
            state->temperature,
            state->humidity,
            state->app_major,
            state->app_minor,
            state->counter,
            state->frames,
            state->checksum_errors);
    fclose(fp);
}

static void publish_state_if_due(const char *state_path, const char *csv_path,
                                 const stm32_state_t *state, int has_sample)
{
    time_t now = time(NULL);

    if (now - g_last_state_write >= STATE_WRITE_SECONDS) {
        write_state_json(state_path, state);
        g_last_state_write = now;
    }
    if (has_sample && now - g_last_csv_write >= CSV_WRITE_SECONDS) {
        append_csv(csv_path, state);
        g_last_csv_write = now;
    }
}

static void handle_heartbeat(const struct can_frame *frame, stm32_state_t *state)
{
    if (frame->can_dlc < 8 || checksum8(frame->data, 7) != frame->data[7]) {
        state->checksum_errors++;
        return;
    }

    state->online = 1;
    state->app_major = frame->data[0];
    state->app_minor = frame->data[1];
    state->counter = (unsigned int)frame->data[2] | ((unsigned int)frame->data[3] << 8);
    state->last_seen = time(NULL);
    state->frames++;

    if (g_verbose) {
        printf("[heartbeat] version=%u.%u counter=%u\n",
               state->app_major,
               state->app_minor,
               state->counter);
    }
}

static void handle_dht11(const struct can_frame *frame, stm32_state_t *state)
{
    if (frame->can_dlc < 8 || checksum8(frame->data, 7) != frame->data[7]) {
        state->checksum_errors++;
        if (g_verbose || state->checksum_errors == 1 ||
            (state->checksum_errors % 100u) == 0u)
            printf("[dht11] checksum error, total=%lu\n", state->checksum_errors);
        return;
    }

    state->online = 1;
    state->temperature = frame->data[0];
    state->humidity = frame->data[2];
    state->dht_ok = frame->data[4] ? 1 : 0;
    state->app_major = frame->data[5];
    state->counter = frame->data[6];
    state->last_seen = time(NULL);
    state->frames++;

    if (g_verbose || state->last_seen - g_last_summary_log >= SUMMARY_LOG_SECONDS) {
        printf("[dht11] temp=%d C humidity=%d %% ok=%d version=%u.%u counter=%u frames=%lu\n",
               state->temperature,
               state->humidity,
               state->dht_ok,
               state->app_major,
               state->app_minor,
               state->counter,
               state->frames);
        g_last_summary_log = state->last_seen;
    }
}

static void handle_ack(const struct can_frame *frame)
{
    if (frame->can_dlc < 8) {
        return;
    }

    printf("[ack] cmd=0x%02X result=%u app=%u.%u\n",
           frame->data[0],
           frame->data[1],
           frame->data[2],
           frame->data[3]);
}

int main(int argc, char *argv[])
{
    const char *iface = DEFAULT_CAN_IFACE;
    const char *state_path = DEFAULT_STATE_PATH;
    const char *csv_path = DEFAULT_CSV_PATH;
    int fd;
    int opt;
    int boot_request = 0;
    int led_control = -1;
    int led_value = 0;
    stm32_state_t state;

    while ((opt = getopt(argc, argv, "i:s:c:L:bvh")) != -1) {
        switch (opt) {
        case 'i':
            iface = optarg;
            break;
        case 's':
            state_path = optarg;
            break;
        case 'c':
            csv_path = optarg;
            break;
        case 'L':
            if (sscanf(optarg, "%d:%d", &led_control, &led_value) != 2) {
                usage(argv[0]);
                return 1;
            }
            break;
        case 'b':
            boot_request = 1;
            break;
        case 'v':
            g_verbose = 1;
            break;
        case 'h':
        default:
            usage(argv[0]);
            return opt == 'h' ? 0 : 1;
        }
    }

    memset(&state, 0, sizeof(state));
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    setvbuf(stdout, NULL, _IONBF, 0);

    fd = open_can_socket(iface);
    if (fd < 0) {
        return 1;
    }

    printf("listening on %s, state=%s, csv=%s\n", iface, state_path, csv_path);
    write_state_json(state_path, &state);
    g_last_state_write = time(NULL);

    if (led_control == 0 || led_control == 1) {
        if (send_led_control(fd, led_control, led_value) < 0) {
            close(fd);
            return 1;
        }
    }

    if (boot_request) {
        if (send_enter_bootloader(fd) < 0) {
            close(fd);
            return 1;
        }
    }

    while (g_running) {
        fd_set rfds;
        struct timeval tv;
        struct can_frame frame;
        ssize_t n;
        uint32_t id;
        time_t now;
        int ret;

        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        ret = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("select CAN");
            break;
        }

        now = time(NULL);
        if (state.last_seen > 0 && now - state.last_seen > 3 && state.online) {
            state.online = 0;
            write_state_json(state_path, &state);
            printf("[state] STM32 offline\n");
        }

        if (ret == 0) {
            continue;
        }

        n = read(fd, &frame, sizeof(frame));

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("read CAN");
            break;
        }
        if ((size_t)n != sizeof(frame)) {
            continue;
        }
        if ((frame.can_id & CAN_EFF_FLAG) || (frame.can_id & CAN_RTR_FLAG)) {
            continue;
        }

        id = frame.can_id & CAN_SFF_MASK;
        if (id == CAN_ID_STM32_HEARTBEAT) {
            handle_heartbeat(&frame, &state);
            publish_state_if_due(state_path, csv_path, &state, 0);
        } else if (id == CAN_ID_STM32_DHT11) {
            handle_dht11(&frame, &state);
            publish_state_if_due(state_path, csv_path, &state, 1);
        } else if (id == CAN_ID_STM32_ACK) {
            handle_ack(&frame);
        }
    }

    close(fd);
    return 0;
}
