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

## 19. 第一种 LLEXT 方式：静态嵌入（历史验证方案）

本项目早期使用以下验证方案，当前代码已经切换到第 20 章的文件系统加载方式。本章保留用于解释 `.inc` 和两种方案的区别：

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

## 20. 当前方案：插件动态编译与文件系统动态加载

这里的“动态”包含两个彼此独立的阶段：

- **动态编译/独立发布**：PPG、ECG 等插件不参与主固件链接，使用主固件生成的 LLEXT EDK 单独编译成 `.llext`；更新插件时不需要重新链接主固件。
- **动态加载/运行**：MCU 启动后，主固件扫描文件系统，调用 `llext_load()` 将插件的可加载段复制并重定位到可执行 RAM，然后查询统一 ABI 并调用插件的 `init()`、`start()`。

完整链路如下：

```text
主固件配置和编译
  → 生成llext-edk.tar.xz
  → 用EDK独立编译ppg_plugin.llext/ecg_plugin.llext
  → 将.llext写入文件系统/llext/business
  → MCU初始化存储并挂载/llext
  → 主固件扫描所有.llext文件
  → llext_load()分配RAM、复制section、解析符号并重定位
  → 查找导出对象jkcare_plugin
  → ABI校验
  → init(&host_api)
  → start()
  → 插件业务线程独立运行
```

因此，`.llext` 文件长期保存在 Flash/文件系统，运行期间使用的是已经加载到 RAM 的代码和数据。普通文件系统不要求外部 Flash 支持 XIP，也不能直接把文件系统中的字节当作指令执行。

当前代码假设系统已经将某个文件系统挂载在：

```text
/llext
```

业务插件存放目录：

```text
/llext/business/
├── ppg_plugin.llext
├── ecg_plugin.llext
└── 后续其他业务插件.llext
```

`jkcare_plugin_manager_load_all()` 不再包含固定的 PPG/ECG 镜像，而是扫描该目录中的所有 `.llext` 文件。新增插件不需要修改主固件 CMake或镜像数组，只要插件遵守 `jkcare_plugin.h` ABI并被写入该目录即可。

### 20.1 主固件与文件系统的职责

当前代码只实现插件目录扫描和文件加载，不负责初始化具体 Flash 芯片、创建分区、格式化或挂载文件系统。实际产品必须在调用插件管理器前完成：

```text
初始化内部Flash/外部SPI NOR
  → 找到文件系统分区
  → 挂载LittleFS等文件系统到/llext
  → 确保/llext/business目录存在
  → jkcare_plugin_manager_load_all()
```

如果文件系统没有挂载，启动日志会出现：

```text
Cannot open plugin directory /llext/business: ...; mount the filesystem first
```

`prj.conf` 当前启用通用文件系统和 LLEXT：

```conf
CONFIG_FILE_SYSTEM=y
CONFIG_LLEXT=y
CONFIG_LLEXT_HEAP_SIZE=64
CONFIG_LLEXT_TYPE_ELF_RELOCATABLE=y
```

实际使用 LittleFS 时还需要根据存储硬件增加类似配置：

```conf
CONFIG_FLASH=y
CONFIG_FLASH_MAP=y
CONFIG_FILE_SYSTEM_LITTLEFS=y
```

具体 Flash、分区和挂载配置必须根据实际硬件确定，不能仅打开 Kconfig 就自动得到 `/llext`。

### 20.2 主固件不再编译插件

主固件根 `CMakeLists.txt` 当前只编译应用和插件管理器：

```cmake
zephyr_include_directories(include)

target_sources(app PRIVATE
    src/main.c
    src/plugin_manager.c
)
```

其中已经删除：

```cmake
add_llext_target(...)
generate_inc_file_for_target(...)
```

因此完整构建主固件不会生成或嵌入 PPG/ECG，也不会生成 `ppg_plugin.inc`、`ecg_plugin.inc`：

```powershell
west build -p always `
    -b jkcare_apollo4p_blue `
    -d build `
    .
```

主固件产物与插件产物相互独立：

```text
主固件：build/zephyr/zephyr.elf、zephyr.bin、zephyr.hex
PPG插件：plugins/ppg/build-standalone/ppg_plugin.llext
ECG插件：plugins/ecg/build-standalone/ecg_plugin.llext
```

### 20.3 插件目录扫描

插件管理器中的目录配置：

```c
#define PLUGIN_DIRECTORY   "/llext/business"
#define PLUGIN_FILE_SUFFIX ".llext"
```

扫描过程：

```text
fs_opendir("/llext/business")
  → fs_readdir()
  → 忽略子目录和非.llext文件
  → 拼接完整文件路径
  → LLEXT_FS_LOADER(path)
  → llext_load()
