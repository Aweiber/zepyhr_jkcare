/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/llext/buf_loader.h>
#include <zephyr/llext/llext.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "jkcare_plugin.h"
#include "jkcare_plugin_manager.h"

LOG_MODULE_REGISTER(jkcare_plugin_manager, LOG_LEVEL_INF);

#define MAX_PLUGINS             2
#define MAX_PLUGIN_THREADS      2
#define PLUGIN_THREAD_STACK_SIZE 2048

/* These byte arrays are generated from the two independent LLEXT ELF files. */
static uint8_t ppg_plugin_image[] = {
#include "ppg_plugin.inc"
};

static uint8_t ecg_plugin_image[] = {
#include "ecg_plugin.inc"
};

struct plugin_image {
	const char *name;
	uint8_t *data;
	size_t size;
};

static const struct plugin_image plugin_images[] = {
	{ "ppg", ppg_plugin_image, sizeof(ppg_plugin_image) },
	{ "ecg", ecg_plugin_image, sizeof(ecg_plugin_image) },
};

struct loaded_plugin {
	struct llext *extension;
	const struct jkcare_plugin_api *api;
	bool started;
};

struct jkcare_plugin_thread {
	struct k_thread thread;
	jkcare_plugin_thread_entry_t entry;
	void *context;
	bool allocated;
};

K_THREAD_STACK_ARRAY_DEFINE(plugin_thread_stacks, MAX_PLUGIN_THREADS,
			    PLUGIN_THREAD_STACK_SIZE);

static struct jkcare_plugin_thread plugin_threads[MAX_PLUGIN_THREADS];
static struct loaded_plugin loaded_plugins[MAX_PLUGINS];
static size_t loaded_plugin_count;

static void host_print(const char *plugin_name, const char *message)
{
	LOG_INF("[%s] %s", plugin_name, message);
}

static void host_sleep_ms(uint32_t milliseconds)
{
	k_sleep(K_MSEC(milliseconds));
}

static void plugin_thread_trampoline(void *thread_arg, void *unused1,
				     void *unused2)
{
	struct jkcare_plugin_thread *thread = thread_arg;

	ARG_UNUSED(unused1);
	ARG_UNUSED(unused2);
	thread->entry(thread->context);
}

static int host_thread_create(struct jkcare_plugin_thread **thread,
			      const char *name,
			      jkcare_plugin_thread_entry_t entry,
			      void *context, size_t stack_size,
			      int priority)
{
	if (thread == NULL || entry == NULL || stack_size > PLUGIN_THREAD_STACK_SIZE) {
		return -EINVAL;
	}

	for (size_t i = 0; i < ARRAY_SIZE(plugin_threads); i++) {
		struct jkcare_plugin_thread *slot = &plugin_threads[i];

		if (slot->allocated) {
			continue;
		}

		slot->allocated = true;
		slot->entry = entry;
		slot->context = context;
		k_tid_t tid = k_thread_create(&slot->thread,
					      plugin_thread_stacks[i],
					      K_THREAD_STACK_SIZEOF(plugin_thread_stacks[i]),
					      plugin_thread_trampoline,
					      slot, NULL, NULL,
					      priority, 0, K_NO_WAIT);
		if (tid == NULL) {
			slot->allocated = false;
			slot->entry = NULL;
			slot->context = NULL;
			return -EIO;
		}

		(void)k_thread_name_set(tid, name);
		*thread = slot;
		return 0;
	}

	return -ENOMEM;
}

static int host_thread_join(struct jkcare_plugin_thread *thread,
			    uint32_t timeout_ms)
{
	int ret;

	if (thread == NULL || !thread->allocated) {
		return -EINVAL;
	}

	ret = k_thread_join(&thread->thread, K_MSEC(timeout_ms));
	if (ret == 0) {
		thread->allocated = false;
		thread->entry = NULL;
		thread->context = NULL;
	}

	return ret;
}

static const struct jkcare_host_api host_api = {
	.abi_version = JKCARE_HOST_ABI_VERSION,
	.struct_size = sizeof(struct jkcare_host_api),
	.print = host_print,
	.sleep_ms = host_sleep_ms,
	.thread_create = host_thread_create,
	.thread_join = host_thread_join,
};

static int load_and_start_plugin(const struct plugin_image *image)
{
	struct llext_buf_loader buffer_loader =
		LLEXT_BUF_LOADER(image->data, image->size);
	struct llext_load_param load_param = LLEXT_LOAD_PARAM_DEFAULT;
	struct loaded_plugin *plugin;
	int ret;

	if (loaded_plugin_count >= ARRAY_SIZE(loaded_plugins)) {
		return -ENOMEM;
	}

	plugin = &loaded_plugins[loaded_plugin_count];

	ret = llext_load(&buffer_loader.loader, image->name,
			 &plugin->extension, &load_param);
	if (ret < 0) {
		LOG_ERR("Failed to load %s plugin: %d", image->name, ret);
		return ret;
	}

	plugin->api = llext_find_sym(&plugin->extension->exp_tab,
				     "jkcare_plugin");
	if (plugin->api == NULL) {
		LOG_ERR("Plugin %s does not export jkcare_plugin", image->name);
		(void)llext_unload(&plugin->extension);
		return -ENOENT;
	}

	if (plugin->api->abi_version != JKCARE_PLUGIN_ABI_VERSION ||
	    plugin->api->struct_size != sizeof(struct jkcare_plugin_api)) {
		LOG_ERR("Plugin %s has an incompatible ABI", image->name);
		(void)llext_unload(&plugin->extension);
		return -EPROTO;
	}

	ret = plugin->api->init(&host_api);
	if (ret < 0) {
		LOG_ERR("Plugin %s init failed: %d", image->name, ret);
		(void)llext_unload(&plugin->extension);
		return ret;
	}

	ret = plugin->api->start();
	if (ret < 0) {
		LOG_ERR("Plugin %s start failed: %d", image->name, ret);
		plugin->api->deinit();
		(void)llext_unload(&plugin->extension);
		return ret;
	}

	plugin->started = true;
	loaded_plugin_count++;
	LOG_INF("Loaded and started plugin: %s", plugin->api->name);
	return 0;
}

int jkcare_plugin_manager_load_all(void)
{
	int first_error = 0;

	for (size_t i = 0; i < ARRAY_SIZE(plugin_images); i++) {
		int ret = load_and_start_plugin(&plugin_images[i]);

		if (ret < 0 && first_error == 0) {
			first_error = ret;
		}
	}

	return first_error;
}

int jkcare_plugin_manager_stop_all(void)
{
	int first_error = 0;

	/* Stop in reverse load order. Never unload an extension with a live thread. */
	while (loaded_plugin_count > 0) {
		struct loaded_plugin *plugin =
			&loaded_plugins[loaded_plugin_count - 1];
		int ret = 0;

		if (plugin->started) {
			ret = plugin->api->stop();
			if (ret < 0) {
				LOG_ERR("Plugin %s did not stop: %d",
					plugin->api->name, ret);
				if (first_error == 0) {
					first_error = ret;
				}

				/* Keep it loaded because its thread may still execute its code. */
				break;
			}
			plugin->started = false;
		}

		plugin->api->deinit();
		ret = llext_unload(&plugin->extension);
		if (ret < 0 && first_error == 0) {
			first_error = ret;
		}

		plugin->api = NULL;
		loaded_plugin_count--;
	}

	return first_error;
}
