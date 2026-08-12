/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(jkcare, LOG_LEVEL_INF);

#if !DT_NODE_HAS_STATUS(DT_NODELABEL(i2c0), okay)
#error "The selected board does not define an enabled i2c0 node"
#endif

#if !DT_NODE_HAS_STATUS(DT_NODELABEL(sht4x), okay)
#error "app.overlay does not define an enabled sht4x node"
#endif

#if !DT_NODE_HAS_STATUS(DT_NODELABEL(cht8305c), okay)
#error "app.overlay does not define an enabled sht4x node"
#endif

static const struct device *const i2c0 = DEVICE_DT_GET(DT_NODELABEL(i2c0));
static const struct device *const sht4x = DEVICE_DT_GET(DT_NODELABEL(sht4x));
static const struct device *const cht8305c = DEVICE_DT_GET(DT_NODELABEL(cht8305c));

static void log_milli_value(const char *name, int64_t milli, const char *unit)
{
	bool negative = milli < 0;
	uint32_t magnitude = (uint32_t)(negative ? -milli : milli);

	LOG_INF("%s: %s%u.%03u %s", name, negative ? "-" : "",
		magnitude / 1000U, magnitude % 1000U, unit);
}

int main(void)
{
	if (!device_is_ready(i2c0)) {
		LOG_ERR("I2C0 is not ready");
		return 0;
	}

	if (!device_is_ready(sht4x)) {
		LOG_ERR("SHT4x is not ready");
		return 0;
	}

    if (!device_is_ready(cht8305c)) {
        LOG_ERR("CHT8305C is not ready");
    }

	LOG_INF("jkcare started on %s", CONFIG_BOARD_TARGET);
	LOG_INF("UART0 console is running at 115200 baud");
	LOG_INF("I2C0 is ready");
	LOG_INF("SHT4x is ready");
    
	while (true) {
		struct sensor_value temperature;
		struct sensor_value humidity;
		int ret;

		ret = sensor_sample_fetch(sht4x);
		if (ret < 0) {
			LOG_ERR("Failed to fetch SHT4x sample: %d", ret);
			k_sleep(K_SECONDS(5));
			continue;
		}

		ret = sensor_channel_get(sht4x, SENSOR_CHAN_AMBIENT_TEMP,
					 &temperature);
		if (ret < 0) {
			LOG_ERR("Failed to read temperature: %d", ret);
			k_sleep(K_SECONDS(5));
			continue;
		}

		ret = sensor_channel_get(sht4x, SENSOR_CHAN_HUMIDITY, &humidity);
		if (ret < 0) {
			LOG_ERR("Failed to read humidity: %d", ret);
			k_sleep(K_SECONDS(5));
			continue;
		}

		log_milli_value("Temperature", sensor_value_to_milli(&temperature), "C");
		log_milli_value("Humidity", sensor_value_to_milli(&humidity), "%RH");

        ret = sensor_sample_fetch(cht8305c);
        if (ret < 0) {
            LOG_ERR("CHT8305C sample fetch failed: %d", ret);
            continue;
        }
        
        ret = sensor_channel_get(
            cht8305c,
            SENSOR_CHAN_AMBIENT_TEMP,
            &temperature
        );
        
        ret = sensor_channel_get(
            cht8305c,
            SENSOR_CHAN_HUMIDITY,
            &humidity
        );
		
        log_milli_value("Temperature", sensor_value_to_milli(&temperature), "C");
		log_milli_value("Humidity", sensor_value_to_milli(&humidity), "%RH");

		k_sleep(K_SECONDS(5));
	}
}