```

核心逻辑近似为：

```c
while (fs_readdir(&directory, &entry) == 0 &&
       entry.name[0] != '\0') {
    if (!is_plugin_file(&entry)) {
        continue;
    }

    snprintk(path, sizeof(path), "%s/%s",
             PLUGIN_DIRECTORY, entry.name);

    load_and_start_plugin(path, entry.name);
}
```

文件名去掉 `.llext` 后作为 LLEXT实例名：

```text
ppg_plugin.llext → ppg_plugin
ecg_plugin.llext → ecg_plugin
```

文件名必须能放入 `LLEXT_MAX_NAME_LEN + 1`。不同插件应使用不同文件名，避免 LLEXT按相同名称复用已加载实例。

### 20.4 从文件读取后加载到哪里

`LLEXT_FS_LOADER` 使用 `LLEXT_STORAGE_TEMPORARY`。文件系统只负责长期保存 ELF；加载时 Zephyr把运行所需区域复制到 LLEXT RAM heap：

```text
/llext/business/ppg_plugin.llext
  → 读取ELF header和section
  → 在LLEXT heap分配.text/.rodata/.data/.bss
  → 复制section并清零.bss
  → 处理符号和重定位
  → 刷新数据/指令缓存
  → 关闭插件文件
  → CPU执行RAM中的.text
```

当前：

```conf
CONFIG_LLEXT_HEAP_SIZE=64
```

会在 MCU RAM中静态预留 64 KiB LLEXT heap。它需要容纳所有同时加载插件的代码、只读数据、读写数据、BSS、导出符号和分配管理开销。插件业务线程栈由主固件的 `plugin_thread_stacks` 单独提供，不在这 64 KiB 中。

### 20.5 加载和启动统一插件 API

每个文件加载成功后，管理器执行：

```text
llext_find_sym("jkcare_plugin")
  → 检查abi_version
  → 检查struct_size
  → plugin->init(&host_api)
  → plugin->start()
  → 插件请求创建自己的业务线程
```

某个插件失败不会阻止目录中后续文件被扫描；函数最终返回第一个错误，并保留已成功启动的插件。当前最多允许：

```c
#define MAX_PLUGINS        8
#define MAX_PLUGIN_THREADS 8
```

超过限制的插件会返回 `-ENOMEM`，后续可以根据 RAM和产品需求调整。

### 20.6 生成主固件专用 LLEXT EDK

独立插件仍必须与主固件使用相同架构、编译 ABI、LLEXT格式和导出符号。先构建主固件：

```powershell
cd E:\liaoguangwei\code\zephyr_zip\jkcare

west build -p always `
    -b jkcare_apollo4p_blue `
    -d build `
    .
```

然后生成 Extension Development Kit：

```powershell
west build -d build -t llext-edk
```

产物：

```text
build/zephyr/llext-edk.tar.xz
```

Windows的 `tar` 若提示无法运行 `xz`，可用 7-Zip分两层解压：

```powershell
$EdkRoot = "E:\jkcare-plugin-sdk"
New-Item -ItemType Directory -Force $EdkRoot

& "C:\Program Files\7-Zip\7z.exe" x `
    .\build\zephyr\llext-edk.tar.xz `
    "-o$EdkRoot" -y

& "C:\Program Files\7-Zip\7z.exe" x `
    "$EdkRoot\llext-edk.tar" `
    "-o$EdkRoot" -y
```

最终必须存在：

```text
E:\jkcare-plugin-sdk\llext-edk\cmake.cflags
```

每次修改以下内容后都应重新生成 EDK并重新编译插件：

- Zephyr版本或工具链；
- `jkcare_plugin.h`、Host API或ABI版本；
- 目标SoC、浮点ABI；
- LLEXT类型和主固件导出符号；
- 影响插件编译兼容性的 Kconfig。

### 20.7 独立插件 CMake结构

PPG 和 ECG 目录现在各自拥有独立 `CMakeLists.txt`：

```text
plugins/
├── cmake/
│   ├── toolchain.cmake
│   └── JkcareLlextPlugin.cmake
├── ppg/
│   ├── CMakeLists.txt
│   └── ppg_plugin.c
└── ecg/
    ├── CMakeLists.txt
    └── ecg_plugin.c
```

