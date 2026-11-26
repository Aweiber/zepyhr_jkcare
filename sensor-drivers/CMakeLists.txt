# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_CHT8305C
    drivers/sensor/cht8305c/cht8305c.c
)
