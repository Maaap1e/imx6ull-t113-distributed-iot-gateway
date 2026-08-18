#ifndef IMX6ULL_SENSOR_UAPI_H
#define IMX6ULL_SENSOR_UAPI_H

/*
 * Shared binary contract between the character drivers and userspace tools.
 * Values use the target CPU's native endianness; i.MX6ULL is little-endian.
 * Keep this header free of kernel-internal types so it can also be compiled by
 * the gateway application and host-side ABI tests.
 */
#ifdef __KERNEL__
#include <linux/types.h>
typedef __u16 sensor_uapi_u16;
typedef __s32 sensor_uapi_s32;
#else
#include <stdint.h>
typedef uint16_t sensor_uapi_u16;
typedef int32_t sensor_uapi_s32;
#endif

#define AP3216C_SAMPLE_ABI_SIZE 6u
#define ICM20608_SAMPLE_ABI_SIZE 28u

struct gateway_ap3216c_sample {
	sensor_uapi_u16 ir;
	sensor_uapi_u16 als;
	sensor_uapi_u16 ps;
};

struct gateway_icm20608_sample {
	sensor_uapi_s32 gyro_x;
	sensor_uapi_s32 gyro_y;
	sensor_uapi_s32 gyro_z;
	sensor_uapi_s32 accel_x;
	sensor_uapi_s32 accel_y;
	sensor_uapi_s32 accel_z;
	sensor_uapi_s32 temperature;
};

#endif
