/*
 * ICM20608 character-device adapter for the i.MX6ULL gateway.
 *
 * Derived from an ALIENTEK Linux teaching/BSP example supplied with the
 * project.  The SPI transfer, lifetime and userspace error handling below were
 * hardened for this repository.  See THIRD_PARTY_NOTICES.md and
 * docs/PROJECT_OWNERSHIP.md for the contribution boundary.
 */

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>

#include "sensor_uapi.h"

#define ICM20608_NAME                "icm20608"
#define ICM20608_READ_BIT            0x80
#define ICM20608_WHO_AM_I_VALUE      0xaf

#define ICM20608_SMPLRT_DIV          0x19
#define ICM20608_CONFIG              0x1a
#define ICM20608_GYRO_CONFIG         0x1b
#define ICM20608_ACCEL_CONFIG        0x1c
#define ICM20608_ACCEL_CONFIG2       0x1d
#define ICM20608_LP_MODE_CFG         0x1e
#define ICM20608_FIFO_EN             0x23
#define ICM20608_ACCEL_XOUT_H        0x3b
#define ICM20608_PWR_MGMT_1          0x6b
#define ICM20608_PWR_MGMT_2          0x6c
#define ICM20608_WHO_AM_I            0x75

struct icm20608_dev {
	struct spi_device *spi;
	struct mutex lock;
	dev_t devt;
	struct cdev cdev;
	struct class *class;
	struct device *device;
};

static int icm20608_read_regs(struct icm20608_dev *sensor, u8 reg,
			       u8 *data, size_t len)
{
	u8 command = reg | ICM20608_READ_BIT;
	int ret;

	/*
	 * Two transfers in one SPI message keep chip select asserted while the
	 * command byte is followed by the register data.  This also avoids the
	 * original len+1 transfer from a one-byte stack TX buffer.
	 */
	ret = spi_write_then_read(sensor->spi, &command, 1, data, len);
	if (ret)
		dev_err_ratelimited(&sensor->spi->dev,
				    "register read failed: reg=0x%02x len=%zu err=%d\n",
				    reg, len, ret);
	return ret;
}

static int icm20608_write_reg(struct icm20608_dev *sensor, u8 reg, u8 value)
{
	u8 data[2] = { reg & ~ICM20608_READ_BIT, value };
	int ret;

	ret = spi_write(sensor->spi, data, sizeof(data));
	if (ret)
		dev_err(&sensor->spi->dev,
			"register write failed: reg=0x%02x err=%d\n", reg, ret);
	return ret;
}

static int icm20608_hw_init(struct icm20608_dev *sensor)
{
	static const struct {
		u8 reg;
		u8 value;
	} settings[] = {
		{ ICM20608_SMPLRT_DIV,    0x00 },
		{ ICM20608_GYRO_CONFIG,   0x18 }, /* +/-2000 dps */
		{ ICM20608_ACCEL_CONFIG,  0x18 }, /* +/-16 g */
		{ ICM20608_CONFIG,        0x04 }, /* gyro DLPF */
		{ ICM20608_ACCEL_CONFIG2, 0x04 }, /* accel DLPF */
		{ ICM20608_PWR_MGMT_2,    0x00 },
		{ ICM20608_LP_MODE_CFG,   0x00 },
		{ ICM20608_FIFO_EN,       0x00 },
	};
	u8 who_am_i;
	unsigned int i;
	int ret;

	ret = icm20608_write_reg(sensor, ICM20608_PWR_MGMT_1, 0x80);
	if (ret)
		return ret;
	msleep(50);

	ret = icm20608_write_reg(sensor, ICM20608_PWR_MGMT_1, 0x01);
	if (ret)
		return ret;
	msleep(50);

	ret = icm20608_read_regs(sensor, ICM20608_WHO_AM_I, &who_am_i, 1);
	if (ret)
		return ret;
	if (who_am_i != ICM20608_WHO_AM_I_VALUE) {
		dev_err(&sensor->spi->dev,
			"WHO_AM_I mismatch: expected 0x%02x, got 0x%02x\n",
			ICM20608_WHO_AM_I_VALUE, who_am_i);
		return -ENODEV;
	}

	for (i = 0; i < ARRAY_SIZE(settings); ++i) {
		ret = icm20608_write_reg(sensor, settings[i].reg,
					 settings[i].value);
		if (ret)
			return ret;
	}

	return 0;
}

static sensor_uapi_s32 icm20608_be16_to_s32(u8 high, u8 low)
{
	return (__s16)(((u16)high << 8) | low);
}

static int icm20608_read_sample(struct icm20608_dev *sensor,
				 struct gateway_icm20608_sample *sample)
{
	u8 data[14];
	int ret;

	ret = icm20608_read_regs(sensor, ICM20608_ACCEL_XOUT_H, data,
				 ARRAY_SIZE(data));
	if (ret)
		return ret;

