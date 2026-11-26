# Dot-source this script: . .\env.ps1
$workspace = $PSScriptRoot.Replace('\', '/')
$sensorModule = (Join-Path $PSScriptRoot 'sensor-drivers').Replace('\', '/')
$env:ZEPHYR_BASE = "$workspace/zephyr-4.4.0"
$env:ZEPHYR_SDK_INSTALL_DIR = "$workspace/tools/zephyr-sdk-1.0.1"
$env:ZEPHYR_TOOLCHAIN_VARIANT = "zephyr"
$env:LLEXT_EDK_INSTALL_DIR = "$workspace/tools/jkcare-plugin-sdk/llext-edk"
$env:VIRTUAL_ENV = Join-Path $workspace ".venv"
$extraModules = @($env:ZEPHYR_EXTRA_MODULES -split ';' | Where-Object { $_ })
if ($sensorModule -notin $extraModules) {
    $env:ZEPHYR_EXTRA_MODULES = (@($extraModules) + $sensorModule) -join ';'
}
$paths = @(
    (Join-Path $workspace ".venv/Scripts"),
    (Join-Path $env:ZEPHYR_SDK_INSTALL_DIR "gnu/arm-zephyr-eabi/bin"),
    "C:/Program Files/7-Zip",
    "C:/Program Files/SEGGER/JLink_V976"
)
$existingPaths = $env:Path -split ";" | Where-Object { $_ -and $_ -notin $paths }
$env:Path = (($paths + $existingPaths) -join ";")
Write-Host "JKCare environment ready: $workspace"
