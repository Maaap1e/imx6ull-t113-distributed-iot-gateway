#include "protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_BIND_IP "0.0.0.0"
#define DEFAULT_PORT 5000
#define LISTEN_BACKLOG 4
#define DEFAULT_LOG_PATH "/tmp/t113_tcp.log"
#define DEFAULT_PID_PATH "/tmp/t113_display_app.pid"
#define DEFAULT_STATE_PATH "/tmp/t113_sensor_state.json"
#define DEFAULT_CSV_PATH "/tmp/t113_sensor_data.csv"
#define SOCKET_TIMEOUT_SECONDS 8
#define STATE_WRITE_SECONDS 1
#define CSV_WRITE_SECONDS 5
#define SUMMARY_LOG_SECONDS 10

static volatile sig_atomic_t g_running = 1;
static int g_server_fd = -1;
static int g_client_fd = -1;
static const char *g_state_path = DEFAULT_STATE_PATH;
static const char *g_csv_path = DEFAULT_CSV_PATH;
static unsigned long g_tcp_rx_frames = 0;
static unsigned long g_tcp_crc_errors = 0;
static unsigned long g_tcp_protocol_errors = 0;
static unsigned long g_tcp_disconnects = 0;
static int g_verbose = 0;
static time_t g_last_state_write = 0;
static time_t g_last_csv_write = 0;
static time_t g_last_summary_log = 0;

static void usage(const char *prog)
{
    printf("Usage: %s [-a bind_ip] [-p port] [-d] [-v] [-l log_path] [-P pid_path] [-S state_path] [-C csv_path]\n", prog);
    printf("Default: %s -a %s -p %d\n", prog, DEFAULT_BIND_IP, DEFAULT_PORT);
}

static void handle_stop_signal(int signo)
{
    (void)signo;
    g_running = 0;
    if (g_server_fd >= 0) {
        close(g_server_fd);
        g_server_fd = -1;
    }
    if (g_client_fd >= 0) {
        close(g_client_fd);
        g_client_fd = -1;
    }
}

static int json_get_double(const char *json, const char *key, double *value)
{
    char pattern[64];
    char *pos;
    char *colon;
    char *end;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    pos = strstr(json, pattern);
    if (pos == NULL) {
        return -1;
    }

    colon = strchr(pos, ':');
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

static int json_get_uint(const char *json, const char *key, unsigned int *value)
{
    double v;

    if (json_get_double(json, key, &v) < 0) {
        return -1;
    }
    if (v < 0.0) {
        return -1;
    }

    *value = (unsigned int)v;
    return 0;
}

static void write_latest_state_file(const char *payload)
{
    char tmp_path[512];
    FILE *fp;
    int n;

    if (g_state_path == NULL || g_state_path[0] == '\0') {
        return;
    }

    n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_state_path);
    if (n < 0 || (size_t)n >= sizeof(tmp_path)) {
        fprintf(stderr, "state path is too long: %s\n", g_state_path);
        return;
    }

    fp = fopen(tmp_path, "w");
    if (fp == NULL) {
        perror("fopen state tmp");
        return;
    }

    fputs(payload, fp);
    fputc('\n', fp);

    if (fclose(fp) != 0) {
        perror("fclose state tmp");
        unlink(tmp_path);
        return;
    }

    if (rename(tmp_path, g_state_path) < 0) {
        perror("rename state file");
        unlink(tmp_path);
    }
}

static void write_gateway_status_state(int online, int connected, const char *status, const char *alert)
{
    char payload[1024];
    int n;

    if (status == NULL) {
        status = online ? "online" : "offline";
    }
    if (alert == NULL) {
        alert = "";
    }

    n = snprintf(payload, sizeof(payload),
                 "{"
                 "\"gateway\":\"imx6ull\","
                 "\"timestamp\":%ld,"
                 "\"online\":%d,"
                 "\"connected\":%d,"
                 "\"status\":\"%s\","
                 "\"alert\":\"%s\","
                 "\"tcp_server\":{"
                 "\"rx_frames\":%lu,"
                 "\"crc_errors\":%lu,"
                 "\"protocol_errors\":%lu,"
                 "\"disconnects\":%lu"
                 "},"
                 "\"ap3216c_ok\":0,"
                 "\"ap3216c\":{\"als\":0,\"ir\":0,\"ps\":0},"
                 "\"icm20608_ok\":0,"
                 "\"icm20608\":{"
                 "\"gyro_x\":0,"
                 "\"gyro_y\":0,"
                 "\"gyro_z\":0,"
                 "\"accel_x\":0,"
                 "\"accel_y\":0,"
                 "\"accel_z\":0,"
                 "\"temp\":0"
                 "},"
                 "\"stm32_can_ok\":0,"
                 "\"stm32_can\":{"
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
                 "}"
                 "}",
                 (long)time(NULL),
                 online,
                 connected,
                 status,
                 alert,
                 g_tcp_rx_frames,
                 g_tcp_crc_errors,
                 g_tcp_protocol_errors,
                 g_tcp_disconnects);
    if (n < 0 || (size_t)n >= sizeof(payload)) {
        fprintf(stderr, "gateway status payload is too long\n");
        return;
    }

    write_latest_state_file(payload);
}

