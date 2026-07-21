#include "protocol.h"
#include "sensors.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_SERVER_IP "192.168.10.20"
#define DEFAULT_SERVER_PORT 5000
#define DEFAULT_INTERVAL_MS 1000
#define HEARTBEAT_SECONDS 3
#define CONNECT_TIMEOUT_MS 3000
#define SOCKET_TIMEOUT_SECONDS 8
#define SUMMARY_LOG_SECONDS 10
#define RECONNECT_MIN_MS 1000u
#define RECONNECT_MAX_MS 8000u
#define DEFAULT_AP3216C_DIR "/sys/class/misc/ap3216c"
#define DEFAULT_AP3216C_DEV "/dev/ap3216c"
#define DEFAULT_ICM20608_DEV "/dev/icm20608"
#define DEFAULT_STM32_CAN_STATE "/tmp/stm32_can_state.json"

typedef struct {
    int connected;
    uint32_t reconnect_count;
    uint32_t disconnect_count;
    time_t last_connected_time;
    const char *last_error;
} gateway_runtime_status_t;

static volatile sig_atomic_t g_running = 1;

static void handle_stop_signal(int signo)
{
    (void)signo;
    g_running = 0;
}

static void usage(const char *prog)
{
    printf("Usage: %s [-a t113_ip] [-p port] [-i interval_ms] [-A ap3216c_sysfs_dir] [-D ap3216c_dev] [-I icm20608_dev] [-M stm32_can_state_json] [-s] [-v]\n", prog);
    printf("Default: %s -a %s -p %d -i %d -A %s -D %s -I %s -M %s\n",
           prog,
           DEFAULT_SERVER_IP,
           DEFAULT_SERVER_PORT,
           DEFAULT_INTERVAL_MS,
           DEFAULT_AP3216C_DIR,
           DEFAULT_AP3216C_DEV,
           DEFAULT_ICM20608_DEV,
           DEFAULT_STM32_CAN_STATE);
    printf("  -s  use simulated sensor data, useful for VM TCP testing\n");
    printf("  -v  print every transmitted JSON frame\n");
}

static void sleep_ms(unsigned int ms)
{
    struct timespec req;
    req.tv_sec = ms / 1000u;
    req.tv_nsec = (long)(ms % 1000u) * 1000000L;

    while (nanosleep(&req, &req) < 0 && errno == EINTR && g_running) {
    }
}

static int build_json_payload(char *buf,
                              size_t size,
                              const sensor_snapshot_t *snapshot,
                              const gateway_runtime_status_t *runtime)
{
    int n = snprintf(buf, size,
                     "{"
                     "\"gateway\":\"imx6ull\","
                     "\"timestamp\":%ld,"
                     "\"simulated\":%d,"
                     "\"tcp_client\":{"
                     "\"connected\":%d,"
                     "\"reconnect_count\":%u,"
                     "\"disconnect_count\":%u,"
                     "\"last_connected_time\":%ld,"
                     "\"last_error\":\"%s\""
                     "},"
                     "\"ap3216c_ok\":%d,"
                     "\"ap3216c\":{"
                     "\"als\":%d,"
                     "\"ir\":%d,"
                     "\"ps\":%d"
                     "},"
                     "\"icm20608_ok\":%d,"
                     "\"icm20608\":{"
                     "\"gyro_x\":%.2f,"
                     "\"gyro_y\":%.2f,"
                     "\"gyro_z\":%.2f,"
                     "\"accel_x\":%.2f,"
                     "\"accel_y\":%.2f,"
                     "\"accel_z\":%.2f,"
                     "\"temp\":%.2f"
                     "},"
                     "\"stm32_can_ok\":%d,"
                     "\"stm32_can\":%s"
                     "}",
                     (long)time(NULL),
                     snapshot->simulated,
                     runtime->connected,
                     runtime->reconnect_count,
                     runtime->disconnect_count,
                     (long)runtime->last_connected_time,
                     runtime->last_error == NULL ? "" : runtime->last_error,
                     snapshot->ap3216c_ok,
                     snapshot->ap3216c.als,
                     snapshot->ap3216c.ir,
                     snapshot->ap3216c.ps,
                     snapshot->icm20608_ok,
                     snapshot->icm20608.gyro_x,
                     snapshot->icm20608.gyro_y,
                     snapshot->icm20608.gyro_z,
                     snapshot->icm20608.accel_x,
                     snapshot->icm20608.accel_y,
                     snapshot->icm20608.accel_z,
                     snapshot->icm20608.temp,
                     snapshot->stm32_can_ok,
                     snapshot->stm32_can_json);

    if (n < 0 || (size_t)n >= size) {
        return -1;
    }
    return n;
}

