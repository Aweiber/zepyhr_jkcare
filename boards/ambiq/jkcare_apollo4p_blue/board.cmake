# SPDX-License-Identifier: Apache-2.0

# Verify this J-Link device name against the exact MCU fitted to the PCB.
board_runner_args(jlink "--device=AMAP42KK-KBR" "--iface=swd" "--speed=1000")

include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
