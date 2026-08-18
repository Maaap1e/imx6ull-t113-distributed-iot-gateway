#define _POSIX_C_SOURCE 200809L

#include "sensor_uapi.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_AP3216C_DEVICE "/dev/ap3216c"
#define DEFAULT_ICM20608_DEVICE "/dev/icm20608"
#define DEFAULT_SAMPLE_COUNT 5u
#define DEFAULT_INTERVAL_MS 500u

static void print_usage(const char *program)
{
	fprintf(stderr,
		"Usage: %s [-a ap3216c_dev|-] [-i icm20608_dev|-] "
		"[-n count] [-d interval_ms]\n"
		"  Use '-' as a device path to skip that sensor.\n",
		program);
}

static int parse_unsigned(const char *text, unsigned int maximum,
			  unsigned int *value)
{
	char *end = NULL;
	unsigned long parsed;

	errno = 0;
	parsed = strtoul(text, &end, 10);
	if (errno != 0 || end == text || *end != '\0' || parsed > maximum)
		return -1;

	*value = (unsigned int)parsed;
	return 0;
}

static int sleep_ms(unsigned int milliseconds)
{
	struct timespec request;

	request.tv_sec = milliseconds / 1000u;
	request.tv_nsec = (long)(milliseconds % 1000u) * 1000000L;
	while (nanosleep(&request, &request) < 0) {
		if (errno != EINTR)
			return -1;
	}
	return 0;
}

static int open_sensor(const char *path)
{
	int fd = open(path, O_RDONLY);

	if (fd < 0)
		fprintf(stderr, "open %s failed: %s\n", path, strerror(errno));
	return fd;
}

static int read_sample(int fd, const char *path, void *sample, size_t size,
		       int *legacy_zero_return)
{
	ssize_t received;

	memset(sample, 0, size);
	errno = 0;
	received = read(fd, sample, size);
	if (received == (ssize_t)size)
		return 0;
	if (received == 0) {
		/* ALIENTEK's original teaching driver copied data but returned zero. */
		*legacy_zero_return = 1;
		return 0;
	}
	if (received < 0)
		fprintf(stderr, "read %s failed: %s\n", path, strerror(errno));
	else
		fprintf(stderr, "read %s returned %zd bytes, expected %zu\n",
			path, received, size);
	return -1;
}

static void print_ap3216c(unsigned int index,
			 const struct gateway_ap3216c_sample *sample)
{
	printf("sample=%u ap3216c ir=%u als=%u ps=%u\n",
	       index,
	       (unsigned int)sample->ir,
	       (unsigned int)sample->als,
	       (unsigned int)sample->ps);
}

static void print_icm20608(unsigned int index,
			 const struct gateway_icm20608_sample *sample)
{
	printf("sample=%u icm20608 "
	       "gyro_raw=%" PRId32 ",%" PRId32 ",%" PRId32 " "
	       "gyro_dps=%.3f,%.3f,%.3f "
	       "accel_raw=%" PRId32 ",%" PRId32 ",%" PRId32 " "
	       "accel_g=%.3f,%.3f,%.3f temp_raw=%" PRId32 " temp_c=%.2f\n",
	       index,
	       sample->gyro_x, sample->gyro_y, sample->gyro_z,
	       (double)sample->gyro_x / 16.4,
	       (double)sample->gyro_y / 16.4,
	       (double)sample->gyro_z / 16.4,
	       sample->accel_x, sample->accel_y, sample->accel_z,
	       (double)sample->accel_x / 2048.0,
	       (double)sample->accel_y / 2048.0,
	       (double)sample->accel_z / 2048.0,
	       sample->temperature,
	       (double)sample->temperature / 326.8 + 25.0);
}

int main(int argc, char **argv)
{
	const char *ap3216c_path = DEFAULT_AP3216C_DEVICE;
	const char *icm20608_path = DEFAULT_ICM20608_DEVICE;
	unsigned int sample_count = DEFAULT_SAMPLE_COUNT;
	unsigned int interval_ms = DEFAULT_INTERVAL_MS;
	struct gateway_ap3216c_sample ap3216c;
	struct gateway_icm20608_sample icm20608;
	int ap3216c_legacy = 0;
	int icm20608_legacy = 0;
	int ap3216c_fd = -1;
	int icm20608_fd = -1;
	unsigned int index;
	int option;
	int status = EXIT_FAILURE;

	while ((option = getopt(argc, argv, "a:i:n:d:h")) != -1) {
		switch (option) {
		case 'a':
			ap3216c_path = optarg;
			break;
		case 'i':
			icm20608_path = optarg;
			break;
		case 'n':
			if (parse_unsigned(optarg, 100000u, &sample_count) < 0 ||
			    sample_count == 0u) {
				fprintf(stderr, "invalid sample count: %s\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 'd':
			if (parse_unsigned(optarg, 60000u, &interval_ms) < 0) {
				fprintf(stderr, "invalid interval: %s\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 'h':
			print_usage(argv[0]);
			return EXIT_SUCCESS;
		default:
			print_usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (optind != argc) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}
	if (strcmp(ap3216c_path, "-") == 0 &&
	    strcmp(icm20608_path, "-") == 0) {
		fprintf(stderr, "both sensors are disabled\n");
		return EXIT_FAILURE;
	}

	if (strcmp(ap3216c_path, "-") != 0) {
		ap3216c_fd = open_sensor(ap3216c_path);
		if (ap3216c_fd < 0)
			goto out;
	}
	if (strcmp(icm20608_path, "-") != 0) {
		icm20608_fd = open_sensor(icm20608_path);
		if (icm20608_fd < 0)
			goto out;
	}

	/* Also gives the legacy AP3216C open-time reset one conversion interval. */
	if (sleep_ms(120u) < 0) {
		fprintf(stderr, "initial delay failed: %s\n", strerror(errno));
		goto out;
	}

	for (index = 1u; index <= sample_count; ++index) {
		if (ap3216c_fd >= 0) {
			if (read_sample(ap3216c_fd, ap3216c_path, &ap3216c,
					sizeof(ap3216c), &ap3216c_legacy) < 0)
				goto out;
			print_ap3216c(index, &ap3216c);
		}
		if (icm20608_fd >= 0) {
			if (read_sample(icm20608_fd, icm20608_path, &icm20608,
					sizeof(icm20608), &icm20608_legacy) < 0)
				goto out;
			print_icm20608(index, &icm20608);
		}

		if (index < sample_count && sleep_ms(interval_ms) < 0) {
			fprintf(stderr, "sample delay failed: %s\n", strerror(errno));
			goto out;
		}
	}

	if (ap3216c_legacy || icm20608_legacy)
		fprintf(stderr,
			"note: accepted legacy driver read() success return value 0\n");
	status = EXIT_SUCCESS;

out:
	if (icm20608_fd >= 0)
		close(icm20608_fd);
	if (ap3216c_fd >= 0)
		close(ap3216c_fd);
	return status;
}
