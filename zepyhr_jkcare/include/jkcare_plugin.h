/* SPDX-License-Identifier: Apache-2.0 */

#ifndef JKCARE_PLUGIN_H_
#define JKCARE_PLUGIN_H_

#include <stddef.h>
#include <stdint.h>

#define JKCARE_HOST_ABI_VERSION   1U
#define JKCARE_PLUGIN_ABI_VERSION 1U

struct jkcare_plugin_thread;

typedef void (*jkcare_plugin_thread_entry_t)(void *context);

/* Services owned by the main firmware and made available to every plugin. */
struct jkcare_host_api {
	uint32_t abi_version;
	uint32_t struct_size;

	void (*print)(const char *plugin_name, const char *message);
	void (*sleep_ms)(uint32_t milliseconds);

	int (*thread_create)(struct jkcare_plugin_thread **thread,
			     const char *name,
			     jkcare_plugin_thread_entry_t entry,
			     void *context,
			     size_t stack_size,
			     int priority);
	int (*thread_join)(struct jkcare_plugin_thread *thread,
			   uint32_t timeout_ms);
};

/* Every LLEXT exports one object named "jkcare_plugin" with this layout. */
struct jkcare_plugin_api {
	uint32_t abi_version;
	uint32_t struct_size;
	const char *name;

	int (*init)(const struct jkcare_host_api *host);
	int (*start)(void);
	int (*stop)(void);
	void (*deinit)(void);
};

#endif /* JKCARE_PLUGIN_H_ */
