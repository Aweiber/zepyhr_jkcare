# JKCare Zephyr 开发指南

本项目是在 Zephyr 4.4.0 上为自研 JKCare Apollo4 Plus Blue 硬件建立的独立应用仓库。文档整理了 Windows 环境搭建、west 工作区、Zephyr SDK、自定义开发板、DeviceTree、I2C/UART 配置，以及内置 SHT4x 和项目外置 CHT8305C 传感器驱动的完整关系。

## 1. 当前工程结构

推荐只用 Git 管理 `jkcare`，Zephyr 源码、模块、SDK、虚拟环境和构建产物均不提交到该仓库。

```text
zephyr_zip/                         west 工作区根目录
├── .west/                          west 工作区元数据
├── .venv/                          Python 虚拟环境
├── zephyr-4.4.0/                   Zephyr v4.4.0 源码
├── modules/                        west 下载的 HAL、CMSIS 等模块
├── bootloader/                     west 下载的引导程序模块
└── jkcare/                         本项目，建议作为独立 Git 仓库
    ├── west.yml                    固定 Zephyr 版本和依赖
    ├── CMakeLists.txt              应用构建入口
    ├── Kconfig                     项目 Kconfig 入口
    ├── prj.conf                    应用功能配置
    ├── app.overlay                 应用级硬件扩展
    ├── boards/ambiq/
    │   └── jkcare_apollo4p_blue/   自定义板定义
    ├── dts/bindings/sensor/        项目自己的 DT binding
    ├── drivers/sensor/cht8305c/    项目自己的传感器驱动
    └── src/main.c                  应用入口
```

`build/`、`.venv/`、Zephyr 源码和 west 模块不应纳入 `jkcare` 的 Git。建议 `.gitignore` 至少包含：

```gitignore
build/
.cache/
.vscode/ipch/
```

## 2. 为什么选择 Zephyr 4.4.0

开发一个稳定项目时，建议使用正式 tag，例如 `v4.4.0`，而不是随时变化的 `main` 分支。项目的 `west.yml` 已固定版本：

```yaml
manifest:
  version: "1.2"

  remotes:
    - name: zephyrproject
      url-base: https://github.com/zephyrproject-rtos

  projects:
    - name: zephyr
      remote: zephyrproject
      revision: v4.4.0
      path: zephyr-4.4.0
      import: true

  self:
    path: jkcare
```

`import: true` 会读取 Zephyr 自己的 manifest，并下载该版本声明的模块。默认会处理 Zephyr manifest 中的全部项目，不仅是当前 SoC 的 HAL。下载失败时可以重新执行 `west update`；但实际编译 Apollo4 时，至少必须存在 Ambiq HAL、CMSIS 等被构建依赖的模块。

从零创建工作区时，在将要作为工作区根目录的上级目录执行：

```powershell
git clone <jkcare仓库地址> jkcare
west init -l .\jkcare
west update
```

已有 `.west` 工作区时不要反复执行 `west init`。

> GitHub 下载的 Zephyr tag ZIP 只包含 Zephyr 主仓库，不包含 manifest 中的 HAL、CMSIS 等其他仓库。仅解压 ZIP 不能形成完整工作区，仍需执行 `west init -l` 和 `west update`，或从有网络的机器复制完整工作区。

## 3. Windows Python 与 west 环境

### 3.1 创建虚拟环境

在工作区根目录创建一个统一的虚拟环境：

```powershell
cd E:\liaoguangwei\code\zephyr_zip
python -m venv .venv
```

创建和激活是两条命令，不能写成一条：

```powershell
.\.venv\Scripts\Activate.ps1
```

如果 PowerShell 禁止执行激活脚本，可仅为当前用户放开：

```powershell
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
```

激活后确认解释器和 west 来自同一个虚拟环境：

```powershell
python -c "import sys; print(sys.executable)"
Get-Command west
python -m pip show west
```

理想结果应指向：

```text
E:\liaoguangwei\code\zephyr_zip\.venv\Scripts\python.exe
E:\liaoguangwei\code\zephyr_zip\.venv\Scripts\west.exe
```

虚拟环境彼此隔离。在 `zephyr-4.4.0\.venv` 中安装的 west，不会自动出现在 `zephyr_zip\.venv` 中。切换到新虚拟环境后，需要在新环境安装依赖，但不需要重新下载 Zephyr 源码和 modules。

### 3.2 安装 west 和 Zephyr Python 依赖

```powershell
python -m pip install --upgrade pip
python -m pip install west
python -m pip install -r .\zephyr-4.4.0\scripts\requirements.txt
```

只安装 `west` 通常不够；例如 `Missing jsonschema dependency` 表示尚未安装 Zephyr 的完整 Python requirements。

如果出现代理错误，先检查环境和 pip 配置：

```powershell
Get-ChildItem Env:HTTP_PROXY,Env:HTTPS_PROXY,Env:ALL_PROXY -ErrorAction SilentlyContinue
python -m pip config list -v
```

无意使用代理时，可在当前终端清除：

```powershell
Remove-Item Env:HTTP_PROXY,Env:HTTPS_PROXY,Env:ALL_PROXY -ErrorAction SilentlyContinue
python -m pip config unset global.proxy
```

