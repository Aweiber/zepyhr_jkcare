# JKCare Zephyr 工作区

本目录是主 Git 仓库，沿用原 `zepyhr_jkcare` 仓库的历史和 remote。

| 目录 | 管理方式 | 用途 |
| --- | --- | --- |
| `zepyhr_jkcare/` | 主仓库普通文件 | 产品板级、内存布局、配置和业务 |
| `sensor-drivers/` | 主仓库普通文件 | 跨产品复用的 Zephyr 驱动模块 |
| `env.ps1`、`scripts/`、`.vscode/` | 主仓库普通文件 | 环境、工作区初始化和编辑器配置 |
| `zephyr-4.4.0/` | Git submodule | Zephyr 源码 |
| `bootloader/mcuboot/` | Git submodule | MCUboot |
| `modules/*` 下的开源项目 | 各自为 Git submodule | HAL、文件系统和其他库 |
| `tools/edtt/`、`tools/net-tools/` | Git submodule | 上游测试工具源码 |

`.gitmodules` 保存每个依赖的上游地址，主仓库 gitlink 保存准确提交。
根目录 `west.yml` 继续供 Zephyr 发现依赖使用；更新依赖时必须同时核对 west manifest 和子模块指针。

## 克隆和恢复环境

```powershell
git clone --recurse-submodules https://github.com/Aweiber/zepyhr_jkcare.git
cd zepyhr_jkcare
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install west
git submodule update --init --recursive
```

在本机安装对应平台的 Zephyr SDK、CMake、Ninja 和 DTC。当前 Windows
`env.ps1` 预期 SDK 位于 `tools/zephyr-sdk-1.0.1`；SDK 安装包和安装目录不提交。

```powershell
. .\env.ps1
python -m pip install -r .\zephyr-4.4.0\scripts\requirements.txt
.\scripts\init-workspace.ps1
west build -b jkcare_apollo4p_blue -d .\zepyhr_jkcare\build .\zepyhr_jkcare -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

已有工作区切换主仓库提交后，使用 `git submodule update --init --recursive`
恢复该提交记录的依赖版本。不要用 `git submodule update --remote` 替代它。
`west update` 会按 manifest 调整依赖 HEAD；执行后需核对并提交子模块指针变化。

## 不提交的内容

`.gitignore` 排除 `.west`、Python 虚拟环境、下载缓存、SDK 安装目录、
固件和独立插件构建目录、生成的 LLEXT EDK 以及编辑器缓存。
EDK 应从对应固件构建重新生成，不能用旧 EDK 替代当前固件的导出接口。

产品及 PPG/ECG 插件编译细节见 [产品开发指南](zepyhr_jkcare/README.md)。