插件构建不使用 `west build`，不调用 `find_package(Zephyr)`，也不需要再次指定 `-b jkcare_apollo4p_blue`。目标板和 ABI已经固化在主固件生成的 EDK中。

公共 ABI头文件仍为：

```text
include/jkcare_plugin.h
```

如果插件代码移动到另一个仓库或另一台电脑，必须同时发布该头文件，或通过 `JKCARE_PLUGIN_API_DIR` 指向插件 SDK 中的兼容版本。

### 20.8 单独编译 PPG 插件

设置环境变量：

```powershell
$env:ZEPHYR_SDK_INSTALL_DIR = `
    "C:\Users\Owner\zephyr-sdk-1.0.1"

$env:LLEXT_EDK_INSTALL_DIR = `
    "E:\jkcare-plugin-sdk\llext-edk"
```

配置和编译：

```powershell
cd E:\liaoguangwei\code\zephyr_zip\jkcare\plugins\ppg

cmake -S . -B build-standalone -G Ninja
cmake --build build-standalone
```

产物：

```text
plugins/ppg/build-standalone/ppg_plugin.llext
```

修改 `ppg_plugin.c` 后只需：

```powershell
cmake --build build-standalone
```

### 20.9 单独编译 ECG 插件

环境变量与 PPG相同：

```powershell
cd E:\liaoguangwei\code\zephyr_zip\jkcare\plugins\ecg

cmake -S . -B build-standalone -G Ninja
cmake --build build-standalone
```

产物：

```text
plugins/ecg/build-standalone/ecg_plugin.llext
```

现在不再存在：

```powershell
west build -d build -t ppg_plugin
west build -d build -t ecg_plugin
```

这些曾经是主固件 CMake创建的目标，移除 `add_llext_target()` 后，主固件构建系统不会再识别它们。独立插件统一使用：

```powershell
cmake -S . -B build-standalone -G Ninja
cmake --build build-standalone
```

### 20.10 将插件写入文件系统

独立编译后，需要通过实际产品提供的存储通道将文件复制为：

```text
/llext/business/ppg_plugin.llext
/llext/business/ecg_plugin.llext
```

传输方式可以是 UART、USB、BLE、网络下载、SD卡，或者开发阶段使用 Flash编程工具写入预生成的文件系统镜像。`west flash` 默认只烧录主固件，不会自动把独立 `.llext` 复制到外部文件系统。

正式升级建议增加：

```text
临时文件写入
  → 检查文件长度
  → SHA-256
  → 数字签名
  → fs_sync()/关闭文件
  → 停止旧插件
  → 原子重命名为正式文件
  → 加载新插件
  → 失败时回滚
```

不要覆盖正在运行插件对应的文件并立即卸载。应先保证插件线程、work、timer和所有回调完全停止，再调用 `llext_unload()`。

### 20.11 使用 PSRAM 作为 LLEXT 运行内存

当前配置：

```conf
CONFIG_LLEXT_HEAP_SIZE=64
```

表示链接时在 MCU 内部 RAM 静态预留 64 KiB。若希望运行时将插件区域放进 PSRAM，应改用动态 LLEXT heap：

```conf
CONFIG_LLEXT=y
CONFIG_LLEXT_HEAP_DYNAMIC=y
CONFIG_LLEXT_HEAP_K_HEAP=y
CONFIG_LLEXT_TYPE_ELF_RELOCATABLE=y

# 动态模式下不再设置：
# CONFIG_LLEXT_HEAP_SIZE=64
```

PSRAM 完成初始化和内存映射后，在加载任何插件之前把真实地址范围交给 LLEXT：

```c
#include <zephyr/llext/llext.h>

static int plugin_heap_init(void)
{
    void *psram_base = /* 板级PSRAM可执行映射起始地址 */;
    size_t psram_size = /* 专门划给LLEXT的字节数 */;

    return llext_heap_init(psram_base, psram_size);
}
```

正确启动顺序为：

```text
初始化PSRAM控制器
  → 建立CPU可寻址的memory-mapped区域
  → 配置MPU/cache，保证目标区域允许取指
  → llext_heap_init(psram_base, psram_size)
  → 初始化Flash并挂载文件系统到/llext
  → jkcare_plugin_manager_load_all()
```