若终端完全不能联网，可在联网机器下载 wheel：

```powershell
python -m pip download west -d .\wheelhouse
python -m pip download -r .\zephyr-4.4.0\scripts\requirements.txt -d .\wheelhouse
```

把 `wheelhouse` 复制到目标机器后离线安装：

```powershell
python -m pip install --no-index --find-links .\wheelhouse west
python -m pip install --no-index --find-links .\wheelhouse -r .\zephyr-4.4.0\scripts\requirements.txt
```

### 3.3 为什么会提示 unknown command build

`build` 是 Zephyr 提供的 west 扩展命令。出现：

```text
west: unknown command "build"; do you need to run this inside a workspace?
```

通常意味着当前目录不在包含 `.west` 的工作区中，或者 west 找不到 Zephyr 项目。应进入 `zephyr_zip`、`jkcare` 或其子目录后再运行：

```powershell
west topdir
west list zephyr
```

## 4. Zephyr SDK 与 7-Zip

Zephyr SDK 提供交叉编译器和主机工具。安装 ARM 工具链：

```powershell
west sdk install --version 1.0.1 --gnu-toolchains arm-zephyr-eabi
```

Windows 的 SDK 压缩包是 `.7z`。系统必须安装 7-Zip，并且 `7z.exe` 所在目录必须进入 `PATH`。例如：

```powershell
$env:Path = "C:\Program Files\7-Zip;$env:Path"
7z
```

若 `winget` 的 `msstore` 源证书失败，可以明确使用 winget 源：

```powershell
winget install --id 7zip.7zip --exact --source winget
```

SDK 已经解压但 setup 失败时，在 SDK 目录重新运行：

```powershell
cd C:\Users\Owner\zephyr-sdk-1.0.1
cmd /c setup.cmd /t arm-zephyr-eabi
```

如果工具链下载失败，可以在联网机器下载对应的 `toolchain_gnu_windows-x86_64_arm-zephyr-eabi.7z`，复制到离线机器后按 SDK 的包结构安装。网络报错 `unable to resolve host address` 属于 DNS/网络问题，不是 Zephyr 编译错误。

检查安装情况：

```powershell
west sdk list
```

## 5. 构建 JKCare 工程

先激活根目录虚拟环境：

```powershell
cd E:\liaoguangwei\code\zephyr_zip
.\.venv\Scripts\Activate.ps1
```

在工作区根目录构建：

```powershell
west build -p always `
    -b jkcare_apollo4p_blue `
    -d .\jkcare\build `
    .\jkcare
```

也可以进入应用目录：

```powershell
cd .\jkcare
west build -p always -b jkcare_apollo4p_blue .
```

`-b jkcare_apollo4p_blue` 指定的是板，不只是 MCU。即使是自研板，也必须告诉 Zephyr：

- 使用什么 SoC/CPU；
- Flash、SRAM 和时钟如何配置；
- 控制台使用哪个 UART；
- 板上默认启用哪些外设；
- 如何烧录和调试。

`apollo4p_evb` 表示 Ambiq 官方 EVB，其引脚、时钟和外设连接不一定等于 JKCare 自研板。因此早期验证可使用：

```powershell
west build -p always -b apollo4p_evb samples\hello_world
```

正式自研板工程则使用 `jkcare_apollo4p_blue`。

根 `CMakeLists.txt` 中这一行让 Zephyr 能在应用的 `boards/` 下找到自定义板：

```cmake
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
```

如果提示 `No board named 'jkcare_apollo4p_blue' found`，检查：

1. `BOARD_ROOT` 是否在 `find_package(Zephyr ...)` 之前设置；
2. `boards/ambiq/jkcare_apollo4p_blue/board.yml` 是否存在；
3. 板目录和板名是否一致；
4. 使用 `-p always` 或删除旧构建缓存后重新配置。

构建成功后，常见产物位于：

```text
jkcare/build/zephyr/zephyr.elf
jkcare/build/zephyr/zephyr.bin
jkcare/build/zephyr/zephyr.hex
jkcare/build/zephyr/zephyr.dts
jkcare/build/zephyr/.config
```

烧录：

```powershell
west flash -d .\jkcare\build
```

## 6. west build 如何找到工程文件

命令最后一个参数是应用源码目录：

```powershell
west build -b jkcare_apollo4p_blue .\jkcare
```

因此 west/CMake 会使用：

```text
jkcare/CMakeLists.txt
jkcare/prj.conf
jkcare/app.overlay
```

其中：

- `CMakeLists.txt`：应用构建入口，必需且名称固定；
- `prj.conf`：默认应用 Kconfig 配置，通常需要，名称默认固定，但可通过参数换用其他配置；
- `app.overlay`：默认 DeviceTree 覆盖文件，可选；没有应用级硬件修改时可以不存在；
- `src/main.c`：不是 Zephyr 自动识别，必须由 `CMakeLists.txt` 加入构建。

基本 `CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 3.20.0)

list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(jkcare)

