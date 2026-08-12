/*
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT sensylink_cht8305c

#include <errno.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(cht8305c, CONFIG_SENSOR_LOG_LEVEL);

#define CHT8305C_REG_TEMPERATURE       0x00
#define CHT8305C_REG_MANUFACTURER_ID   0xFE
#define CHT8305C_REG_VERSION_ID        0xFF

#define CHT8305C_MANUFACTURER_ID       0x5959
#define CHT8305C_VERSION_ID            0x8305
#define CHT8305C_MEASUREMENT_TIME_MS   20

struct cht8305c_config {
	struct i2c_dt_spec bus;
};

struct cht8305c_data {
	uint16_t temperature_raw;
	uint16_t humidity_raw;
};

static int cht8305c_read_register(const struct device *dev, uint8_t reg,
				  uint8_t *buffer, size_t length)
{
	const struct cht8305c_config *config = dev->config;

	return i2c_write_read_dt(&config->bus, &reg, sizeof(reg), buffer, length);
}

static int cht8305c_read_register_u16(const struct device *dev, uint8_t reg,
				      uint16_t *value)
{
	uint8_t buffer[2];
	int ret;

	ret = cht8305c_read_register(dev, reg, buffer, sizeof(buffer));
	if (ret < 0) {
		return ret;
	}

	*value = ((uint16_t)buffer[0] << 8) | buffer[1];
	return 0;
}

static int cht8305c_sample_fetch(const struct device *dev,
				 enum sensor_channel channel)
{
	const struct cht8305c_config *config = dev->config;
	struct cht8305c_data *data = dev->data;
	uint8_t reg = CHT8305C_REG_TEMPERATURE;
	uint8_t buffer[4];
	int ret;

	if (channel != SENSOR_CHAN_ALL &&
	    channel != SENSOR_CHAN_AMBIENT_TEMP &&
	    channel != SENSOR_CHAN_HUMIDITY) {
		return -ENOTSUP;
	}

	/* Selecting register 0x00 starts a temperature and humidity conversion. */
	ret = i2c_write_dt(&config->bus, &reg, sizeof(reg));
	if (ret < 0) {
		LOG_ERR("Failed to start measurement: %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(CHT8305C_MEASUREMENT_TIME_MS));

	/* Reading four bytes from 0x00 returns temperature followed by humidity. */
	ret = i2c_read_dt(&config->bus, buffer, sizeof(buffer));
	if (ret < 0) {
		LOG_ERR("Failed to read measurement: %d", ret);
		return ret;
	}

	data->temperature_raw = ((uint16_t)buffer[0] << 8) | buffer[1];
	data->humidity_raw = ((uint16_t)buffer[2] << 8) | buffer[3];

	return 0;
}

static int cht8305c_channel_get(const struct device *dev,
				enum sensor_channel channel,
				struct sensor_value *value)
{
	const struct cht8305c_data *data = dev->data;
	int64_t micro_value;

	switch (channel) {
	case SENSOR_CHAN_AMBIENT_TEMP:
		/* Temperature [C] = raw * 165 / 65535 - 40. */
		micro_value = ((int64_t)data->temperature_raw * 165000000LL) / 65535LL;
		micro_value -= 40000000LL;
		break;

	case SENSOR_CHAN_HUMIDITY:
		/* Relative humidity [%RH] = raw * 100 / 65535. */
		micro_value = ((int64_t)data->humidity_raw * 100000000LL) / 65535LL;
		break;

	default:
		return -ENOTSUP;
	}

	return sensor_value_from_micro(value, micro_value);
}

static int cht8305c_init(const struct device *dev)
{
	const struct cht8305c_config *config = dev->config;
	uint16_t manufacturer_id;
	uint16_t version_id;
	int ret;

	if (!i2c_is_ready_dt(&config->bus)) {
		LOG_ERR("I2C bus is not ready");
		return -ENODEV;
	}

	ret = cht8305c_read_register_u16(dev, CHT8305C_REG_MANUFACTURER_ID,
					 &manufacturer_id);
	if (ret < 0) {
		LOG_ERR("Failed to read manufacturer ID: %d", ret);
		return ret;
	}

	ret = cht8305c_read_register_u16(dev, CHT8305C_REG_VERSION_ID, &version_id);
	if (ret < 0) {
		LOG_ERR("Failed to read version ID: %d", ret);
		return ret;
	}

	if (manufacturer_id != CHT8305C_MANUFACTURER_ID ||
	    version_id != CHT8305C_VERSION_ID) {
		LOG_ERR("Unexpected IDs: manufacturer=0x%04x version=0x%04x",
			manufacturer_id, version_id);
		return -ENODEV;
	}

	return 0;
}

static DEVICE_API(sensor, cht8305c_api) = {
	.sample_fetch = cht8305c_sample_fetch,
	.channel_get = cht8305c_channel_get,
};

#define CHT8305C_DEFINE(inst)                                                   \
	static struct cht8305c_data cht8305c_data_##inst;                       \
	static const struct cht8305c_config cht8305c_config_##inst = {           \
		.bus = I2C_DT_SPEC_INST_GET(inst),                                \
	};                                                                          \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, cht8305c_init, NULL,                    \
				     &cht8305c_data_##inst,                    \
				     &cht8305c_config_##inst,                  \
				     POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY, \
				     &cht8305c_api);

DT_INST_FOREACH_STATUS_OKAY(CHT8305C_DEFINE)