static void write_online_sensor_state_file(const char *sensor_payload)
{
    char payload[MAX_PAYLOAD_SIZE + 512u];
    int n;

    if (sensor_payload == NULL || sensor_payload[0] != '{') {
        write_gateway_status_state(0, 1, "invalid_data", "invalid sensor payload");
        return;
    }

    n = snprintf(payload, sizeof(payload),
                 "{"
                 "\"gateway\":\"imx6ull\","
                 "\"timestamp\":%ld,"
                 "\"online\":1,"
                 "\"connected\":1,"
                 "\"status\":\"online\","
                 "\"tcp_server\":{"
                 "\"rx_frames\":%lu,"
                 "\"crc_errors\":%lu,"
                 "\"protocol_errors\":%lu,"
                 "\"disconnects\":%lu"
                 "},"
                 "\"payload\":%s"
                 "}",
                 (long)time(NULL),
                 g_tcp_rx_frames,
                 g_tcp_crc_errors,
                 g_tcp_protocol_errors,
                 g_tcp_disconnects,
                 sensor_payload);
    if (n < 0 || (size_t)n >= sizeof(payload)) {
        fprintf(stderr, "online sensor payload is too long, writing raw payload\n");
        write_latest_state_file(sensor_payload);
        return;
    }

    write_latest_state_file(payload);
}

static void append_csv_sample(long timestamp,
                              unsigned int ap3216c_ok,
                              double als,
                              double ir,
                              double ps,
                              unsigned int icm20608_ok,
                              double gyro_x,
                              double gyro_y,
                              double gyro_z,
                              double accel_x,
                              double accel_y,
                              double accel_z,
                              double temp,
                              unsigned int stm32_can_ok,
                              unsigned int stm32_online,
                              unsigned int stm32_dht_ok,
                              double stm32_temperature,
                              double stm32_humidity,
                              unsigned int stm32_counter,
                              unsigned int simulated)
{
    FILE *fp;
    int need_header = 0;
    struct stat st;

    if (g_csv_path == NULL || g_csv_path[0] == '\0') {
        return;
    }

    if (stat(g_csv_path, &st) < 0 || st.st_size == 0) {
        need_header = 1;
    }

    fp = fopen(g_csv_path, "a");
    if (fp == NULL) {
        perror("fopen csv");
        return;
    }

    if (need_header) {
        fprintf(fp,
                "timestamp,ap3216c_ok,als,ir,ps,icm20608_ok,"
                "gyro_x,gyro_y,gyro_z,accel_x,accel_y,accel_z,temp,"
                "stm32_can_ok,stm32_online,stm32_dht_ok,"
                "stm32_temperature,stm32_humidity,stm32_counter,simulated\n");
    }

    fprintf(fp,
            "%ld,%u,%.0f,%.0f,%.0f,%u,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,"
            "%u,%u,%u,%.0f,%.0f,%u,%u\n",
            timestamp,
            ap3216c_ok,
            als,
            ir,
            ps,
            icm20608_ok,
            gyro_x,
            gyro_y,
            gyro_z,
            accel_x,
            accel_y,
            accel_z,
            temp,
            stm32_can_ok,
            stm32_online,
            stm32_dht_ok,
            stm32_temperature,
            stm32_humidity,
            stm32_counter,
            simulated);
    fclose(fp);
}

