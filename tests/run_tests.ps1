# PortLens smoke tests
# Run from repo root or tests\ directory: .\tests\run_tests.ps1

[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$exe = Join-Path $PSScriptRoot "..\port_checker.exe"
if (-not (Test-Path $exe)) {
    Write-Error "port_checker.exe not found. Run build.bat first."
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

# --version
$out = & $exe --version
Check "--version 输出版本号" ("$out" -match "PortLens v\d+\.\d+\.\d+")

# --help
$out = & $exe --help
Check "--help 输出用法" ("$out" -match "-c <端口>")

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

Write-Host ""
Write-Host "通过: $Pass  失败: $Fail"
if ($Fail -gt 0) { exit 1 } else { exit 0 }