target_sources(app PRIVATE src/main.c)
add_subdirectory(drivers/sensor/cht8305c)
```

`target_sources()` 适合直接加入普通应用源码：

```cmake
target_sources(app PRIVATE
    src/main.c
    src/drivers/some_sensor.c
)
```

`add_subdirectory()` 适合拥有独立 `CMakeLists.txt`/Kconfig 的模块化驱动：

```cmake
add_subdirectory(drivers/sensor/cht8305c)
```

驱动目录再按 Kconfig 决定是否加入：

```cmake
if(CONFIG_CHT8305C)
    target_sources(app PRIVATE cht8305c.c)
endif()
```

两种方案不要对同一个 C 文件重复使用，否则可能重复编译或绕过 Kconfig 条件。

## 7. Kconfig 与 prj.conf

`prj.conf` 使用的名字必须是已经由 Zephyr或项目 Kconfig 定义的配置符号。例如：

```conf
CONFIG_GPIO=y
CONFIG_I2C=y
CONFIG_SENSOR=y
CONFIG_LOG=y
CONFIG_LOG_BACKEND_UART=y
CONFIG_SHT4X=y
CONFIG_CHT8305C=y
```

之所以写 `CONFIG_GPIO` 而不是 `TEST_GPIO`，是因为 Zephyr Kconfig 已声明：

```kconfig
config GPIO
    bool "GPIO Drivers"
```

Kconfig 中声明名为 `GPIO`，生成到 C 和 `.config` 后才表现为 `CONFIG_GPIO`。不能随意发明 `TEST_GPIO`，除非项目 Kconfig 中存在：

```kconfig
config TEST_GPIO
    bool "Test GPIO"
```

CHT8305C 的项目 Kconfig 为：

```kconfig
config CHT8305C
    bool "Sensylink CHT8305C temperature and humidity sensor"
    depends on DT_HAS_SENSYLINK_CHT8305C_ENABLED
    select I2C
```

根 `Kconfig` 通过下面的语句引入驱动配置：

```kconfig
rsource "drivers/sensor/cht8305c/Kconfig"
source "Kconfig.zephyr"
```

查看最终生效配置：

```powershell
Select-String -Path .\jkcare\build\zephyr\.config -Pattern "CONFIG_CHT8305C|CONFIG_SHT4X|CONFIG_I2C"
```

也可执行：

```powershell
west build -d .\jkcare\build -t menuconfig
```

## 8. DeviceTree 是什么

DeviceTree 不是单独指某一个源文件，而是描述硬件的一整套输入文件在构建时合并后的硬件树。

本项目主要输入包括：

```text
SoC .dtsi
  + 自定义板 .dts
  + 板级 pinctrl .dtsi
  + app.overlay
  + binding YAML
  = build/zephyr/zephyr.dts
```

最终结果以构建目录中的文件为准：

```text
jkcare/build/zephyr/zephyr.dts
jkcare/build/zephyr/include/generated/zephyr/devicetree_generated.h
```

`.dtsi` 通常是被其他 DTS 文件包含的公共片段；`.dts` 是板级硬件描述入口；`app.overlay` 在不修改板定义的情况下追加或覆盖应用需要的硬件节点。

`app.overlay` 不应再包含完整板级 `.dts`：

```dts
/* 不需要，也不应该这样做 */
#include "jkcare_apollo4p_blue.dts"
```

Zephyr 已经根据 `-b jkcare_apollo4p_blue` 选中了板级 DTS。该 include 还会因为 include 搜索路径和重复 `/dts-v1/` 等原因出错。

## 9. 板级 UART0、I2C0 和引脚复用

### 9.1 UART0 日志输出

板级 DTS 通过 `chosen` 选择控制台：

```dts
/ {
    chosen {
        zephyr,console = &uart0;
        zephyr,shell-uart = &uart0;
    };
};

&uart0 {
    current-speed = <115200>;
    pinctrl-0 = <&uart0_default>;
    pinctrl-names = "default";
    status = "okay";
};
```

`prj.conf` 打开日志及 UART 后端：

```conf
CONFIG_LOG=y
CONFIG_LOG_BACKEND_UART=y
```

因此 `LOG_INF()` 的关系是：

```text
LOG_INF
  → Zephyr logging 子系统
  → UART logging backend
  → chosen: zephyr,console
  → uart0
  → uart0_default 引脚组
