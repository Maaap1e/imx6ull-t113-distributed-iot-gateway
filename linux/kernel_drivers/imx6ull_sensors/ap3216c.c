/*
 * AP3216C character-device adapter for the i.MX6ULL gateway.
 *
 * Derived from an ALIENTEK Linux teaching/BSP example supplied with the
 * project.  The bus, lifetime and userspace error handling below were
 * hardened for this repository.  See THIRD_PARTY_NOTICES.md and
 * docs/PROJECT_OWNERSHIP.md for the contribution boundary.
 */

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "sensor_uapi.h"

#define AP3216C_NAME                 "ap3216c"
#define AP3216C_SYSTEM_CONFIG        0x00
#define AP3216C_IR_DATA_LOW          0x0a
#define AP3216C_MODE_RESET           0x04
#define AP3216C_MODE_ALS_PS_IR       0x03
#define AP3216C_SAMPLE_DELAY_MS      120

struct ap3216c_dev {
	struct i2c_client *client;
	struct mutex lock;
	dev_t devt;
	struct cdev cdev;
	struct class *class;
	struct device *device;
};

static int ap3216c_read_regs(struct ap3216c_dev *sensor, u8 reg,
			      u8 *data, size_t len)
{
	struct i2c_msg messages[2] = {
		{
			.addr = sensor->client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = sensor->client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = data,
		},
	};
	int ret;

	ret = i2c_transfer(sensor->client->adapter, messages,
			   ARRAY_SIZE(messages));
	if (ret == (int)ARRAY_SIZE(messages))
		return 0;

	if (ret >= 0)
		ret = -EIO;
	dev_err_ratelimited(&sensor->client->dev,
			    "register read failed: reg=0x%02x len=%zu err=%d\n",
			    reg, len, ret);
	return ret;
}

static int ap3216c_write_reg(struct ap3216c_dev *sensor, u8 reg, u8 value)
{
	u8 data[2] = { reg, value };
	int ret;

	ret = i2c_master_send(sensor->client, data, sizeof(data));
	if (ret == (int)sizeof(data))
		return 0;

	if (ret >= 0)
		ret = -EIO;
	dev_err(&sensor->client->dev,
		"register write failed: reg=0x%02x err=%d\n", reg, ret);
	return ret;
}

static int ap3216c_hw_init(struct ap3216c_dev *sensor)
{
	u8 config;
	int ret;

	ret = ap3216c_write_reg(sensor, AP3216C_SYSTEM_CONFIG,
				AP3216C_MODE_RESET);
	if (ret)
		return ret;

	msleep(50);
	ret = ap3216c_write_reg(sensor, AP3216C_SYSTEM_CONFIG,
				AP3216C_MODE_ALS_PS_IR);
	if (ret)
		return ret;

	/* ALS + PS/IR conversion needs more than 112.5 ms before first data. */
	msleep(AP3216C_SAMPLE_DELAY_MS);
	ret = ap3216c_read_regs(sensor, AP3216C_SYSTEM_CONFIG, &config, 1);
	if (ret)
		return ret;

	if ((config & 0x07) != AP3216C_MODE_ALS_PS_IR) {
		dev_err(&sensor->client->dev,
			"unexpected system configuration: 0x%02x\n", config);
		return -ENODEV;
	}

	return 0;
}

static int ap3216c_read_sample(struct ap3216c_dev *sensor,
				struct gateway_ap3216c_sample *sample)
{
	u8 data[6];
	unsigned int i;
	int ret;

	/* Preserve the register-by-register access used by the verified BSP path. */
	for (i = 0; i < ARRAY_SIZE(data); ++i) {
		ret = ap3216c_read_regs(sensor, AP3216C_IR_DATA_LOW + i,
					&data[i], 1);
		if (ret)
			return ret;
	}

	sample->ir = (data[0] & 0x80) ? 0 :
		(((sensor_uapi_u16)data[1] << 2) | (data[0] & 0x03));
	sample->als = ((sensor_uapi_u16)data[3] << 8) | data[2];
	sample->ps = (data[4] & 0x40) ? 0 :
		(((sensor_uapi_u16)(data[5] & 0x3f) << 4) |
		 (data[4] & 0x0f));