static void display_update_from_payload(const char *payload)
{
    double timestamp_value = 0.0;
    double als = 0.0;
    double ir = 0.0;
    double ps = 0.0;
    double gyro_x = 0.0;
    double gyro_y = 0.0;
    double gyro_z = 0.0;
    double accel_x = 0.0;
    double accel_y = 0.0;
    double accel_z = 0.0;
    double temp = 0.0;
    double stm32_temperature = 0.0;
    double stm32_humidity = 0.0;
    unsigned int ap3216c_ok = 0;
    unsigned int icm20608_ok = 0;
    unsigned int stm32_can_ok = 0;
    unsigned int stm32_online = 0;
    unsigned int stm32_dht_ok = 0;
    unsigned int stm32_counter = 0;
    unsigned int simulated = 0;
    time_t now = time(NULL);

    /*
     * This is the display-layer boundary. Replace printf() with LVGL widget
     * updates, or let your LVGL process read DEFAULT_STATE_PATH.
     */
    if (now - g_last_state_write >= STATE_WRITE_SECONDS) {
        write_online_sensor_state_file(payload);
        g_last_state_write = now;
    }

    if (json_get_uint(payload, "ap3216c_ok", &ap3216c_ok) < 0 ||
        json_get_uint(payload, "icm20608_ok", &icm20608_ok) < 0 ||
        json_get_double(payload, "als", &als) < 0 ||
        json_get_double(payload, "ir", &ir) < 0 ||
        json_get_double(payload, "ps", &ps) < 0 ||
        json_get_double(payload, "gyro_x", &gyro_x) < 0 ||
        json_get_double(payload, "gyro_y", &gyro_y) < 0 ||
        json_get_double(payload, "gyro_z", &gyro_z) < 0 ||
        json_get_double(payload, "accel_x", &accel_x) < 0 ||
        json_get_double(payload, "accel_y", &accel_y) < 0 ||
        json_get_double(payload, "accel_z", &accel_z) < 0 ||
        json_get_double(payload, "temp", &temp) < 0) {
        printf("[UI] raw payload: %s\n", payload);
        return;
    }

    json_get_double(payload, "timestamp", &timestamp_value);
    json_get_uint(payload, "simulated", &simulated);
    json_get_uint(payload, "stm32_can_ok", &stm32_can_ok);
    json_get_uint(payload, "online", &stm32_online);
    json_get_uint(payload, "dht_ok", &stm32_dht_ok);
    json_get_double(payload, "temperature", &stm32_temperature);
    json_get_double(payload, "humidity", &stm32_humidity);
    json_get_uint(payload, "counter", &stm32_counter);

    if (now - g_last_csv_write >= CSV_WRITE_SECONDS) {
        append_csv_sample((long)timestamp_value,
                          ap3216c_ok,
                          als,
                          ir,
                          ps,
                          icm20608_ok,
                          gyro_x,
                          gyro_y,
                          gyro_z,
                          accel_x,
                          accel_y,
                          accel_z,
                          temp,
                          stm32_can_ok,
                          stm32_online,
                          stm32_dht_ok,
                          stm32_temperature,
                          stm32_humidity,
                          stm32_counter,
                          simulated);
        g_last_csv_write = now;
    }

    if (g_verbose || now - g_last_summary_log >= SUMMARY_LOG_SECONDS) {
        printf("[UI] AP3216C ok=%u als=%.0f ir=%.0f ps=%.0f | "
               "ICM20608 ok=%u temp=%.2f C | "
               "STM32 online=%u dht_ok=%u temp=%.0f C humidity=%.0f %% counter=%u%s\n",
               ap3216c_ok,
               als,
               ir,
               ps,
               icm20608_ok,
               temp,
               stm32_online,
               stm32_dht_ok,
               stm32_temperature,
               stm32_humidity,
               stm32_counter,
               simulated ? " [simulated]" : "");
        g_last_summary_log = now;
    }
}

static void set_socket_options(int fd, int connected_socket)
{
    int yes = 1;
    int keep_idle = 5;
    int keep_interval = 2;
    int keep_count = 3;
    struct timeval timeout;

    timeout.tv_sec = SOCKET_TIMEOUT_SECONDS;
    timeout.tv_usec = 0;

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (!connected_socket)
        return;

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

static int create_server_socket(const char *bind_ip, unsigned short port)
{
    int fd;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    set_socket_options(fd, 0);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, bind_ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "invalid bind IP address: %s\n", bind_ip);
        close(fd);
        return -1;
    }

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, LISTEN_BACKLOG) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}

static int daemonize_process(const char *log_path)
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid > 0) {
        exit(0);
    }

    if (setsid() < 0) {
        perror("setsid");
        return -1;
    }

    signal(SIGHUP, SIG_IGN);
    umask(0);

    if (freopen("/dev/null", "r", stdin) == NULL) {
        perror("freopen stdin");
        return -1;
    }
    if (freopen(log_path, "a", stdout) == NULL) {
        perror("freopen stdout");
        return -1;
    }
    if (freopen(log_path, "a", stderr) == NULL) {
        perror("freopen stderr");
        return -1;
    }

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    return 0;
}

