# jkcare

Zephyr application with a custom board definition for the JKCare Apollo4
Plus Blue hardware.

## Workspace setup

The west manifest pins Zephyr to `v4.4.0` and imports the matching Zephyr
modules. From the directory which will contain the workspace:

```powershell
git clone <jkcare-repository-url> jkcare
west init -l .\jkcare
west update
```

This creates `zephyr-4.4.0` and the module directories next to `jkcare`.

Initial board pin assignment:

- UART0 TX: GPIO 12
- UART0 RX: GPIO 47
- I2C0 SCL: GPIO 5
- I2C0 SDA: GPIO 6
- 32 MHz clock output: GPIO 46
- 32 kHz clock: GPIO 4

These are temporary assignments copied from the Ambiq EVB. Update
`boards/ambiq/jkcare_apollo4p_blue/jkcare_apollo4p_blue-pinctrl.dtsi` and
the I2C GPIO properties in `jkcare_apollo4p_blue.dts` to match the schematic.

## Build

From `E:\liaoguangwei\code\zephyr_zip\zephyr-4.4.0` in an activated
Zephyr Python virtual environment:

```powershell
west build -p always `
    -b jkcare_apollo4p_blue `
    -d ..\jkcare\build `
    ..\jkcare
```

## Flash

```powershell
west flash -d ..\jkcare\build
```