```

应用不需要在 `main()` 中再次初始化 UART0，但应理解为 `prj.conf` 和 DeviceTree 只负责启用、配置和实例化设备，不会替应用完成业务层收发操作。

### 9.2 为什么 UART 分两个 group，而 I2C 可放一个 group

当前 pinctrl：

```dts
uart0_default: uart0_default {
    group1 {
        pinmux = <UART0TX_P12>;
    };

    group2 {
        pinmux = <UART0RX_P47>;
        input-enable;
    };
};
```

TX 是输出，RX 需要 `input-enable`。同一个 group 中的属性会应用到该组所有引脚，所以将方向和电气属性不同的引脚分组。

I2C 的 SCL/SDA 都使用开漏、上拉和相同驱动强度，可以放在同一组：

```dts
i2c0_default: i2c0_default {
    group1 {
        pinmux = <M0SCL_P5>, <M0SDAWIR3_P6>;
        drive-open-drain;
        drive-strength = "0.5";
        bias-pull-up;
    };
};
```

`drive-strength` 的单位和允许值由 SoC 的 pinctrl binding/驱动决定，不能跨平台照抄。应查看：

1. 芯片数据手册和引脚电气特性；
2. Zephyr 对应 SoC 的 pinctrl binding；
3. Zephyr 同系列官方开发板 DTS；
4. pinctrl 驱动如何解析该属性。

STM32 的 pinctrl 表达方式不同，通常使用 `STM32_PINMUX()` 和 speed/slew-rate 等该平台 binding 支持的属性。例如具体内容必须根据所用 STM32 型号、AF 编号和 Zephyr 对应 binding 确认，不能直接使用 Ambiq 的 `UART0TX_P12` 或字符串型 `drive-strength`。

### 9.3 `&iom0`、`&i2c0` 和 `&gpio0_31`

Apollo4 的 IOM0 是多功能串行外设控制器，`i2c0` 是在它下面创建的 I2C 总线子节点：

```dts
&iom0 {
    i2c0: i2c {
        pinctrl-0 = <&i2c0_default>;
        pinctrl-names = "default";
        clock-frequency = <I2C_BITRATE_STANDARD>;
        scl-gpios = <&gpio0_31 5 (GPIO_OPEN_DRAIN | GPIO_PULL_UP)>;
        sda-gpios = <&gpio0_31 6 (GPIO_OPEN_DRAIN | GPIO_PULL_UP)>;
        status = "okay";
    };
};
```

- `&iom0`：引用 IOM0 控制器节点，用于配置控制器工作模式；
- `i2c0:`：给子节点起 label，其他文件才能写 `&i2c0`；
- `&gpio0_31`：引用管理 GPIO 0～31 的 GPIO 控制器；
- 后面的 `5`、`6`：该 GPIO 控制器内的引脚编号；
- `M0SCL_P5`、`M0SDAWIR3_P6`：pinctrl 的功能复用编码，告诉引脚连接到 IOM0 的 SCL/SDA 信号。

两种描述不重复：pinctrl 负责引脚复用和电气模式，`scl-gpios`/`sda-gpios` 为驱动提供对应 GPIO 控制器与编号。

传感器是 I2C 总线上的设备，所以应用 overlay 应把传感器挂在 `&i2c0` 下，而不是控制器容器 `&iom0` 下。

## 10. app.overlay 的作用

板级 DTS 描述所有使用该板的应用共同拥有的硬件，例如 MCU、UART0、I2C0 和时钟。`app.overlay` 描述当前应用额外连接的器件，例如 SHT4x 和 CHT8305C：

```dts
&i2c0 {
    status = "okay";

    sht4x: sht4x@44 {
        compatible = "sensirion,sht4x";
        reg = <0x44>;
        repeatability = <2>;
        status = "okay";
    };

    cht8305c: humidity-sensor@40 {
        compatible = "sensylink,cht8305c";
        reg = <0x40>;
        status = "okay";
    };
};
```

节点名 `humidity-sensor@40` 可以按 DT 命名规则选择；`cht8305c:` 是 label，供 C 宏 `DT_NODELABEL(cht8305c)` 引用。真正决定匹配哪个驱动的是：

```dts
compatible = "sensylink,cht8305c";
```

`reg` 是否必需由 binding 决定。CHT8305C binding 包含 `i2c-device.yaml`，I2C 子设备因此需要 `reg` 表示从机地址。一个 binding 包含 `spi-device.yaml` 时则表示它是 SPI 子设备。仅查看传感器名称无法判断总线类型，要查看 binding 的 `include` 和节点所在的父总线。

配置好 overlay 和 `prj.conf` 后，Zephyr 会初始化驱动实例；`main()` 不应重复初始化 I2C/UART/传感器设备。但应用仍需：

- 用 `DEVICE_DT_GET()` 取得设备；
- 用 `device_is_ready()` 检查初始化结果；
- 调用传感器读取、UART 发送等业务接口。

## 11. SHT4x 内置驱动

SHT4x binding 的核心内容：

```yaml
compatible: "sensirion,sht4x"
include: [sensor-device.yaml, i2c-device.yaml]

properties:
  repeatability:
    type: int
    required: true
    enum: [0, 1, 2]
```

由此可知：

- `compatible` 必须写 `sensirion,sht4x`；
- 它是 I2C 设备，所以节点必须位于 I2C 总线下并包含 `reg`；
- `repeatability` 被该 binding 标为必需；
- `reg` 的必需性来自被包含的 `i2c-device.yaml`。

确认 SHT4x 是否编译：

```powershell
Select-String .\jkcare\build\zephyr\.config -Pattern "CONFIG_SHT4X"
Select-String .\jkcare\build\zephyr\zephyr.dts -Pattern "sensirion,sht4x"
Get-ChildItem .\jkcare\build -Recurse -Filter "*sht4x*.obj"
```

## 12. CHT8305C 项目外置驱动

Zephyr 4.4.0 没有使用本项目这份 CHT8305C 驱动，因此它作为 out-of-tree 驱动放在应用仓库中：

```text
jkcare/
├── dts/bindings/
│   ├── vendor-prefixes.txt
│   └── sensor/sensylink,cht8305c.yaml
└── drivers/sensor/cht8305c/
    ├── CMakeLists.txt
    ├── Kconfig
    └── cht8305c.c
