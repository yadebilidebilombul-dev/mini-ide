$ErrorActionPreference = "Stop"

$MiniRoot = $PSScriptRoot
$Root = Split-Path -Parent $MiniRoot
$BuildDir = Join-Path $MiniRoot "build"
$OutElf = Join-Path $MiniRoot "mini-ide.elf"
$OutBin = Join-Path $MiniRoot "mini-ide.bin"
$OutPacked = Join-Path $MiniRoot "mini-ide.packed.bin"
$GccBin = Join-Path $Root "GCC\xpack-arm-none-eabi-gcc-15.2.1-1.1\bin"
$Cc = Join-Path $GccBin "arm-none-eabi-gcc.exe"
$Objcopy = Join-Path $GccBin "arm-none-eabi-objcopy.exe"
$Size = Join-Path $GccBin "arm-none-eabi-size.exe"

$Sources = @(
    (Join-Path $Root "start.S"),
    (Join-Path $Root "init.c"),
    (Join-Path $Root "sram-overlay.c"),
    (Join-Path $Root "driver\bk4819.c"),
    (Join-Path $Root "driver\flash.c"),
    (Join-Path $Root "driver\gpio.c"),
    (Join-Path $Root "driver\spi.c"),
    (Join-Path $Root "driver\st7565.c"),
    (Join-Path $Root "driver\system.c"),
    (Join-Path $Root "driver\systick.c"),
    (Join-Path $Root "driver\keyboard.c"),
    (Join-Path $Root "font.c"),
    (Join-Path $MiniRoot "src\mini_script.c"),
    (Join-Path $MiniRoot "firmware\mini_board.c"),
    (Join-Path $MiniRoot "firmware\mini_display.c"),
    (Join-Path $MiniRoot "firmware\mini_input.c"),
    (Join-Path $MiniRoot "firmware\mini_app.c"),
    (Join-Path $MiniRoot "firmware\mini_storage.c"),
    (Join-Path $MiniRoot "firmware\main.c")
)

$Includes = @(
    "-I", $Root,
    "-I", $MiniRoot,
    "-I", (Join-Path $MiniRoot "firmware"),
    "-I", (Join-Path $Root "external\CMSIS_5\CMSIS\Core\Include"),
    "-I", (Join-Path $Root "external\CMSIS_5\Device\ARM\ARMCM0\Include")
)

$CommonFlags = @(
    "-Os",
    "-Wall",
    "-Werror",
    "-mcpu=cortex-m0",
    "-DENABLE_OVERLAY",
    "-fno-builtin",
    "-fshort-enums",
    "-fno-delete-null-pointer-checks",
    "-std=c11"
)

$AsmFlags = @("-c", "-mcpu=cortex-m0", "-DENABLE_OVERLAY")
$LdFlags = @("-mcpu=cortex-m0", "-nostartfiles", "-Wl,-T,$(Join-Path $MiniRoot 'firmware-mini.ld')")

if (Test-Path $BuildDir) {
    Remove-Item -LiteralPath $BuildDir -Recurse -Force
}
New-Item -ItemType Directory -Path $BuildDir | Out-Null

$Objects = @()
foreach ($Source in $Sources) {
    $Object = Join-Path $BuildDir (([System.IO.Path]::GetFileNameWithoutExtension($Source)) + ".o")
    $Objects += $Object
    if ([System.IO.Path]::GetExtension($Source) -ieq ".S") {
        & $Cc @AsmFlags @Includes $Source "-o" $Object
    } else {
        & $Cc @CommonFlags @Includes "-c" $Source "-o" $Object
    }
}

& $Cc @LdFlags @Objects "-o" $OutElf
& $Objcopy "-O" "binary" $OutElf $OutBin
& $Size $OutElf

@'
import binascii
import pathlib
import sys

source = pathlib.Path(sys.argv[1]).read_bytes()
obfuscation = bytes([
    0x47, 0x22, 0xC0, 0x52, 0x5D, 0x57, 0x48, 0x94, 0xB1, 0x60, 0x60, 0xDB, 0x6F, 0xE3, 0x4C, 0x7C,
    0xD8, 0x4A, 0xD6, 0x8B, 0x30, 0xEC, 0x25, 0xE0, 0x4C, 0xD9, 0x00, 0x7F, 0xBF, 0xE3, 0x54, 0x05,
    0xE9, 0x3A, 0x97, 0x6B, 0xB0, 0x6E, 0x0C, 0xFB, 0xB1, 0x1A, 0xE2, 0xC9, 0xC1, 0x56, 0x47, 0xE9,
    0xBA, 0xF1, 0x42, 0xB6, 0x67, 0x5F, 0x0F, 0x96, 0xF7, 0xC9, 0x3C, 0x84, 0x1B, 0x26, 0xE1, 0x4E,
    0x3B, 0x6F, 0x66, 0xE6, 0xA0, 0x6A, 0xB0, 0xBF, 0xC6, 0xA5, 0x70, 0x3A, 0xBA, 0x18, 0x9E, 0x27,
    0x1A, 0x53, 0x5B, 0x71, 0xB1, 0x94, 0x1E, 0x18, 0xF2, 0xD6, 0x81, 0x02, 0x22, 0xFD, 0x5A, 0x28,
    0x91, 0xDB, 0xBA, 0x5D, 0x64, 0xC6, 0xFE, 0x86, 0x83, 0x9C, 0x50, 0x1C, 0x73, 0x03, 0x11, 0xD6,
    0xAF, 0x30, 0xF4, 0x2C, 0x77, 0xB2, 0x7D, 0xBB, 0x3F, 0x29, 0x28, 0x57, 0x22, 0xD6, 0x92, 0x8B,
])
version = b"*OEFW-miniide"
version = version + (b"\x00" * (16 - len(version)))
inserted = source[:0x2000] + version + source[0x2000:]
packed = bytes(value ^ obfuscation[index % len(obfuscation)] for index, value in enumerate(inserted))
crc = binascii.crc_hqx(packed, 0x0000)
digest = bytes([(crc >> 0) & 0xFF, (crc >> 8) & 0xFF])
digest = bytes([digest[1], digest[0]])
pathlib.Path(sys.argv[2]).write_bytes(packed + digest)
'@ | python - $OutBin $OutPacked

Write-Host "Built:"
Write-Host "  $OutElf"
Write-Host "  $OutBin"
Write-Host "  $OutPacked"
