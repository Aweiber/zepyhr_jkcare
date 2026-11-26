/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Uninitialized buffers in the default Zephyr RAM region (SSRAM0 on JKCare).
 * This snippet is inserted with zephyr_linker_sources(RAM_SECTIONS ...).
 */
SECTION_PROLOGUE(.jkcare_normal_ram, (NOLOAD), SUBALIGN(32))
{
	. = ALIGN(32);
	__jkcare_normal_ram_start = .;
	KEEP(*(.jkcare_normal_ram))
	KEEP(*(.jkcare_normal_ram.*))
	. = ALIGN(32);
	__jkcare_normal_ram_end = .;
} GROUP_NOLOAD_LINK_IN(RAMABLE_REGION, RAMABLE_REGION)

__jkcare_normal_ram_size =
	__jkcare_normal_ram_end - __jkcare_normal_ram_start;
