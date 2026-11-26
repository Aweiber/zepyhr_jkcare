/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Dedicated Apollo4 Plus RAM banks.  These are NOLOAD sections: code must
 * initialize their contents explicitly before use.  Keeping them NOLOAD also
 * avoids an early boot copy into a bank whose power/retention policy may not
 * have been configured yet.
 */
SECTION_PROLOGUE(.jkcare_fast_ram, (NOLOAD), SUBALIGN(32))
{
	. = ALIGN(32);
	__jkcare_fast_ram_start = .;
	KEEP(*(.jkcare_fast_ram))
	KEEP(*(.jkcare_fast_ram.*))
	. = ALIGN(32);
	__jkcare_fast_ram_end = .;
} GROUP_NOLOAD_LINK_IN(FAST_RAM, FAST_RAM)

__jkcare_fast_ram_size =
	__jkcare_fast_ram_end - __jkcare_fast_ram_start;

SECTION_PROLOGUE(.jkcare_extended_ram, (NOLOAD), SUBALIGN(32))
{
	. = ALIGN(32);
	__jkcare_extended_ram_start = .;
	KEEP(*(.jkcare_extended_ram))
	KEEP(*(.jkcare_extended_ram.*))
	. = ALIGN(32);
	__jkcare_extended_ram_end = .;
} GROUP_NOLOAD_LINK_IN(EXTENDED_RAM, EXTENDED_RAM)

__jkcare_extended_ram_size =
	__jkcare_extended_ram_end - __jkcare_extended_ram_start;

SECTION_PROLOGUE(.jkcare_ssram1, (NOLOAD), SUBALIGN(32))
{
	. = ALIGN(32);
	__jkcare_ssram1_start = .;
	KEEP(*(.jkcare_ssram1))
	KEEP(*(.jkcare_ssram1.*))
	. = ALIGN(32);
	__jkcare_ssram1_end = .;
} GROUP_NOLOAD_LINK_IN(SSRAM1, SSRAM1)

__jkcare_ssram1_size = __jkcare_ssram1_end - __jkcare_ssram1_start;