`llext_heap_init()` 指定的是整个 LLEXT 内存池，不是某个插件的固定装载地址。每个插件的 `.text`、`.rodata`、`.data` 和 `.bss` 由 LLEXT 在该池中动态分配。卸载全部插件后，如果要关闭或重新分配 PSRAM，调用：

```c
jkcare_plugin_manager_stop_all();
llext_heap_uninit();
```

使用 PSRAM 时必须同时满足：

- PSRAM 是 CPU 可直接寻址的内存映射区域；仅能通过 SPI 收发函数访问的 PSRAM 不能作为执行 heap。
- Cortex-M/SoC 的 MPU 与总线必须允许从该区域执行指令（不能是 XN 区域）。
- 数据缓存和指令缓存必须由平台及 LLEXT 正确维护。
- 分配给 LLEXT 的区域不能与 framebuffer、通用 heap、DMA buffer 等重叠。
- 插件运行期间 PSRAM 不能掉电或退出映射模式。

“外部 Flash 文件系统保存插件”和“PSRAM 执行插件”是两件事：前者解决掉电保存与升级，后者提供运行内存。常见组合是：

```text
SPI NOR/LittleFS长期保存.llext
  → 启动或按需读取
  → LLEXT加载到memory-mapped PSRAM
  → CPU从PSRAM执行插件代码
```

当前插件线程栈仍由主固件中的 `plugin_thread_stacks` 静态提供，默认位于内部 RAM；把 LLEXT heap 切到 PSRAM并不会自动移动线程栈。

> 注意：PSRAM 起始地址、容量、可执行属性和缓存配置必须以 Apollo4 Plus Blue 的实际芯片手册、BSP 和原理图为准，不能直接照搬示例地址。

### 20.12 从零发布一个插件的操作清单

首次准备插件 SDK：

```powershell
cd E:\liaoguangwei\code\zephyr_zip\jkcare

west build -p always `
    -b jkcare_apollo4p_blue `
    -d build `
    .

west build -d build -t llext-edk
```

解压 EDK 并设置 `LLEXT_EDK_INSTALL_DIR` 后，独立编译插件：

```powershell
cd .\plugins\ppg
cmake -S . -B build-standalone -G Ninja
cmake --build build-standalone

cd ..\ecg
cmake -S . -B build-standalone -G Ninja
cmake --build build-standalone
```

发布以下文件，不发布 `.obj`，文件系统加载模式也不需要 `.inc`：

```text
plugins/ppg/build-standalone/ppg_plugin.llext
  → /llext/business/ppg_plugin.llext

plugins/ecg/build-standalone/ecg_plugin.llext
  → /llext/business/ecg_plugin.llext
```

设备启动前确认文件系统已挂载，随后调用：

```c
int ret = jkcare_plugin_manager_load_all();
```

加载成功的关键日志应包含扫描目录以及每个插件的启动信息。若新增插件，只需保持 `jkcare_plugin.h` ABI兼容、独立生成新的 `.llext` 并放入相同目录；主固件不需要提前知道插件业务名称，也不需要增加固定的 `for` 调用。主固件只需要保留通用的目录扫描、ABI校验、Host API和生命周期管理能力。

### 20.13 掉电保存、SD 卡失效与 Flash XIP

设计插件存储时必须区分两个概念：

```text
掉电保存位置 ≠ 插件运行位置
```

当前推荐架构是：

```text
内部Flash/MRAM或板载SPI NOR
  → 以文件或原始分区永久保存.llext
  → 每次启动时读取ELF
  → 加载并重定位到内部RAM或memory-mapped PSRAM
  → 运行插件
```

掉电后 RAM/PSRAM 中的运行副本会消失，但非易失存储中的 `.llext` 不会丢失。重新上电后再次调用 `jkcare_plugin_manager_load_all()` 即可恢复插件。因此，为了实现掉电保存，通常不需要让 CPU 直接在 Flash 中执行 LLEXT。

#### 20.13.1 不要把 SD 卡作为关键插件的唯一来源

SD 卡适合作为升级包输入或容量扩展，但可能被拔出、接触不良或出现文件系统损坏。PPG、ECG 等必须可用的业务插件建议保存在：

- MCU 内部 Flash/MRAM 的独立分区；
- 板载焊接的 SPI NOR Flash；
- 内部非易失存储上的 LittleFS；
- 板载 SPI NOR 上的 LittleFS。

推荐的数据流是：

```text
SD卡/UART/USB/BLE/网络收到升级包
  → 校验升级包
  → 写入内部Flash或板载SPI NOR的备用插件槽
  → 验证写入结果
  → 切换有效插件版本
  → 下次启动从非易失插件区加载到RAM/PSRAM
