# SPDX-License-Identifier: Apache-2.0
# Run after installing Python, west and the platform-specific Zephyr SDK.
$ErrorActionPreference = 'Stop'
$workspaceRoot = Split-Path -Parent $PSScriptRoot
Push-Location $workspaceRoot
try {
    . (Join-Path $workspaceRoot 'env.ps1')
    if (-not (Test-Path -LiteralPath (Join-Path $workspaceRoot '.west/config'))) {
        west init -l .
        if ($LASTEXITCODE -ne 0) { throw 'west init failed' }
    }
    west config manifest.path .
    if ($LASTEXITCODE -ne 0) { throw 'Setting manifest.path failed' }
    west config zephyr.base zephyr-4.4.0
    if ($LASTEXITCODE -ne 0) { throw 'Setting zephyr.base failed' }
    west zephyr-export
    if ($LASTEXITCODE -ne 0) { throw 'west zephyr-export failed' }
} finally {
    Pop-Location
}
