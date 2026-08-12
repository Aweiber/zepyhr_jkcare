/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(jkcare, LOG_LEVEL_INF);

#if !DT_NODE_HAS_STATUS(DT_NODELABEL(i2c0), okay)
#error "The selected board does not define an enabled i2c0 node"
#endif

static const struct device *const i2c0 = DEVICE_DT_GET(DT_NODELABEL(i2c0));

int main(void)
{
	if (!device_is_ready(i2c0)) {
		LOG_ERR("I2C0 is not ready");
		return 0;
	}

	LOG_INF("jkcare started on %s", CONFIG_BOARD_TARGET);
	LOG_INF("UART0 console is running at 115200 baud");
	LOG_INF("I2C0 is ready");

	while (true) {
		k_sleep(K_SECONDS(5));
	}
}
