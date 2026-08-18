#include "../linux/kernel_drivers/imx6ull_sensors/sensor_uapi.h"

#include <stddef.h>
#include <stdio.h>

#define CHECK(condition)                                                     \
	do {                                                                   \
		if (!(condition)) {                                              \
			fprintf(stderr, "CHECK failed at %s:%d: %s\n",           \
				__FILE__, __LINE__, #condition);                    \
			return 1;                                                  \
		}                                                              \
	} while (0)

int main(void)
{
	CHECK(sizeof(struct gateway_ap3216c_sample) ==
	      AP3216C_SAMPLE_ABI_SIZE);
	CHECK(offsetof(struct gateway_ap3216c_sample, ir) == 0u);
	CHECK(offsetof(struct gateway_ap3216c_sample, als) == 2u);
	CHECK(offsetof(struct gateway_ap3216c_sample, ps) == 4u);

	CHECK(sizeof(struct gateway_icm20608_sample) ==
	      ICM20608_SAMPLE_ABI_SIZE);
	CHECK(offsetof(struct gateway_icm20608_sample, gyro_x) == 0u);
	CHECK(offsetof(struct gateway_icm20608_sample, gyro_y) == 4u);
	CHECK(offsetof(struct gateway_icm20608_sample, gyro_z) == 8u);
	CHECK(offsetof(struct gateway_icm20608_sample, accel_x) == 12u);
	CHECK(offsetof(struct gateway_icm20608_sample, accel_y) == 16u);
	CHECK(offsetof(struct gateway_icm20608_sample, accel_z) == 20u);
	CHECK(offsetof(struct gateway_icm20608_sample, temperature) == 24u);

	puts("sensor UAPI layout tests passed");
	return 0;
}
