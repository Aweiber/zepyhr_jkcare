/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "jkcare_plugin_manager.h"

LOG_MODULE_REGISTER(jkcare, LOG_LEVEL_INF);

#define USER_NODE DT_PATH(zephyr_user)

#if !DT_NODE_HAS_STATUS(DT_NODELABEL(i2c0), okay)
#error "The selected board does not define an enabled i2c0 node"
#endif

#if !DT_NODE_HAS_STATUS(DT_NODELABEL(sht4x), okay)
#error "app.overlay does not define an enabled sht4x node"
#endif

#if !DT_NODE_HAS_STATUS(DT_NODELABEL(cht8305c), okay)
#error "app.overlay does not define an enabled cht8305c node"
#endif

#if !DT_NODE_EXISTS(USER_NODE)
#error "app.overlay does not define /zephyr,user"
#endif

static const struct device *const i2c0 = DEVICE_DT_GET(DT_NODELABEL(i2c0));
static const struct device *const sht4x = DEVICE_DT_GET(DT_NODELABEL(sht4x));
static const struct device *const cht8305c = DEVICE_DT_GET(DT_NODELABEL(cht8305c));
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(USER_NODE, led_gpios);
static const struct gpio_dt_spec touch = GPIO_DT_SPEC_GET(USER_NODE, touch_gpios);
static struct gpio_callback touch_callback;

static void touch_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	int ret = gpio_pin_toggle_dt(&led);

	if (ret < 0) {
		LOG_ERR("Failed to toggle LED: %d", ret);
		return;
	}

	LOG_INF("Touch rising edge, LED=%d", gpio_pin_get_dt(&led));
}

K_WORK_DEFINE(touch_work, touch_work_handler);

static void touch_interrupt_handler(const struct device *port,
				    struct gpio_callback *callback,
				    gpio_port_pins_t pins)
{
	ARG_UNUSED(port);
	ARG_UNUSED(callback);
	ARG_UNUSED(pins);

	/* Keep the ISR short; GPIO access and logging run on the system workqueue. */
	k_work_submit(&touch_work);
}

static int gpio_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&led)) {
		LOG_ERR("LED GPIO controller is not ready");
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&touch)) {
		LOG_ERR("Touch GPIO controller is not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure LED GPIO: %d", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(&touch, GPIO_INPUT);
	if (ret < 0) {
		LOG_ERR("Failed to configure touch GPIO: %d", ret);
		return ret;
	}

	gpio_init_callback(&touch_callback, touch_interrupt_handler, BIT(touch.pin));

	ret = gpio_add_callback(touch.port, &touch_callback);
	if (ret < 0) {
		LOG_ERR("Failed to add touch callback: %d", ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&touch, GPIO_INT_EDGE_RISING);
	if (ret < 0) {
		LOG_ERR("Failed to enable touch rising-edge interrupt: %d", ret);
		gpio_remove_callback(touch.port, &touch_callback);
		return ret;
	}

	LOG_INF("LED GPIO%u and touch GPIO%u are ready", led.pin, touch.pin);
	return 0;
}

static void log_milli_value(const char *name, int64_t milli, const char *unit)
{
	bool negative = milli < 0;
	uint32_t magnitude = (uint32_t)(negative ? -milli : milli);

	LOG_INF("%s: %s%u.%03u %s", name, negative ? "-" : "",
		magnitude / 1000U, magnitude % 1000U, unit);
}

int main(void)
{
	if (gpio_init() < 0) {
		return 0;
	}

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

	/*
	 * The filesystem must already be mounted at /llext. Discover every
	 * plugin file below /llext/business, load its ELF into the LLEXT RAM heap,
	 * resolve the common jkcare_plugin symbol, then call init() and start().
	 * PPG and ECG start their own managed business threads, so main does not
	 * call a process() function for them.
	 */
	int plugin_ret = jkcare_plugin_manager_load_all();
	if (plugin_ret < 0) {
		LOG_ERR("One or more dynamic plugins failed to start: %d", plugin_ret);
	}
    
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
