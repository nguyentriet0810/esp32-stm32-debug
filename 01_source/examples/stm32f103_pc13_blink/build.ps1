# SPDX-License-Identifier: GPL-3.0-or-later
<#
.SYNOPSIS
Build the standalone STM32F103C8 PC13 example; this script never flashes a device.
.PARAMETER ToolchainBin
Optional directory containing the arm-none-eabi tools. Uses PATH or the installed CubeCLT fallback.
#>
param([string]$ToolchainBin = '')
$ErrorActionPreference = 'Stop'
$probeExampleSource = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$probeExampleOutput = Join-Path $probeExampleSource '.cache\stm32_pc13_blink'
if (-not $ToolchainBin) {
    $probeArmCommand = Get-Command arm-none-eabi-gcc -ErrorAction SilentlyContinue
    $ToolchainBin = if ($probeArmCommand) {
        Split-Path -Parent $probeArmCommand.Source
    } else {
        'C:\ST\STM32CubeCLT_1.19.0\GNU-tools-for-STM32\bin'
    }
}
$probeArmCompiler = Join-Path $ToolchainBin 'arm-none-eabi-gcc.exe'
$probeArmObjcopy = Join-Path $ToolchainBin 'arm-none-eabi-objcopy.exe'
$probeArmSize = Join-Path $ToolchainBin 'arm-none-eabi-size.exe'
New-Item -ItemType Directory -Force -Path $probeExampleOutput > $null
$probeArmFlags = @('-mcpu=cortex-m3', '-mthumb', '-Og', '-g3', '-ffunction-sections', '-fdata-sections')
$probeArmCFlags = @('-std=c17', '-ffreestanding', '-fno-builtin', '-fno-common', '-Wall', '-Wextra',
    '-Werror', '-Wconversion', '-Wsign-conversion', '-Wshadow', '-Wstrict-prototypes',
    '-Wmissing-prototypes', '-fanalyzer')
& $probeArmCompiler @probeArmFlags @probeArmCFlags -c (Join-Path $PSScriptRoot 'main.c') -o (Join-Path $probeExampleOutput 'main.o')
if ($LASTEXITCODE -ne 0) { throw 'STM32 C compilation failed' }
& $probeArmCompiler @probeArmFlags -c (Join-Path $PSScriptRoot 'startup.S') -o (Join-Path $probeExampleOutput 'startup.o')
if ($LASTEXITCODE -ne 0) { throw 'STM32 startup compilation failed' }
$probeArmElf = Join-Path $probeExampleOutput 'pc13_blink.elf'
$probeArmMap = Join-Path $probeExampleOutput 'pc13_blink.map'
& $probeArmCompiler @probeArmFlags -nostdlib -nostartfiles -T (Join-Path $PSScriptRoot 'linker.ld') `
    '-Wl,--gc-sections' '-Wl,-z,noexecstack' "-Wl,-Map,$probeArmMap" (Join-Path $probeExampleOutput 'startup.o') `
    (Join-Path $probeExampleOutput 'main.o') -o $probeArmElf
if ($LASTEXITCODE -ne 0) { throw 'STM32 linking failed' }
& $probeArmObjcopy -O binary $probeArmElf (Join-Path $probeExampleOutput 'pc13_blink.bin')
if ($LASTEXITCODE -ne 0) { throw 'STM32 binary generation failed' }
& $probeArmSize $probeArmElf
if ($LASTEXITCODE -ne 0) { throw 'STM32 size inspection failed' }
Write-Output "Built STM32 target ELF: $probeArmElf"