```

### 12.1 Binding

```yaml
description: Sensylink CHT8305C digital humidity and temperature sensor
compatible: "sensylink,cht8305c"
include: [sensor-device.yaml, i2c-device.yaml]

properties:
  reg:
    enum: [0x40, 0x44, 0x48, 0x4c]
```

Binding 让构建系统理解节点的属性和总线规则。它本身不执行 I2C 读写，实际驱动逻辑在 `cht8305c.c`。

### 12.2 编译条件

完整生效需要同时满足：

```text
CONFIG_CHT8305C=y
  + cht8305c.c 被 CMake 加入
  + compatible = "sensylink,cht8305c"
  + status = "okay"
```

确认是否编译：

```powershell
Select-String .\jkcare\build\zephyr\.config -Pattern "CONFIG_CHT8305C=y"
Select-String .\jkcare\build\zephyr\zephyr.dts -Pattern "sensylink,cht8305c"
Get-ChildItem .\jkcare\build -Recurse -Filter "*cht8305c*.obj"
```

### 12.3 设备注册过程

驱动开头定义匹配的 compatible：

```c
#define DT_DRV_COMPAT sensylink_cht8305c
```

文件末尾为每个 `status = "okay"` 的匹配节点生成设备：

```c
DT_INST_FOREACH_STATUS_OKAY(CHT8305C_DEFINE)
```

实例宏中：

```c
SENSOR_DEVICE_DT_INST_DEFINE(
    inst,
    cht8305c_init,
    NULL,
    &cht8305c_data_##inst,
    &cht8305c_config_##inst,
    POST_KERNEL,
    CONFIG_SENSOR_INIT_PRIORITY,
    &cht8305c_api
);
```

它在编译期生成静态 Zephyr 设备对象和启动初始化记录，不是运行时动态注册：

```text
DeviceTree 节点
  → compatible 匹配
  → 为每个 enabled 实例展开宏
  → 生成 struct device、config、data 和 init 记录
  → POST_KERNEL 阶段调用 cht8305c_init()
```

`cht8305c_init()` 是启动初始化函数，不在 `cht8305c_api` 函数表中。它检查 I2C 总线并读取芯片 ID；应用无需直接调用。

### 12.4 Sensor API 注册与分派

驱动建立 Zephyr Sensor 通用接口与芯片实现之间的映射：

```c
static DEVICE_API(sensor, cht8305c_api) = {
    .sample_fetch = cht8305c_sample_fetch,
    .channel_get = cht8305c_channel_get,
};
```

应用调用：

```c
sensor_sample_fetch(dev);
sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
```

Zephyr 实际通过 `dev->api` 分派：

```text
sensor_sample_fetch()
  → dev->api->sample_fetch
  → cht8305c_sample_fetch()

sensor_channel_get()
  → dev->api->channel_get
  → cht8305c_channel_get()
```

`SENSOR_CHAN_AMBIENT_TEMP` 和 `SENSOR_CHAN_HUMIDITY` 是 Zephyr 定义的通用 `enum sensor_channel`。并非 DeviceTree 自动识别传感器数据类型，而是驱动的 `switch (channel)` 明确实现：

```c
switch (channel) {
case SENSOR_CHAN_AMBIENT_TEMP:
    /* 返回温度 */
    break;
case SENSOR_CHAN_HUMIDITY:
    /* 返回相对湿度 */
    break;
default:
    return -ENOTSUP;
}
```

应用可以传入任何合法的 `enum sensor_channel`，但未被驱动实现的通道会返回 `-ENOTSUP`。

## 13. 应用读取温湿度

先通过节点 label 获取设备：

```c
static const struct device *const cht8305c =
    DEVICE_DT_GET(DT_NODELABEL(cht8305c));