static int write_pid_file(const char *pid_path)
{
    FILE *fp = fopen(pid_path, "w");

    if (fp == NULL) {
        perror("fopen pid file");
        return -1;
    }

    fprintf(fp, "%ld\n", (long)getpid());
    fclose(fp);
    return 0;
}

static void handle_client(int client_fd, const struct sockaddr_in *client_addr)
{
    uint8_t payload[MAX_PAYLOAD_SIZE + 1u];
    char ip[INET_ADDRSTRLEN];

    g_client_fd = client_fd;
    inet_ntop(AF_INET, &client_addr->sin_addr, ip, sizeof(ip));
    printf("Gateway connected: %s:%u\n", ip, ntohs(client_addr->sin_port));
    write_gateway_status_state(0, 1, "waiting_data", "gateway connected, waiting data");

    while (g_running) {
        frame_header_t header;
        int ret = protocol_recv_frame(client_fd, &header, payload, MAX_PAYLOAD_SIZE);

        if (ret == 0) {
            printf("Gateway disconnected.\n");
            g_tcp_disconnects++;
            break;
        }
        if (ret < 0) {
            fprintf(stderr, "receive frame failed, code=%d. Closing client.\n", ret);
            if (ret == -4) {
                g_tcp_crc_errors++;
            } else {
                g_tcp_protocol_errors++;
            }
            write_gateway_status_state(0, 0, "protocol_error", "receive frame failed");
            break;
        }

        payload[header.payload_len] = '\0';
        g_tcp_rx_frames++;

        if (g_verbose) {
            printf("Frame: type=%s seq=%u timestamp=%u payload_len=%u\n",
                   protocol_frame_type_name(header.type),
                   header.seq,
                   header.timestamp,
                   header.payload_len);
        }

        if (header.type == FRAME_TYPE_DATA) {
            display_update_from_payload((const char *)payload);
        } else if (header.type == FRAME_TYPE_HEARTBEAT) {
            if (g_verbose)
                printf("Heartbeat received.\n");
        } else {
            printf("Unknown frame type: %u\n", header.type);
        }
    }

    if (g_client_fd >= 0) {
        close(g_client_fd);
        g_client_fd = -1;
    }
    write_gateway_status_state(0, 0, "offline", "gateway disconnected");
}

int main(int argc, char *argv[])
{
    const char *bind_ip = DEFAULT_BIND_IP;
    const char *log_path = DEFAULT_LOG_PATH;
    const char *pid_path = DEFAULT_PID_PATH;
    unsigned short port = DEFAULT_PORT;
    int daemon_mode = 0;
    int opt;
    int server_fd;

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handle_stop_signal);
    signal(SIGTERM, handle_stop_signal);

    while ((opt = getopt(argc, argv, "a:p:dvl:P:S:C:h")) != -1) {
        switch (opt) {
        case 'a':
            bind_ip = optarg;
            break;
        case 'p':
            port = (unsigned short)atoi(optarg);
            break;
        case 'd':
            daemon_mode = 1;
            break;
        case 'v':
            g_verbose = 1;
            break;
        case 'l':
            log_path = optarg;
            break;
        case 'P':
            pid_path = optarg;
            break;
        case 'S':
            g_state_path = optarg;
            break;
        case 'C':
            g_csv_path = optarg;
            break;
        case 'h':
        default:
            usage(argv[0]);
            return opt == 'h' ? 0 : 1;
        }
    }

    if (daemon_mode && daemonize_process(log_path) < 0) {
        return 1;
    }

    if (write_pid_file(pid_path) < 0) {
        return 1;
    }

    server_fd = create_server_socket(bind_ip, port);
    if (server_fd < 0) {
        return 1;
    }
    g_server_fd = server_fd;

    printf("T113 display server listening on %s:%u\n", bind_ip, port);
    write_gateway_status_state(0, 0, "offline", "waiting gateway connection");

    while (g_running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);

        if (client_fd < 0) {
            if (errno == EINTR && g_running) {
                continue;
            }
            if (!g_running) {
                break;
            }
            perror("accept");
            break;
        }

        set_socket_options(client_fd, 1);
        handle_client(client_fd, &client_addr);
    }

    if (g_server_fd >= 0) {
        close(g_server_fd);
        g_server_fd = -1;
    }
    write_gateway_status_state(0, 0, "offline", "server stopped");
    unlink(pid_path);
    return 0;
}