```

这样 SD 卡不存在或损坏时，系统仍能使用已经安装在板载非易失存储中的插件。

#### 20.13.2 当前文件系统 loader 不是 Flash XIP

当前代码使用：

```c
LLEXT_FS_LOADER("/llext/business/ppg_plugin.llext")
```

`LLEXT_FS_LOADER` 的存储类型是 `LLEXT_STORAGE_TEMPORARY`，而且文件系统 loader 不提供可直接引用文件内容的 `peek` 地址。加载时会执行：

```text
文件系统中的.llext
  → 读取ELF header和section
  → 为.text/.rodata/.data/.bss分配运行内存
  → 复制并进行符号解析和重定位
  → CPU从RAM/PSRAM中的.text取指
```

即使 LittleFS 位于 memory-mapped Flash 中，文件路径和文件偏移也不等于稳定的 CPU 可执行地址；文件数据还可能被文件系统分块、搬移或附加元数据。因此，普通文件系统中的 `.llext` 不能自动实现 XIP。

#### 20.13.3 直接从 Flash 运行 LLEXT 的条件

Zephyr 还提供持久缓冲区 loader：

```c
struct llext_buf_loader loader =
    LLEXT_PERSISTENT_BUF_LOADER(image_address, image_size);
```

它表示 ELF 缓冲区在插件整个加载和运行期间始终有效。LLEXT可以尝试直接复用其中满足条件的只读区域，但不保证整个插件都留在 Flash 中执行。直接复用 `.text` 至少需要：

- 插件镜像位于 CPU 可直接寻址的 memory-mapped Flash/MRAM；
- 该地址区域允许 CPU 取指，不是 XN 区域；
- `.text` 满足架构、MPU和缓存要求的地址与对齐；
- `.text` 不需要运行时修改或重定位，或者插件已经提前定位；
- 镜像地址在插件卸载前保持不变。

以下区域通常仍然需要 RAM：

- `.data`：运行时可读写变量；
- `.bss`：文件中没有初始数据，需要分配并清零；
- 包含重定位项、加载时必须修改的 `.text` 或 `.rodata`；
- LLEXT元数据、插件线程栈和其他运行状态。

当前插件使用：

```conf
CONFIG_LLEXT_TYPE_ELF_RELOCATABLE=y
```

可重定位 ELF 的代码通常包含需要 loader 修正的重定位项，而普通 Flash 不能像 RAM一样直接修改，所以当前构建产物通常仍需将代码复制到可写、可执行的 RAM/PSRAM。若要实现真正 XIP，需要额外设计固定 Flash槽位、专用链接脚本、预定位插件、镜像编程流程和自定义加载策略；这会降低插件任意装载和升级的灵活性，不建议作为当前第一阶段方案。

#### 20.13.4 推荐的可靠存储和回退结构

产品中可以采用以下结构：

```text
主固件
├── 插件管理器与恢复逻辑
├── 必要时内置最小可用业务
└── Host API和ABI校验

内部Flash/MRAM或板载SPI NOR
├── plugin_a槽
├── plugin_b槽
├── manifest/有效版本标记
└── 可选LittleFS

SD卡
└── 仅作为插件升级包输入，不作为唯一运行来源
```

插件升级建议采用：

```text
写入临时文件或备用槽
  → 检查文件长度
  → 校验SHA-256
  → 验证数字签名
  → fs_sync()/完成Flash写入
  → 重新读取并验证
  → 原子切换有效版本标记
  → 加载新插件
  → 失败时回退旧版本或内置版本