	sample->accel_x = icm20608_be16_to_s32(data[0], data[1]);
	sample->accel_y = icm20608_be16_to_s32(data[2], data[3]);
	sample->accel_z = icm20608_be16_to_s32(data[4], data[5]);
	sample->temperature = icm20608_be16_to_s32(data[6], data[7]);
	sample->gyro_x = icm20608_be16_to_s32(data[8], data[9]);
	sample->gyro_y = icm20608_be16_to_s32(data[10], data[11]);
	sample->gyro_z = icm20608_be16_to_s32(data[12], data[13]);
	return 0;
}

static int icm20608_open(struct inode *inode, struct file *file)
{
	struct icm20608_dev *sensor;

	sensor = container_of(inode->i_cdev, struct icm20608_dev, cdev);
	file->private_data = sensor;
	return 0;
}

static ssize_t icm20608_read(struct file *file, char __user *buffer,
			      size_t count, loff_t *offset)
{
	struct icm20608_dev *sensor = file->private_data;
	struct gateway_icm20608_sample sample;
	int ret;

	if (count < sizeof(sample))
		return -EINVAL;

	ret = mutex_lock_interruptible(&sensor->lock);
	if (ret)
		return ret;
	ret = icm20608_read_sample(sensor, &sample);
	mutex_unlock(&sensor->lock);
	if (ret)
		return ret;

	if (copy_to_user(buffer, &sample, sizeof(sample)))
		return -EFAULT;

	return sizeof(sample);
}

static const struct file_operations icm20608_fops = {
	.owner = THIS_MODULE,
	.open = icm20608_open,
	.read = icm20608_read,
	.llseek = no_llseek,
};

static int icm20608_register_chrdev(struct icm20608_dev *sensor)
{
	int ret;

	ret = alloc_chrdev_region(&sensor->devt, 0, 1, ICM20608_NAME);
	if (ret)
		return ret;

	cdev_init(&sensor->cdev, &icm20608_fops);
	sensor->cdev.owner = THIS_MODULE;
	ret = cdev_add(&sensor->cdev, sensor->devt, 1);
	if (ret)
		goto err_unregister;

	sensor->class = class_create(THIS_MODULE, ICM20608_NAME);
	if (IS_ERR(sensor->class)) {
		ret = PTR_ERR(sensor->class);
		sensor->class = NULL;
		goto err_cdev;
	}

	sensor->device = device_create(sensor->class, &sensor->spi->dev,
				       sensor->devt, sensor, ICM20608_NAME);
	if (IS_ERR(sensor->device)) {
		ret = PTR_ERR(sensor->device);
		sensor->device = NULL;
		goto err_class;
	}

	return 0;

err_class:
	class_destroy(sensor->class);
err_cdev:
	cdev_del(&sensor->cdev);
err_unregister:
	unregister_chrdev_region(sensor->devt, 1);
	return ret;
}

static void icm20608_unregister_chrdev(struct icm20608_dev *sensor)
{
	device_destroy(sensor->class, sensor->devt);
	class_destroy(sensor->class);
	cdev_del(&sensor->cdev);
	unregister_chrdev_region(sensor->devt, 1);
}

static int icm20608_probe(struct spi_device *spi)
{
	struct icm20608_dev *sensor;
	int ret;

	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;
	ret = spi_setup(spi);
	if (ret)
		return ret;

	sensor = devm_kzalloc(&spi->dev, sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;

	sensor->spi = spi;
	mutex_init(&sensor->lock);
	spi_set_drvdata(spi, sensor);

	ret = icm20608_hw_init(sensor);
	if (ret)
		return ret;

	ret = icm20608_register_chrdev(sensor);
	if (ret)
		return ret;

	dev_info(&spi->dev, "WHO_AM_I=0x%02x, registered /dev/%s\n",
		 ICM20608_WHO_AM_I_VALUE, ICM20608_NAME);
	return 0;
}

static int icm20608_remove(struct spi_device *spi)
{
	struct icm20608_dev *sensor = spi_get_drvdata(spi);

	icm20608_unregister_chrdev(sensor);
	return 0;
}

static const struct spi_device_id icm20608_ids[] = {
	{ "icm20608", 0 },
	{ "alientek,icm20608", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, icm20608_ids);

static const struct of_device_id icm20608_of_match[] = {
	{ .compatible = "alientek,icm20608" },
	{ }
};
MODULE_DEVICE_TABLE(of, icm20608_of_match);

static struct spi_driver icm20608_driver = {
	.driver = {
		.name = ICM20608_NAME,
		.of_match_table = icm20608_of_match,
	},
	.probe = icm20608_probe,
	.remove = icm20608_remove,
	.id_table = icm20608_ids,
};

module_spi_driver(icm20608_driver);

MODULE_AUTHOR("Maaap1e (project adaptation)");
MODULE_DESCRIPTION("ICM20608 character-device adapter for i.MX6ULL");
MODULE_LICENSE("GPL");