static void set_socket_options(int fd)
{
    int yes = 1;
    int keep_idle = 5;
    int keep_interval = 2;
    int keep_count = 3;
    struct timeval timeout;

    timeout.tv_sec = SOCKET_TIMEOUT_SECONDS;
    timeout.tv_usec = 0;

    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
#ifdef TCP_KEEPIDLE
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keep_idle, sizeof(keep_idle));
#endif
#ifdef TCP_KEEPINTVL
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &keep_interval, sizeof(keep_interval));
#endif
#ifdef TCP_KEEPCNT
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &keep_count, sizeof(keep_count));
#endif
}

static int connect_with_timeout(int fd, const struct sockaddr *addr,
                                socklen_t addr_len, int timeout_ms)
{
    struct pollfd pfd;
    int flags;
    int socket_error = 0;
    socklen_t error_len = sizeof(socket_error);
    int ret;

    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return -1;

    ret = connect(fd, addr, addr_len);
    if (ret < 0 && errno != EINPROGRESS)
        goto fail;

    if (ret < 0) {
        memset(&pfd, 0, sizeof(pfd));
        pfd.fd = fd;
        pfd.events = POLLOUT;
        do {
            ret = poll(&pfd, 1, timeout_ms);
        } while (ret < 0 && errno == EINTR);
        if (ret == 0) {
            errno = ETIMEDOUT;
            goto fail;
        }
        if (ret < 0 || getsockopt(fd, SOL_SOCKET, SO_ERROR,
                                  &socket_error, &error_len) < 0)
            goto fail;
        if (socket_error != 0) {
            errno = socket_error;
            goto fail;
        }
    }

    if (fcntl(fd, F_SETFL, flags) < 0)
        return -1;
    return 0;

fail:
    {
        int saved_errno = errno;
        (void)fcntl(fd, F_SETFL, flags);
        errno = saved_errno;
    }
    return -1;
}

static int connect_to_t113(const char *server_ip, unsigned short port)
{
    int fd;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    set_socket_options(fd);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "invalid T113 IP address: %s\n", server_ip);
        close(fd);
        return -1;
    }

    printf("Connecting to T113 %s:%u ...\n", server_ip, port);
    if (connect_with_timeout(fd, (struct sockaddr *)&addr, sizeof(addr),
                             CONNECT_TIMEOUT_MS) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }

    printf("Connected to T113 display server.\n");
    return fd;
}

