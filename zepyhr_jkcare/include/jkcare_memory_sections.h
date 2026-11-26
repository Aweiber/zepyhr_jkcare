/* SPDX-License-Identifier: Apache-2.0 */

#ifndef JKCARE_MEMORY_SECTIONS_H_
#define JKCARE_MEMORY_SECTIONS_H_

#include <stddef.h>
#include <stdint.h>

/* DMA/cache-friendly minimum alignment used by all JKCare RAM sections. */
#define JKCARE_RAM_ALIGNMENT 32

#define JKCARE_SECTION(_name)                                                \
	__attribute__((section(_name), aligned(JKCARE_RAM_ALIGNMENT), used))

/* Low-latency CPU TCM: 0x10010000 - 0x1005ffff. */
#define JKCARE_FAST_RAM JKCARE_SECTION(".jkcare_fast_ram")

/* Default/normal system memory in SSRAM0: 0x10060000 - 0x1015ffff. */
#define JKCARE_NORMAL_RAM JKCARE_SECTION(".jkcare_normal_ram")
#define JKCARE_SSRAM0     JKCARE_NORMAL_RAM

/* Apollo4 Plus Extended SRAM: 0x10160000 - 0x101bffff. */
#define JKCARE_EXTENDED_RAM JKCARE_SECTION(".jkcare_extended_ram")

/* Second shared SRAM group: 0x101c0000 - 0x102bffff. */
#define JKCARE_SSRAM1 JKCARE_SECTION(".jkcare_ssram1")

extern uint8_t __jkcare_fast_ram_start[];
extern uint8_t __jkcare_fast_ram_end[];
extern uint8_t __jkcare_normal_ram_start[];
extern uint8_t __jkcare_normal_ram_end[];
extern uint8_t __jkcare_extended_ram_start[];
extern uint8_t __jkcare_extended_ram_end[];
extern uint8_t __jkcare_ssram1_start[];
extern uint8_t __jkcare_ssram1_end[];

#define JKCARE_SECTION_SIZE(_start, _end) \
	((size_t)((uintptr_t)(_end) - (uintptr_t)(_start)))

#endif /* JKCARE_MEMORY_SECTIONS_H_ */
