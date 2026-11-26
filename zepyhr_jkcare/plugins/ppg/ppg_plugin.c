/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdbool.h>

#include <zephyr/llext/symbol.h>

#include "jkcare_plugin.h"

static const struct jkcare_host_api *host;
static struct jkcare_plugin_thread *worker;
static volatile bool running;

static void ppg_thread(void *context)
{
	(void)context;

	while (running) {
		host->print("PPG", " business thread is running");
		host->sleep_ms(1000);
	}

	host->print("PPG", " business thread stopped");
}

static int ppg_init(const struct jkcare_host_api *api)
{
	if (api == NULL || api->abi_version != JKCARE_HOST_ABI_VERSION) {
		return -EINVAL;
	}

	host = api;
	host->print("PPG", "initialized");
	return 0;
}

static int ppg_start(void)
{
	int ret;

	if (running) {
		return -EALREADY;
	}

	running = true;
	ret = host->thread_create(&worker, "ppg_plugin", ppg_thread, NULL,
				  2048, 5);
	if (ret < 0) {
		running = false;
	}

	return ret;
}

static int ppg_stop(void)
{
	if (!running) {
		return 0;
	}

	running = false;
	return host->thread_join(worker, 2000);
}

static void ppg_deinit(void)
{
	host = NULL;
	worker = NULL;
}

const struct jkcare_plugin_api jkcare_plugin = {
	.abi_version = JKCARE_PLUGIN_ABI_VERSION,
	.struct_size = sizeof(struct jkcare_plugin_api),
	.name = "PPG business",
	.init = ppg_init,
	.start = ppg_start,
	.stop = ppg_stop,
	.deinit = ppg_deinit,
};

EXPORT_SYMBOL(jkcare_plugin);
