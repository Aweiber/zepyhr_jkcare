/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/llext/fs_loader.h>
#include <zephyr/llext/llext.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include "jkcare_plugin.h"
#include "jkcare_plugin_manager.h"

LOG_MODULE_REGISTER(jkcare_plugin_manager, LOG_LEVEL_INF);

#define PLUGIN_DIRECTORY         "/llext/business"
#define PLUGIN_FILE_SUFFIX       ".llext"
#define PLUGIN_PATH_MAX          192
#define MAX_PLUGINS              8
#define MAX_PLUGIN_THREADS       8
#define PLUGIN_THREAD_STACK_SIZE 2048

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

static bool is_plugin_file(const struct fs_dirent *entry)
{
	size_t name_length;
	size_t suffix_length = strlen(PLUGIN_FILE_SUFFIX);

	if (entry->type != FS_DIR_ENTRY_FILE) {
		return false;
	}

	name_length = strlen(entry->name);
	return name_length > suffix_length &&
	       strcmp(entry->name + name_length - suffix_length,
		      PLUGIN_FILE_SUFFIX) == 0;
}

static int plugin_name_from_filename(const char *filename, char *name,
				     size_t name_size)
{
	size_t filename_length = strlen(filename);
	size_t suffix_length = strlen(PLUGIN_FILE_SUFFIX);
	size_t name_length;

	if (filename_length <= suffix_length || name_size == 0) {
		return -EINVAL;
	}

	name_length = filename_length - suffix_length;
	if (name_length >= name_size) {
		return -ENAMETOOLONG;
	}

	memcpy(name, filename, name_length);
	name[name_length] = '\0';
	return 0;
}

static int load_and_start_plugin(const char *path, const char *filename)
{
	struct llext_fs_loader fs_loader = LLEXT_FS_LOADER(path);
	struct llext_load_param load_param = LLEXT_LOAD_PARAM_DEFAULT;
	struct loaded_plugin *plugin;
	char plugin_name[LLEXT_MAX_NAME_LEN + 1];
	int ret;

	if (loaded_plugin_count >= ARRAY_SIZE(loaded_plugins)) {
		return -ENOMEM;
	}

	plugin = &loaded_plugins[loaded_plugin_count];

	ret = plugin_name_from_filename(filename, plugin_name,
					sizeof(plugin_name));
	if (ret < 0) {
		LOG_ERR("Invalid plugin filename %s: %d", filename, ret);
		return ret;
	}

	ret = llext_load(&fs_loader.loader, plugin_name,
			 &plugin->extension, &load_param);
	if (ret < 0) {
		LOG_ERR("Failed to load plugin %s: %d", path, ret);
		return ret;
	}

	plugin->api = llext_find_sym(&plugin->extension->exp_tab,
				     "jkcare_plugin");
	if (plugin->api == NULL) {
		LOG_ERR("Plugin %s does not export jkcare_plugin", path);
		(void)llext_unload(&plugin->extension);
		return -ENOENT;
	}

	if (plugin->api->abi_version != JKCARE_PLUGIN_ABI_VERSION ||
	    plugin->api->struct_size != sizeof(struct jkcare_plugin_api)) {
		LOG_ERR("Plugin %s has an incompatible ABI", path);
		(void)llext_unload(&plugin->extension);
		return -EPROTO;
	}

	ret = plugin->api->init(&host_api);
	if (ret < 0) {
		LOG_ERR("Plugin %s init failed: %d", path, ret);
		(void)llext_unload(&plugin->extension);
		return ret;
	}

	ret = plugin->api->start();
	if (ret < 0) {
		LOG_ERR("Plugin %s start failed: %d", path, ret);
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
	struct fs_dir_t directory;
	struct fs_dirent entry;
	char path[PLUGIN_PATH_MAX];
	int first_error = 0;
	int ret;

	fs_dir_t_init(&directory);
	ret = fs_opendir(&directory, PLUGIN_DIRECTORY);
	if (ret < 0) {
		LOG_ERR("Cannot open plugin directory %s: %d; mount the filesystem first",
			PLUGIN_DIRECTORY, ret);
		return ret;
	}

	LOG_INF("Scanning plugin directory: %s", PLUGIN_DIRECTORY);

	while (true) {
		ret = fs_readdir(&directory, &entry);
		if (ret < 0) {
			LOG_ERR("Failed to read plugin directory: %d", ret);
			if (first_error == 0) {
				first_error = ret;
			}
			break;
		}

		if (entry.name[0] == '\0') {
			break;
		}

		if (!is_plugin_file(&entry)) {
			continue;
		}

		ret = snprintk(path, sizeof(path), "%s/%s",
			       PLUGIN_DIRECTORY, entry.name);
		if (ret < 0 || ret >= sizeof(path)) {
			LOG_ERR("Plugin path is too long: %s", entry.name);
			if (first_error == 0) {
				first_error = -ENAMETOOLONG;
			}
			continue;
		}

		ret = load_and_start_plugin(path, entry.name);

		if (ret < 0 && first_error == 0) {
			first_error = ret;
		}
	}

	(void)fs_closedir(&directory);
	LOG_INF("Started %u plugin(s) from %s",
		(unsigned int)loaded_plugin_count, PLUGIN_DIRECTORY);
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