```

`DEVICE_DT_GET()` 取得编译期生成的设备对象，不负责调用初始化。初始化已在内核启动过程中执行，应用应检查结果：

```c
if (!device_is_ready(cht8305c)) {
    LOG_ERR("CHT8305C is not ready");
    return 0;
}
```

循环读取：

```c
while (true) {
    struct sensor_value temperature;
    struct sensor_value humidity;
    int ret;

    ret = sensor_sample_fetch(cht8305c);
    if (ret < 0) {
        LOG_ERR("CHT8305C sample fetch failed: %d", ret);
        k_sleep(K_SECONDS(5));
        continue;
    }

    ret = sensor_channel_get(cht8305c,
                             SENSOR_CHAN_AMBIENT_TEMP,
                             &temperature);
    if (ret < 0) {
        LOG_ERR("Failed to get temperature: %d", ret);
        k_sleep(K_SECONDS(5));
        continue;
    }

    ret = sensor_channel_get(cht8305c,
                             SENSOR_CHAN_HUMIDITY,
                             &humidity);
    if (ret < 0) {
        LOG_ERR("Failed to get humidity: %d", ret);
        k_sleep(K_SECONDS(5));
        continue;
    }

    LOG_INF("T=%d.%06d C, RH=%d.%06d %%",
            temperature.val1, temperature.val2,
            humidity.val1, humidity.val2);

    k_sleep(K_SECONDS(5));
}
```

通常调用顺序是：

1. `sensor_sample_fetch()` 访问芯片并把最新原始数据保存到驱动 `data`；
2. `sensor_channel_get()` 按通道从已保存数据中取值并转换为 `struct sensor_value`；
3. 每次调用都检查返回值。

## 14. 驱动源码与 build 中的 OBJ

`build/modules/.../am_hal_uart.c.obj` 是编译产物，不是驱动源文件。Apollo4 HAL 源码通常在 west 模块目录，例如：

```text
modules/hal/ambiq/...
```

实际目录布局取决于 manifest 中项目的 `path`。可以用以下命令确认：

```powershell
west list hal_ambiq
west list cmsis_6
```

也可以从构建信息反查某个 `.obj` 对应的源文件，但不要修改 build 目录；重新构建会覆盖其中内容。

Zephyr 通用驱动通常位于：

```text
zephyr-4.4.0/drivers/
```

SoC 支持位于：

```text
zephyr-4.4.0/soc/ambiq/apollo4x/
```

厂商底层 HAL 通常位于 west 模块：

```text
modules/hal/ambiq/
```

三者职责分别是 Zephyr 驱动模型、SoC 集成，以及厂商寄存器/HAL 实现。

## 15. 新 MCU 接入 Zephyr 的最低工作

CPU 架构与 MCU 型号不是同一层概念。Cortex-M4 是 ARM CPU 内核架构，Zephyr 已支持；HC32F467 是包含 Cortex-M4 内核、时钟、Flash、GPIO、UART、I2C 等外设的具体 MCU。CPU 架构已支持并不意味着该 MCU 已可直接使用。

一个尚未被 Zephyr 支持的 MCU，最低通常需要：

1. SoC Kconfig 和 CMake 集成；
2. SoC DeviceTree `.dtsi` 与 binding；
3. 启动、链接脚本、内存布局和中断定义；
4. 时钟、pinmux/pinctrl、GPIO、串口等最基础驱动；
5. 一块 board 定义，包括 `.dts`、defconfig、board metadata；
6. 烧录/调试 runner 或可用的外部烧录方法；
7. 基础样例和测试验证。

如果厂商已有 CMSIS 启动文件和 HAL，可以复用其寄存器定义及底层实现，但仍需要接入 Zephyr 的设备模型、Kconfig、DeviceTree、初始化阶段和驱动 API。

## 16. VS Code 宏跳转与 IntelliSense

只打开 `jkcare` 时，VS Code 默认看不到 Zephyr 源码、生成头文件和模块，因此大量宏会显示未识别。建议将整个 `zephyr_zip` 作为工作区打开，或者建立多根工作区，把以下目录同时加入：

- `jkcare`；
- `zephyr-4.4.0`；
- `modules`；
- `jkcare/build` 中的生成头文件。

先成功构建一次，因为许多 DeviceTree/Kconfig 宏只有配置后才会生成。建议导出编译数据库：

```powershell
west build -d .\jkcare\build -b jkcare_apollo4p_blue .\jkcare -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

然后让 C/C++ 扩展或 clangd 使用：

```text
jkcare/build/compile_commands.json
```

对于复杂 Zephyr 宏，普通“转到定义”仍可能较慢，因为它们经过多层宏展开；以实际构建结果、`zephyr.dts`、`.config` 和生成头文件为最终依据。

## 17. 常用诊断命令

```powershell
# 当前工作区根目录
west topdir

# Zephyr 和模块实际路径
west list zephyr
west list hal_ambiq

# 列出可用开发板并筛选 Apollo4
west boards | Select-String "apollo4|jkcare"

# 强制重新配置构建
west build -p always -b jkcare_apollo4p_blue -d .\jkcare\build .\jkcare

# 检查最终 Kconfig
Select-String .\jkcare\build\zephyr\.config -Pattern "CONFIG_CHT8305C|CONFIG_SHT4X"

# 检查最终 DeviceTree
Select-String .\jkcare\build\zephyr\zephyr.dts -Pattern "cht8305c|sht4x|i2c0|uart0"

# 查找驱动对象文件
Get-ChildItem .\jkcare\build -Recurse -Filter "*cht8305c*.obj"
```

排查 Zephyr 构建问题时，应优先查看 CMake 输出中第一条真正的错误。不要把输出中的：

```text
-- Configuring incomplete, errors occurred!
```

重新粘贴到 PowerShell 执行；开头的 `--` 会被 PowerShell 当成运算符，因此产生“一元运算符后面缺少表达式”，这只是误执行日志，不是新的 Zephyr 错误。

## 18. 当前临时引脚分配

当前引脚暂时参考 Apollo4 Blue Plus KXR EVB：

| 功能 | 引脚 |
|---|---:|
| UART0 TX | GPIO 12 |
| UART0 RX | GPIO 47 |
| I2C0 SCL | GPIO 5 |
| I2C0 SDA | GPIO 6 |
| LED（高电平点亮） | GPIO 7 |
| Touch（下拉、上升沿中断） | GPIO 8 |
| 32 MHz clock output | GPIO 46 |
| 32 kHz clock | GPIO 4 |