```

最终建议组合为：

```text
内部Flash/MRAM或板载SPI NOR：掉电保存.llext
内部RAM或可执行PSRAM：加载后运行插件
SD卡：只负责导入升级包
主固件：负责校验、A/B切换、回退和安全卸载
```

该方案既能保证掉电后插件仍然存在，也能避免 SD 卡损坏导致整个业务系统无法启动，同时保留 LLEXT动态升级和按需加载的能力。

## 21. Apollo4 Plus 自定义 RAM节区

### 21.1 为什么需要自定义节区

默认情况下，Zephyr把普通全局变量、`.data`、`.bss`、线程栈和内核对象统一放进 `chosen` 的 `zephyr,sram`。Apollo4 Plus内部存在多种性能、功耗和共享属性不同的 RAM，因此本工程将它们拆分，允许应用显式选择：

- 低延迟 CPU TCM；
- 普通系统内存 SSRAM0；
- Extended SRAM；
- 第二组共享内存 SSRAM1。

物理布局依据 Ambiq HAL和 [Apollo4 Family Programmer's Guide](https://ambiq.com/wp-content/uploads/2022/10/Apollo4-Family-Programmers-Guide.pdf)：

| 用途 | 地址范围 | 容量 | C宏 |
|---|---:|---:|---|
| ITCM代码区 | `0x10000000–0x1000FFFF` | 64 KiB | Zephyr现有 ITCM机制 |
| 高速 CPU TCM数据区 | `0x10010000–0x1005FFFF` | 320 KiB | `JKCARE_FAST_RAM` |
| 普通 SSRAM0 | `0x10060000–0x1015FFFF` | 1 MiB | `JKCARE_NORMAL_RAM`、`JKCARE_SSRAM0` |
| Extended SRAM | `0x10160000–0x101BFFFF` | 384 KiB | `JKCARE_EXTENDED_RAM` |
| SSRAM1 | `0x101C0000–0x102BFFFF` | 1 MiB | `JKCARE_SSRAM1` |

Ambiq官方资料将物理存储体称为 TCM、SSRAM0、Extended SRAM和SSRAM1，并未把它们命名为“AHPB0/AHPB1”。工程按真实存储体命名，避免逻辑名称与硬件映射不一致。

### 21.2 三层映射关系

自定义 RAM不是只写一个 C属性，而是三层共同完成：

```text
C变量的input section
  → LD定义的output section
  → DeviceTree生成的MEMORY物理区域
```

以高速 RAM为例：

```c
JKCARE_FAST_RAM
static uint8_t ppg_buffer[4096];
```

宏把变量放入 input section：

```c
__attribute__((section(".jkcare_fast_ram"), aligned(32), used))
```

链接脚本收集它并指定目标 MEMORY区域：

```ld
SECTION_PROLOGUE(.jkcare_fast_ram, (NOLOAD), SUBALIGN(32))
{
    __jkcare_fast_ram_start = .;
    KEEP(*(.jkcare_fast_ram))
    KEEP(*(.jkcare_fast_ram.*))
    __jkcare_fast_ram_end = .;
} GROUP_NOLOAD_LINK_IN(FAST_RAM, FAST_RAM)
```

DeviceTree定义 `FAST_RAM` 的真实地址和容量。最终关系为：

```text
ppg_buffer
  → .jkcare_fast_ram
  → FAST_RAM
  → 0x10010000–0x1005FFFF
```

### 21.3 DeviceTree物理内存定义

板级内存定义位于：

```text
boards/ambiq/jkcare_apollo4p_blue/jkcare_apollo4p_blue.dts
```

普通 RAM节点：

```dts
normal_sram: memory@10060000 {
    compatible = "mmio-sram";
    reg = <0x10060000 0x00100000>;
};
```

并通过 `chosen` 成为 Zephyr默认 RAM：

```dts
chosen {
    zephyr,sram = &normal_sram;
};
```

Zephyr据此生成：

```ld
MEMORY
{
    RAM (wx) :
        ORIGIN = 0x10060000,
        LENGTH = 0x00100000
}
```

高速 RAM等额外区域使用：

```dts
fast_ram: fast-memory@10010000 {
    compatible = "zephyr,memory-region", "mmio-sram";
    reg = <0x10010000 0x00050000>;
    zephyr,memory-region = "FAST_RAM";
};
```

从而生成：

```ld
FAST_RAM (rw) :
    ORIGIN = 0x10010000,
    LENGTH = 0x00050000
```

地址只在 DTS中维护，LD通过区域名称引用。这样可以避免 DTS和 LD各写一份地址后发生不一致，也便于更换开发板时调整物理映射。

### 21.4 `mmio-sram` 与 `zephyr,memory-region`

两者作用不同：

```dts
compatible = "mmio-sram";
```

表示该节点是一块 CPU可直接寻址的 SRAM。若节点被 `chosen/zephyr,sram`选为默认内存，Zephyr会自动生成 `RAM`，链接脚本通过 `RAMABLE_REGION`使用它。

```dts
compatible = "zephyr,memory-region", "mmio-sram";
```

除了说明它是 SRAM，还要求 Zephyr为它生成一个独立、可命名的链接器 MEMORY区域。此时还必须提供：

```dts
zephyr,memory-region = "FAST_RAM";
```

完整含义为：

```text
compatible = "zephyr,memory-region"
  → 生成独立链接器区域

