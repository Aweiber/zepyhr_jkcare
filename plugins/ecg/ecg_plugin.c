/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdbool.h>

#include <zephyr/llext/symbol.h>

#include "jkcare_plugin.h"

static const struct jkcare_host_api *host;
static struct jkcare_plugin_thread *worker;
static volatile bool running;

static void ecg_thread(void *context)
{
	(void)context;

	while (running) {
		host->print("ECG", " business thread is running");
		host->sleep_ms(1500);
	}

	host->print("ECG", " business thread stopped");
}

static int ecg_init(const struct jkcare_host_api *api)
{
	if (api == NULL || api->abi_version != JKCARE_HOST_ABI_VERSION) {
		return -EINVAL;
	}

	host = api;
	host->print("ECG", "initialized");
	return 0;
}

static int ecg_start(void)
{
	int ret;

	if (running) {
		return -EALREADY;
	}

	running = true;
	ret = host->thread_create(&worker, "ecg_plugin", ecg_thread, NULL,
				  2048, 5);
	if (ret < 0) {
		running = false;
	}

	return ret;
}

static int ecg_stop(void)
{
	if (!running) {
		return 0;
	}

	running = false;
	return host->thread_join(worker, 2500);
}

static void ecg_deinit(void)
{
	host = NULL;
	worker = NULL;
}

const struct jkcare_plugin_api jkcare_plugin = {
	.abi_version = JKCARE_PLUGIN_ABI_VERSION,
	.struct_size = sizeof(struct jkcare_plugin_api),
	.name = "ECG business",
	.init = ecg_init,
	.start = ecg_start,
	.stop = ecg_stop,
	.deinit = ecg_deinit,
};

EXPORT_SYMBOL(jkcare_plugin);