这些配置必须结合 JKCare 原理图和 Apollo4 Plus Blue 数据手册复核。正式硬件确定后，应同步修改：

```text
boards/ambiq/jkcare_apollo4p_blue/jkcare_apollo4p_blue-pinctrl.dtsi
boards/ambiq/jkcare_apollo4p_blue/jkcare_apollo4p_blue.dts
```

尤其需要保证 pinmux 编码、I2C GPIO 描述、外部上拉、UART 电平和晶振连接与实际 PCB 一致。

当前应用级 GPIO 定义位于 `app.overlay` 的 `/zephyr,user` 节点：

```dts
zephyr,user {
    led-gpios = <&gpio0_31 7 GPIO_ACTIVE_HIGH>;
    touch-gpios = <&gpio0_31 8 (GPIO_PULL_DOWN | GPIO_ACTIVE_HIGH)>;
};
```

启动时 `gpio_init()` 将 LED 配置为低电平初始状态，将 touch 配置为输入并注册物理上升沿中断。中断服务函数只提交系统 work；work handler 负责翻转 LED 并输出日志，避免在中断上下文中执行较重操作。GPIO 7 和 GPIO 8 是未取得最终原理图时的临时分配，必须按实际 PCB 修改。

## 19. 本地构建、静态嵌入与运行时动态加载 LLEXT

当前 PPG、ECG 插件采用以下验证方案：

```text
插件源码
  → 在JKCare主工程中编译成独立.llext ELF
  → 构建时转换成.inc十六进制文本
  → 静态嵌入zephyr.elf
  → MCU启动时由llext_load()动态解析、重定位和加载
  → 查询统一插件API并启动插件线程
```

这种方式应准确称为：

```text
部署时静态打包，运行时动态加载
```

它适合验证 LLEXT、插件 ABI、符号查找和线程生命周期。因为插件仍被打包在主固件中，所以更新插件后需要重新链接并烧录主固件；它还不是外部 Flash 中插件的独立升级方案。

### 19.1 插件目录和公共接口

```text
jkcare/
├── include/
│   ├── jkcare_plugin.h
│   └── jkcare_plugin_manager.h
├── plugins/
│   ├── ppg/ppg_plugin.c
│   └── ecg/ecg_plugin.c
└── src/
    ├── main.c
    └── plugin_manager.c
```

主固件和插件共同使用 `include/jkcare_plugin.h`。所有插件必须导出同名对象：

```c
const struct jkcare_plugin_api jkcare_plugin = {
    .abi_version = JKCARE_PLUGIN_ABI_VERSION,
    .struct_size = sizeof(struct jkcare_plugin_api),
    .name = "PPG business",
    .init = ppg_init,
    .start = ppg_start,
    .stop = ppg_stop,
    .deinit = ppg_deinit,
};

EXPORT_SYMBOL(jkcare_plugin);
```

PPG 和 ECG 可以导出相同的 `jkcare_plugin`，因为它们位于两个独立的 ELF 文件中，不会在主固件静态链接阶段发生同名符号冲突。

### 19.2 CMake 如何生成插件目标

根 `CMakeLists.txt` 当前包含：

```cmake
foreach(plugin_name ppg ecg)
    set(plugin_binary
        ${PROJECT_BINARY_DIR}/llext/${plugin_name}_plugin.llext
    )
    set(plugin_include
        ${ZEPHYR_BINARY_DIR}/include/generated/${plugin_name}_plugin.inc
    )

    add_llext_target(${plugin_name}_plugin
        OUTPUT ${plugin_binary}
        SOURCES plugins/${plugin_name}/${plugin_name}_plugin.c
    )

    generate_inc_file_for_target(
        app
        ${plugin_binary}
        ${plugin_include}
    )
endforeach()
```

循环展开后创建两个构建目标：

```text
ppg_plugin
ecg_plugin
```

目标名来自 `add_llext_target()` 的第一个参数，不是 west 根据文件名自动推断的。可用以下命令确认：

```powershell
west build -d build -t help |
    Select-String "ppg_plugin|ecg_plugin"
```

### 19.3 首次配置与完整构建

新建或清理构建目录后必须指定开发板：

```powershell
cd E:\liaoguangwei\code\zephyr_zip\jkcare

west build -p always `
    -b jkcare_apollo4p_blue `
    -d build `
    .