compatible中的"mmio-sram"
  → 物理类型为直接寻址SRAM

zephyr,memory-region = "FAST_RAM"
  → 链接器区域名称

reg = <起始地址 长度>
  → 链接器区域地址和容量
```

默认 `normal_sram` 没有增加 `zephyr,memory-region`，因为它已经生成默认 `RAM`。如果再为同一地址生成 `SSRAM0`，可能形成两个相互重叠的 MEMORY区域，分别向其中放置对象时存在覆盖风险。

### 21.5 MEMORY区域名称能否自定义

下面的 compatible 是 Zephyr固定 binding名称，不能随意修改：

```dts
compatible = "zephyr,memory-region";
```

下面的属性值是项目自定义的链接区域名：

```dts
zephyr,memory-region = "FAST_RAM";
```

它可以改为：

```dts
zephyr,memory-region = "JKCARE_LOW_LATENCY_RAM";
```

但必须同时把 LD中的名称改为：

```ld
GROUP_NOLOAD_LINK_IN(
    JKCARE_LOW_LATENCY_RAM,
    JKCARE_LOW_LATENCY_RAM
)
```

命名要求：

- 每个 `zephyr,memory-region`值在整个 DTS中必须唯一；
- 推荐只使用大写字母、数字和下划线；
- 推荐格式为 `^[A-Z_][A-Z0-9_]*$`；
- 不建议使用空格、中文、连字符、`@`或以数字开头；
- 修改 DTS名称后必须同步修改 LD引用。

下面三个名称彼此独立：

```dts
fast_ram: fast-memory@10010000 {
    zephyr,memory-region = "FAST_RAM";
};
```

| 内容 | 示例 | 用途 |
|---|---|---|
| 节点标签 | `fast_ram` | DTS中通过 `&fast_ram`引用 |
| 节点名 | `fast-memory@10010000` | 描述设备节点和单元地址 |
| MEMORY区域名 | `FAST_RAM` | LD中使用 |

### 21.6 为什么使用两个 LD文件

当前文件：

```text
linker/jkcare-normal-ram.ld
linker/jkcare-memory-regions.ld
```

它们不是因为一个文件写不下，而是需要插入 Zephyr主链接脚本的不同位置。

普通 SRAM0属于默认 `RAMABLE_REGION`，通过：

```cmake
zephyr_linker_sources(
    RAM_SECTIONS
    linker/jkcare-normal-ram.ld
)
```

放在 Zephyr默认 RAM布局内部，使其参与 `_image_ram_start`、`_image_ram_end`、MPU覆盖范围和 RAM统计。

其他三个区域是独立的 MEMORY区域，通过：

```cmake
zephyr_linker_sources(
    SECTIONS
    linker/jkcare-memory-regions.ld
)
```

分别放入：

```text
FAST_RAM
EXTENDED_RAM
SSRAM1
```

同一个 linker snippet文件不能同时注册到 `RAM_SECTIONS`和 `SECTIONS`；相同文件多次注册时只保留最后一个插入位置。把默认 RAM section强行放在 `SECTIONS`虽然可能链接成功，但会脱离 Zephyr推荐的默认 RAM布局，可能影响内存边界、MPU和统计，因此保持两个入口文件更安全。

### 21.7 C宏使用方法

头文件：

```text
include/jkcare_memory_sections.h
```

使用示例：

```c
#include <string.h>

#include "jkcare_memory_sections.h"

JKCARE_FAST_RAM
static uint8_t ppg_filter_buffer[16 * 1024];

JKCARE_NORMAL_RAM
static uint8_t application_buffer[32 * 1024];

JKCARE_EXTENDED_RAM
static uint8_t display_work_buffer[64 * 1024];