int main(int argc, char *argv[])
{
    const char *server_ip = DEFAULT_SERVER_IP;
    sensor_config_t sensor_cfg = {
        DEFAULT_AP3216C_DIR,
        DEFAULT_AP3216C_DEV,
        DEFAULT_ICM20608_DEV,
        DEFAULT_STM32_CAN_STATE,
        0
    };
    unsigned short port = DEFAULT_SERVER_PORT;
    unsigned int interval_ms = DEFAULT_INTERVAL_MS;
    int opt;
    int fd = -1;
    uint32_t seq = 1;
    time_t last_heartbeat = 0;
    time_t last_summary_log = 0;
    time_t last_sensor_warning = 0;
    int verbose = 0;
    unsigned int reconnect_delay_ms = RECONNECT_MIN_MS;
    gateway_runtime_status_t runtime = {
        0,
        0,
        0,
        0,
        "init"
    };

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handle_stop_signal);
    signal(SIGTERM, handle_stop_signal);
    srand((unsigned int)(time(NULL) ^ getpid()));

    while ((opt = getopt(argc, argv, "a:p:i:A:D:I:M:svh")) != -1) {
        switch (opt) {
        case 'a':
            server_ip = optarg;
            break;
        case 'p':
            port = (unsigned short)atoi(optarg);
            break;
        case 'i':
            interval_ms = (unsigned int)atoi(optarg);
            break;
        case 'A':
            sensor_cfg.ap3216c_dir = optarg;
            break;
        case 'D':
            sensor_cfg.ap3216c_dev = optarg;
            break;
        case 'I':
            sensor_cfg.icm20608_dev = optarg;
            break;
        case 'M':
            sensor_cfg.stm32_can_state_path = optarg;
            break;
        case 's':
            sensor_cfg.simulate = 1;
            break;
        case 'v':
            verbose = 1;
            break;
        case 'h':
        default:
            usage(argv[0]);
            return opt == 'h' ? 0 : 1;
        }
    }

    while (g_running) {
        sensor_snapshot_t snapshot;
        char payload[MAX_PAYLOAD_SIZE];
        int payload_len;
        time_t now;

        if (fd < 0) {
            fd = connect_to_t113(server_ip, port);
            if (fd < 0) {
                runtime.connected = 0;
                runtime.reconnect_count++;
                runtime.last_error = "connect_failed";
                sleep_ms(reconnect_delay_ms);
                if (reconnect_delay_ms < RECONNECT_MAX_MS) {
                    reconnect_delay_ms *= 2u;
                    if (reconnect_delay_ms > RECONNECT_MAX_MS)
                        reconnect_delay_ms = RECONNECT_MAX_MS;
                }
                continue;
            }
            runtime.connected = 1;
            runtime.last_connected_time = time(NULL);
            runtime.last_error = "none";
            reconnect_delay_ms = RECONNECT_MIN_MS;
        }

        if (sensors_read_snapshot(&sensor_cfg, &snapshot) < 0) {
            now = time(NULL);
            if (verbose || now - last_sensor_warning >= SUMMARY_LOG_SECONDS) {
                fprintf(stderr,
                        "sensor read warning: ap3216c_ok=%d icm20608_ok=%d. "
                        "stm32_can_ok=%d. Check %s, %s, %s and %s\n",
                        snapshot.ap3216c_ok,
                        snapshot.icm20608_ok,
                        snapshot.stm32_can_ok,
                        sensor_cfg.ap3216c_dir,
                        sensor_cfg.ap3216c_dev,
                        sensor_cfg.icm20608_dev,
                        sensor_cfg.stm32_can_state_path);
                last_sensor_warning = now;
            }
        }

        payload_len = build_json_payload(payload, sizeof(payload), &snapshot, &runtime);
        if (payload_len < 0) {
            fprintf(stderr, "payload buffer is too small\n");
            sleep_ms(interval_ms);
            continue;
        }

        if (protocol_send_frame(fd, FRAME_TYPE_DATA, seq,
                                payload, (uint32_t)payload_len) < 0) {
            perror("send data frame");
            close(fd);
            fd = -1;
            runtime.connected = 0;
            runtime.disconnect_count++;
            runtime.last_error = "send_data_failed";
            sleep_ms(1000);
            continue;
        }
        seq++;

        now = time(NULL);
        if (verbose) {
            printf("Sent data: %s\n", payload);
        } else if (now - last_summary_log >= SUMMARY_LOG_SECONDS) {
            printf("Sent seq=%u bytes=%d AP3216C=%s ICM20608=%s STM32=%s\n",
                   seq - 1u,
                   payload_len,
                   snapshot.ap3216c_ok ? "ok" : "error",
                   snapshot.icm20608_ok ? "ok" : "error",
                   snapshot.stm32_can_ok ? "online" : "offline");
            last_summary_log = now;
        }
        if (now - last_heartbeat >= HEARTBEAT_SECONDS) {
            if (protocol_send_frame(fd, FRAME_TYPE_HEARTBEAT, seq++,
                                    NULL, 0) < 0) {
                perror("send heartbeat frame");
                close(fd);
                fd = -1;
                runtime.connected = 0;
                runtime.disconnect_count++;
                runtime.last_error = "send_heartbeat_failed";
                sleep_ms(1000);
                continue;
            }
            last_heartbeat = now;
        }

        sleep_ms(interval_ms);
    }

    if (fd >= 0) {
        close(fd);
    }
    printf("Gateway stopped: sent_seq=%u reconnects=%u disconnects=%u\n",
           seq - 1u, runtime.reconnect_count, runtime.disconnect_count);
    return 0;
}
