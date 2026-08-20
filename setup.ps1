$ErrorActionPreference = "Stop"

$ProjectDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ToolsDir = Join-Path $ProjectDir ".tools"
$SdkDir = Join-Path $ToolsDir "pico-sdk"

function Refresh-Path {
    $machine = [Environment]::GetEnvironmentVariable("Path", "Machine")
    $user = [Environment]::GetEnvironmentVariable("Path", "User")
    $env:Path = "$machine;$user"
}

function Has-Tools {
    return (Get-Command git -ErrorAction SilentlyContinue) -and
        (Get-Command cmake -ErrorAction SilentlyContinue) -and
        (Get-Command ninja -ErrorAction SilentlyContinue) -and
        (Get-Command arm-none-eabi-gcc -ErrorAction SilentlyContinue)
}

if (-not (Has-Tools)) {
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        throw "Windows Package Manager is required. Install App Installer from Microsoft Store and run setup.bat again."
    }
    Write-Host "Required build tools are missing. The installer will add them now."
    $packages = @(
        "Git.Git",
        "Kitware.CMake",
        "Ninja-build.Ninja",
        "Arm.GnuArmEmbeddedToolchain"
    )
    foreach ($package in $packages) {
        winget install --id $package --exact --accept-package-agreements --accept-source-agreements --silent
    }
    Refresh-Path
}

if (-not (Get-Command arm-none-eabi-gcc -ErrorAction SilentlyContinue)) {
    $compiler = Get-ChildItem "C:\Program Files", "C:\Program Files (x86)" -Filter arm-none-eabi-gcc.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($compiler) {
        $env:Path = "$($compiler.Directory.FullName);$env:Path"
        $env:PICO_TOOLCHAIN_PATH = Split-Path -Parent $compiler.Directory.FullName
    }
}

if (-not (Has-Tools)) {
    throw "The required build tools could not be installed. Restart Windows and run setup.bat again."
}

New-Item -ItemType Directory -Force -Path $ToolsDir | Out-Null
if (-not (Test-Path (Join-Path $SdkDir "external\pico_sdk_import.cmake"))) {
    git clone --branch 2.2.0 --depth 1 --recurse-submodules --shallow-submodules https://github.com/raspberrypi/pico-sdk.git $SdkDir
}

Write-Host "Select your Raspberry Pi Pico:"
Write-Host "1) Pico W"
Write-Host "2) Pico 2 W"
$choice = Read-Host "Choice [1]"
if ([string]::IsNullOrWhiteSpace($choice) -or $choice -eq "1") {
    $board = "pico_w"
} elseif ($choice -eq "2") {
    $board = "pico2_w"
} else {
    throw "Invalid board selection."
}

$ssid = Read-Host "Wi-Fi name (SSID)"
if ([string]::IsNullOrWhiteSpace($ssid) -or $ssid.Length -gt 32) {
    throw "The Wi-Fi name must contain between 1 and 32 characters."
}

$securePassword = Read-Host "Wi-Fi password" -AsSecureString
$pointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($securePassword)
try {
    $password = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($pointer)
} finally {
    [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($pointer)
}
if ($password.Length -lt 8 -or $password.Length -gt 63) {
    throw "The Wi-Fi password must contain between 8 and 63 characters."
}

$ssidC = $ssid.Replace('\', '\\').Replace('"', '\"')
$passwordC = $password.Replace('\', '\\').Replace('"', '\"')
$random = [Security.Cryptography.RandomNumberGenerator]::Create()
$bytes = New-Object byte[] 4
try {
    $random.GetBytes($bytes)
} finally {
    $random.Dispose()
}
$provisioningId = [BitConverter]::ToUInt32($bytes, 0)
if ($provisioningId -eq 0) {
    $provisioningId = 1
}
$config = @(
    "#ifndef CONFIG_LOCAL_H",
    "#define CONFIG_LOCAL_H",
    "",
    "#define APP_WIFI_SSID `"$ssidC`"",
    "#define APP_WIFI_PASSWORD `"$passwordC`"",
    "#define APP_PROVISIONING_ID $($provisioningId)U",
    "",
    "#endif"
)
$encoding = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllLines(
    (Join-Path $ProjectDir "src\config_local.h"),
    $config,
    $encoding
)
$password = $null

$env:PICO_SDK_PATH = $SdkDir
$buildDir = Join-Path $ProjectDir "build-$board"
cmake -E remove_directory $buildDir
cmake -S $ProjectDir -B $buildDir -G Ninja -DPICO_BOARD=$board -DCMAKE_BUILD_TYPE=Release -DEPAPER_USB_LOGS=ON
cmake --build $buildDir

$firmwareDir = Join-Path $ProjectDir "firmware"
New-Item -ItemType Directory -Force -Path $firmwareDir | Out-Null
$output = Join-Path $firmwareDir "ha_ink_display-$board.uf2"
Copy-Item (Join-Path $buildDir "ha_ink_display.uf2") $output -Force
Write-Host ""
Write-Host "Firmware ready:"
Write-Host $output
Write-Host "Hold BOOTSEL while connecting the Pico, then copy this UF2 to the RPI-RP2 drive."