JKCARE_SSRAM1
static uint8_t frame_buffer[256 * 1024];
```

`JKCARE_SSRAM0` 是 `JKCARE_NORMAL_RAM` 的别名：

```c
#define JKCARE_SSRAM0 JKCARE_NORMAL_RAM
```

所有 section使用 32字节对齐，以便后续处理 Cache line、DMA或硬件对齐要求。具体外设仍可能要求更大的地址或长度对齐，需要以对应驱动和硬件手册为准。

### 21.8 `NOLOAD` 与初始化

当前自定义 section全部定义为：

```ld
(NOLOAD)
```

含义是：

- 固件镜像不保存这部分初始内容；
- 启动时不从 MRAM复制初始值；
- Zephyr不会自动清零；
- 可以避免启动早期访问尚未完成电源配置的 RAM bank。

因此使用前必须显式初始化：

```c
memset(ppg_filter_buffer, 0, sizeof(ppg_filter_buffer));
memset(frame_buffer, 0, sizeof(frame_buffer));
```

`NOLOAD` 不等于掉电保持。软件复位、睡眠和深睡期间是否保留内容，取决于 Apollo4的 RAM供电与 retention配置；掉电后这些 RAM内容都会消失。

在访问 Extended SRAM或 SSRAM1前还应确认：

- 对应 RAM bank已经上电；
- 深睡保持策略符合业务要求；
- MPU允许访问；
- Cache属性和 DMA一致性配置正确；
- 区域没有同时分配给 DSP、显示、GPU或其他子系统。

### 21.9 section边界符号与 `extern`

LD为每个 section定义边界：

```ld
__jkcare_fast_ram_start = .;
__jkcare_fast_ram_end = .;
```

头文件通过数组形式把链接器符号暴露给 C：

```c
extern uint8_t __jkcare_fast_ram_start[];
extern uint8_t __jkcare_fast_ram_end[];
```

这些不是普通数组，也不会分配额外 RAM；真正定义来自 LD。数组形式用于表达“这是一个地址”，避免把链接器符号误当成存储在该地址中的整数变量。

计算 section实际使用量：

```c
size_t used = JKCARE_SECTION_SIZE(
    __jkcare_fast_ram_start,
    __jkcare_fast_ram_end
);
```

整体清零：

```c
memset(
    __jkcare_fast_ram_start,
    0,
    JKCARE_SECTION_SIZE(
        __jkcare_fast_ram_start,
        __jkcare_fast_ram_end
    )
);
```

也可以用于：

- 输出内存统计；
- 检查 DMA buffer是否位于指定 RAM；
- 配置 Cache或 MPU；
- 将预留区域交给自定义 heap；
- 将区域交给 `llext_heap_init()`。

如果应用只使用 `JKCARE_FAST_RAM`放置变量，从不查询边界，`extern`声明并不是 section正确放置的必要条件；保留它们没有运行成本，并方便后续统一初始化和内存管理。

### 21.10 给 LLEXT预留 RAM

例如在 SSRAM1预留 512 KiB：

```c
JKCARE_SSRAM1
static uint8_t llext_memory[512 * 1024];
```

配置动态 LLEXT heap：

```conf
CONFIG_LLEXT=y
CONFIG_LLEXT_HEAP_DYNAMIC=y
CONFIG_LLEXT_HEAP_K_HEAP=y
```

对应 RAM bank上电并初始化后调用：

```c
int ret = llext_heap_init(
    llext_memory,
    sizeof(llext_memory)
);
```

不要同时把整个 `__jkcare_ssram1_start/end`和其中某个数组分别交给两个内存分配器，否则会形成重叠管理。一个物理范围只能由一个明确的 owner负责分配。

### 21.11 编译和验证

重新编译：

```powershell
west build -p always `
    -b jkcare_apollo4p_blue `
    -d build `
    .
```

查看生成的 MEMORY区域：

```powershell
Select-String `
    .\build\zephyr\linker.cmd `
    -Pattern "FAST_RAM|EXTENDED_RAM|SSRAM1|RAM \("
```

查看 section：

```powershell
arm-zephyr-eabi-objdump -h `
    .\build\zephyr\zephyr.elf |
    Select-String "jkcare_"
```

查看变量地址：

```powershell
arm-zephyr-eabi-nm -n `
    .\build\zephyr\zephyr.elf |
    Select-String "ppg_filter_buffer|frame_buffer"
```

判断标准：

```text
JKCARE_FAST_RAM     → 0x10010000–0x1005FFFF
JKCARE_NORMAL_RAM   → 0x10060000–0x1015FFFF
JKCARE_EXTENDED_RAM → 0x10160000–0x101BFFFF
JKCARE_SSRAM1       → 0x101C0000–0x102BFFFF
```

链接器会检查每个独立 MEMORY区域是否溢出，但不会替应用确认 RAM bank电源状态、Cache属性和跨处理器所有权，这些仍属于板级初始化和运行时管理职责。