	return 0;
}

static int ap3216c_open(struct inode *inode, struct file *file)
{
	struct ap3216c_dev *sensor;

	sensor = container_of(inode->i_cdev, struct ap3216c_dev, cdev);
	file->private_data = sensor;
	return 0;
}

static ssize_t ap3216c_read(struct file *file, char __user *buffer,
			     size_t count, loff_t *offset)
{
	struct ap3216c_dev *sensor = file->private_data;
	struct gateway_ap3216c_sample sample;
	int ret;

	if (count < sizeof(sample))
		return -EINVAL;

	ret = mutex_lock_interruptible(&sensor->lock);
	if (ret)
		return ret;
	ret = ap3216c_read_sample(sensor, &sample);
	mutex_unlock(&sensor->lock);
	if (ret)
		return ret;

	if (copy_to_user(buffer, &sample, sizeof(sample)))
		return -EFAULT;

	return sizeof(sample);
}

static const struct file_operations ap3216c_fops = {
	.owner = THIS_MODULE,
	.open = ap3216c_open,
	.read = ap3216c_read,
	.llseek = no_llseek,
};

static int ap3216c_register_chrdev(struct ap3216c_dev *sensor)
{
	int ret;

	ret = alloc_chrdev_region(&sensor->devt, 0, 1, AP3216C_NAME);
	if (ret)
		return ret;

	cdev_init(&sensor->cdev, &ap3216c_fops);
	sensor->cdev.owner = THIS_MODULE;
	ret = cdev_add(&sensor->cdev, sensor->devt, 1);
	if (ret)
		goto err_unregister;

	sensor->class = class_create(THIS_MODULE, AP3216C_NAME);
	if (IS_ERR(sensor->class)) {
		ret = PTR_ERR(sensor->class);
		sensor->class = NULL;
		goto err_cdev;
	}

	sensor->device = device_create(sensor->class, &sensor->client->dev,
				       sensor->devt, sensor, AP3216C_NAME);
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

static void ap3216c_unregister_chrdev(struct ap3216c_dev *sensor)
{
	device_destroy(sensor->class, sensor->devt);
	class_destroy(sensor->class);
	cdev_del(&sensor->cdev);
	unregister_chrdev_region(sensor->devt, 1);
}

static int ap3216c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct ap3216c_dev *sensor;
	int ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -EOPNOTSUPP;

	sensor = devm_kzalloc(&client->dev, sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;

	sensor->client = client;
	mutex_init(&sensor->lock);
	i2c_set_clientdata(client, sensor);

	ret = ap3216c_hw_init(sensor);
	if (ret)
		return ret;

	ret = ap3216c_register_chrdev(sensor);
	if (ret)
		return ret;

	dev_info(&client->dev, "registered /dev/%s\n", AP3216C_NAME);
	return 0;
}

static int ap3216c_remove(struct i2c_client *client)
{
	struct ap3216c_dev *sensor = i2c_get_clientdata(client);

	ap3216c_unregister_chrdev(sensor);
	return 0;
}

static const struct i2c_device_id ap3216c_ids[] = {
	{ "ap3216c", 0 },
	{ "alientek,ap3216c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ap3216c_ids);

static const struct of_device_id ap3216c_of_match[] = {
	{ .compatible = "alientek,ap3216c" },
	{ }
};
MODULE_DEVICE_TABLE(of, ap3216c_of_match);

static struct i2c_driver ap3216c_driver = {
	.driver = {
		.name = AP3216C_NAME,
		.of_match_table = ap3216c_of_match,
	},
	.probe = ap3216c_probe,
	.remove = ap3216c_remove,
	.id_table = ap3216c_ids,
};

module_i2c_driver(ap3216c_driver);

MODULE_AUTHOR("Maaap1e (project adaptation)");
MODULE_DESCRIPTION("AP3216C character-device adapter for i.MX6ULL");
MODULE_LICENSE("GPL");
