# PortLens smoke tests
# Run from repo root or tests\ directory: .\tests\run_tests.ps1

[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$exe = Join-Path $PSScriptRoot "..\port.exe"
if (-not (Test-Path $exe)) {
    Write-Error "port.exe not found. Run build.bat first."
    exit 1
}

$script:Pass = 0
$script:Fail = 0

function Check($name, $condition) {
    if ($condition) {
        $script:Pass++
        Write-Host "[PASS] $name" -ForegroundColor Green
    } else {
        $script:Fail++
        Write-Host "[FAIL] $name" -ForegroundColor Red
    }
}

function GetUserPath {
    $k = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey("Environment")
    if ($null -eq $k) { return $null }
    $v = $k.GetValue("Path", "", [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
    $k.Close()
    return $v
}

# --version
$out = & $exe --version
Check "--version 输出版本号" ("$out" -match "PortLens v\d+\.\d+\.\d+")

# --help
$out = & $exe --help
Check "--help 输出用法" ("$out" -match "-c <端口>" -and "$out" -match "--install-path")

# -c 空闲/被占用均返回有效退出码
& $exe -c 65500 | Out-Null
Check "-c 返回有效退出码 (0 或 1)" ($LASTEXITCODE -eq 0 -or $LASTEXITCODE -eq 1)

# -c 无效参数
& $exe -c abc 2>&1 | Out-Null
Check "-c 非法参数退出码为 2" ($LASTEXITCODE -eq 2)

# -c 缺少参数
& $exe -c 2>&1 | Out-Null
Check "-c 缺参数退出码为 2" ($LASTEXITCODE -eq 2)

# -l 列表
$out = & $exe -l tcp
Check "-l tcp 输出端口列表" ("$out" -match "LISTENING")

# -l 无效协议
& $exe -l xxx 2>&1 | Out-Null
Check "-l 非法协议退出码为 2" ($LASTEXITCODE -eq 2)

# -f 进程反查 (svchost 系统必有)
$out = & $exe -f svchost
Check "-f svchost 有结果" ("$out" -match "svchost")

# -s 小范围扫描
$out = & $exe -s 1 110
Check "-s 范围扫描输出统计" ("$out" -match "个端口被占用")

# -a 自动找可用端口
& $exe -a 60000 | Out-Null
Check "-a 找到可用端口退出码为 0" ($LASTEXITCODE -eq 0)

# 未知选项
& $exe --no-such-option 2>&1 | Out-Null
Check "未知选项退出码为 2" ($LASTEXITCODE -eq 2)

# PATH 安装 / 卸载
$exeDir = (Get-Item $exe).DirectoryName
$before = GetUserPath
$hadDir = $before -like "*$exeDir*"

& $exe --install-path | Out-Null
Check "--install-path 退出码为 0" ($LASTEXITCODE -eq 0)

$after = GetUserPath
Check "--install-path 后 PATH 包含程序目录" ("$after" -like "*$exeDir*")

# 重复安装不产生重复项
& $exe --install-path | Out-Null
$count = ([regex]::Matches("$after", [regex]::Escape($exeDir))).Count
$after2 = GetUserPath
$count2 = ([regex]::Matches("$after2", [regex]::Escape($exeDir))).Count
Check "--install-path 重复执行不重复添加" ($count2 -eq $count)

# 若测试前目录本就不在 PATH 中，则清理掉（还原现场）；否则保留
if (-not $hadDir) {
    & $exe --uninstall-path | Out-Null
    Check "--uninstall-path 退出码为 0" ($LASTEXITCODE -eq 0)
    $final = GetUserPath
    Check "--uninstall-path 后 PATH 已移除程序目录" ("$final" -notlike "*$exeDir*")
} else {
    Check "--uninstall-path 跳过（目录原本已在 PATH，避免破坏用户环境）" $true
}

Write-Host ""
Write-Host "通过: $Pass  失败: $Fail"
if ($Fail -gt 0) { exit 1 } else { exit 0 }
