# PortLens GUI 构建脚本
# 说明：项目路径含空格时 MinGW windres 会失败，这里自动映射一个无空格盘符再编译
param(
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$root = (Split-Path -Parent $MyInvocation.MyCommand.Path).TrimEnd('\')

Write-Host "== PortLens GUI Build ==" -ForegroundColor Cyan

# 1. 选择可用盘符做映射（规避路径空格问题）
$drive = $null
foreach ($d in 'P','Q','R','S','T','U','V','W') {
    if (-not (Test-Path "${d}:\")) { $drive = $d; break }
}
if (-not $drive) { throw "没有可用的盘符用于路径映射" }

& subst "${drive}:" $root
if ($LASTEXITCODE -ne 0) { throw "subst 失败" }
Write-Host "映射 ${drive}: -> $root" -ForegroundColor DarkGray

try {
    $gui = "${drive}:\gui"
    Push-Location $gui
    try {
        $env:CARGO_TERM_COLOR = 'never'
        if ($Clean) {
            Write-Host "[1/3] 清理..." -ForegroundColor Yellow
            cargo clean --manifest-path src-tauri\Cargo.toml
        }

        Write-Host "[1/3] 编译 (Rust + C++ 内核，release)..." -ForegroundColor Yellow
        cargo build --release --manifest-path src-tauri\Cargo.toml
        if ($LASTEXITCODE -ne 0) { throw "编译失败" }

        Write-Host "[2/3] 收集产物..." -ForegroundColor Yellow
        $out = Join-Path $root 'dist'
        New-Item -ItemType Directory -Force -Path $out | Out-Null
        $exe = Join-Path $gui 'src-tauri\target\release\port.exe'
        Copy-Item $exe (Join-Path $out 'port.exe') -Force

        # WebView2Loader.dll（Tauri 在 MinGW 下需要随程序分发）
        $loader = Get-ChildItem "$env:USERPROFILE\.cargo\registry\src\*\webview2-com-sys-*\x64\WebView2Loader.dll" -ErrorAction SilentlyContinue |
                  Select-Object -First 1
        if ($loader) {
            Copy-Item $loader.FullName (Join-Path $out 'WebView2Loader.dll') -Force
        } else {
            Write-Warning "未找到 WebView2Loader.dll，请从 webview2-com-sys 包中复制到 dist 目录"
        }

        Write-Host "[3/3] 完成" -ForegroundColor Green
        Get-ChildItem $out | Select-Object Name, @{N='大小';E={ '{0:N2} MB' -f ($_.Length/1MB) }} | Format-Table
        Write-Host "输出目录: $out" -ForegroundColor Cyan
    } finally {
        Pop-Location
    }
} finally {
    & subst "${drive}:" /D | Out-Null
}