```

`-p always` 会清除 `CMakeCache.txt`。如果只执行：

```powershell
west build -p always -d build -t ppg_plugin
```

缓存中的 BOARD 会被删除，而命令又没有 `-b`，因此会报 `BOARD is not being defined`。首次配置或使用 `-p always` 时必须同时指定：

```powershell
-b jkcare_apollo4p_blue
```

### 19.4 单独构建某个插件

构建目录已经使用正确开发板配置后，单独构建 PPG：

```powershell
west build -d build -t ppg_plugin
```

单独构建 ECG：

```powershell
west build -d build -t ecg_plugin
```

生成文件：

```text
build/llext/ppg_plugin.llext
build/llext/ppg_plugin_debug.elf
build/llext/ecg_plugin.llext
build/llext/ecg_plugin_debug.elf
```

其中：

| 文件 | 用途 |
|---|---|
| `*_plugin.llext` | 实际由 LLEXT loader 加载的插件 ELF |
| `*_plugin_debug.elf` | 保留更多符号，用于 GDB、反汇编和定位问题 |
| `*.obj` | 编译/部分链接过程中的中间产物 |

如果显示：

```text
ninja: no work to do.
```

表示源码、公共头文件和编译配置没有变化，现有 `.llext` 已经是最新产物，并不是错误。

### 19.5 `.inc` 文件是什么

`.inc` 是构建系统生成的 C 文本片段，不是另一个插件格式。它把 `.llext` 的所有二进制字节转换为十六进制常量：

```c
0x7f, 0x45, 0x4c, 0x46, 0x01, 0x01, 0x01, 0x00,
/* 后续是完整ELF的字节 */
```

开头的 `0x7f, 'E', 'L', 'F'` 是 ELF magic。插件管理器通过预处理包含它：

```c
static uint8_t ppg_plugin_image[] = {
#include "ppg_plugin.inc"
};

static uint8_t ecg_plugin_image[] = {
#include "ecg_plugin.inc"
};
```

预处理后相当于：

```c
static uint8_t ppg_plugin_image[] = {
    0x7f, 0x45, 0x4c, 0x46,
    /* ... */
};
```

生成位置：

```text
build/zephyr/include/generated/ppg_plugin.inc
build/zephyr/include/generated/ecg_plugin.inc
```

单独执行：

```powershell
west build -d build -t ecg_plugin
```

只要求生成 `.obj` 和 `.llext`，不会要求生成 `.inc`。这是因为 `generate_inc_file_for_target(app ...)` 把 `.inc` 绑定到了主固件 `app` 目标。要生成 `.inc` 并重新嵌入主固件，应继续执行：

```powershell
west build -d build
```

检查生成文件：

```powershell
Get-ChildItem .\build `
    -Recurse `
    -Filter "*plugin.inc"
```

### 19.6 当前运行时加载过程

`plugin_manager.c` 建立镜像表：

```c
static const struct plugin_image plugin_images[] = {
    { "ppg", ppg_plugin_image, sizeof(ppg_plugin_image) },
    { "ecg", ecg_plugin_image, sizeof(ecg_plugin_image) },
};
```

`main()` 调用：

```c
jkcare_plugin_manager_load_all();
```

插件管理器对每个镜像执行：

```text
LLEXT_BUF_LOADER(image->data, image->size)
  → llext_load()
  → 查询导出符号jkcare_plugin
  → 检查abi_version和struct_size
  → plugin->init(&host_api)
  → plugin->start()
  → 插件通过Host API申请业务线程
```

核心代码近似为：

```c
struct llext_buf_loader buffer_loader =
    LLEXT_BUF_LOADER(image->data, image->size);

ret = llext_load(
    &buffer_loader.loader,
    image->name,
    &plugin->extension,
    &load_param
);

plugin->api = llext_find_sym(
    &plugin->extension->exp_tab,
    "jkcare_plugin"
);

plugin->api->init(&host_api);
plugin->api->start();
```

`start()` 创建线程后立即返回，PPG/ECG 业务线程随后独立运行；`main()` 不需要循环调用插件 `process()`。

### 19.7 插件线程和安全卸载

当前插件通过 Host API 请求主固件创建线程：

```c
host->thread_create(
    &worker,
    "ppg_plugin",
    ppg_thread,
    NULL,
    2048,
    5
);
```

主固件管理线程控制块和线程栈，插件管理线程入口及业务状态。卸载必须按以下顺序：

```text
plugin->stop()
  → running=false
  → 唤醒或等待业务线程退出
  → host->thread_join()
  → plugin->deinit()
  → llext_unload()
```

禁止在线程、timer、work、ISR 回调或函数指针仍引用插件代码时调用 `llext_unload()`，否则下一次执行会跳入已经释放的地址并触发 Fault。

### 19.8 修改插件后的完整验证流程

例如修改 `plugins/ppg/ppg_plugin.c` 后：

```powershell
# 只重新生成PPG的.llext
west build -d build -t ppg_plugin

# 重新生成ppg_plugin.inc并链接主固件
west build -d build

# 烧录包含新插件的主固件
west flash -d build
```

确认产物：

```powershell
Get-ChildItem .\build\llext\*.llext |
    Select-Object Name, Length, LastWriteTime

Get-ChildItem .\build\zephyr\include\generated\*plugin.inc |
    Select-Object Name, Length, LastWriteTime
```

### 19.9 当前方案与外部 Flash 方案的边界

当前方案：

```text
.llext → .inc → zephyr.elf → west flash
```

外部 Flash/文件系统方案：

```text
.llext → 单独写入/lfs/plugins/ → LLEXT_FS_LOADER
```

切换到外部文件系统后，应删除：

```cmake
generate_inc_file_for_target(...)
```

以及：

```c
#include "ppg_plugin.inc"
#include "ecg_plugin.inc"
```

插件管理器改为扫描 `/lfs/plugins`，对所有 `.llext` 文件执行统一的加载、ABI校验和注册流程。外部 Flash 方案只需要发布 `.llext`，不需要 `.inc`，也不需要把插件源码列在主固件 CMake 中。
