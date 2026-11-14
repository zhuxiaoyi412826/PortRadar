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
Check "--help 说明双模式参数" ("$out" -match "--gui" -and "$out" -match "--cli")

# 双模式：--cli 强制命令行，参数需正确透传
$out = & $exe --cli --version
Check "--cli 强制命令行模式" ("$out" -match "PortLens v\d+\.\d+\.\d+")
Check "--cli 退出码为 0" ($LASTEXITCODE -eq 0)

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

# port: URI 形式（无效端口 → 落入未知选项分支）
& $exe port:abc 2>&1 | Out-Null
Check "port:abc 非法端口退出码为 2" ($LASTEXITCODE -eq 2)

& $exe port:99999 2>&1 | Out-Null
Check "port:99999 超出范围退出码为 2" ($LASTEXITCODE -eq 2)

# 裸端口号直接检测形式（无效值，有效值为交互模式不在此测）
& $exe 99999 2>&1 | Out-Null
Check "port 99999 超出范围退出码为 2" ($LASTEXITCODE -eq 2)

& $exe 0 2>&1 | Out-Null
Check "port 0 退出码为 2" ($LASTEXITCODE -eq 2)

# 进程名直达检测（无匹配 → 退出码 1，与 -f 一致；有效匹配为交互模式不在此测）
& $exe zqxnosuchproc 2>&1 | Out-Null
Check "port zqxnosuchproc 无匹配退出码为 1" ($LASTEXITCODE -eq 1)

& $exe abc 2>&1 | Out-Null
Check "port abc 无匹配退出码为 1" ($LASTEXITCODE -eq 1)

# ---------- v0.7 新增功能 ----------
$dataDir = Join-Path $env:LOCALAPPDATA 'PortLens'
$favPath = Join-Path $dataDir 'favorites.txt'
$histPath = Join-Path $dataDir 'history.tsv'
$favBackup = $null
$histBackup = $null
if (Test-Path $favPath) { $favBackup = [System.IO.File]::ReadAllText($favPath) }
if (Test-Path $histPath) { $histBackup = [System.IO.File]::ReadAllText($histPath) }

# --config 查看/生成配置
$out = & $exe --config
Check "--config 退出码为 0" ($LASTEXITCODE -eq 0)
Check "--config 显示配置路径与别名" ("$out" -match "port\.conf" -and "$out" -match "mysql")

# 收藏夹
& $exe fav remove 61201 2>&1 | Out-Null
$out = & $exe fav add 61201
Check "fav add 退出码为 0" ($LASTEXITCODE -eq 0)
$out = & $exe fav list
Check "fav list 列出收藏端口" ("$out" -match "61201" -and $LASTEXITCODE -eq 0)
& $exe fav remove 61201 | Out-Null
Check "fav remove 退出码为 0" ($LASTEXITCODE -eq 0)
& $exe fav add abc 2>&1 | Out-Null
Check "fav add 非法端口退出码为 2" ($LASTEXITCODE -eq 2)

# --tray 无收藏端口时提示并退出
if ($favBackup) { Remove-Item $favPath -Force -ErrorAction SilentlyContinue }
& $exe --tray 2>&1 | Out-Null
Check "--tray 无端口时退出码为 2" ($LASTEXITCODE -eq 2)

# 导出 CSV / JSON
$csvFile = Join-Path $env:TEMP 'portlens_smoke_export.csv'
$jsonFile = Join-Path $env:TEMP 'portlens_smoke_export.json'
Remove-Item $csvFile, $jsonFile -Force -ErrorAction SilentlyContinue
& $exe -l tcp --export "csv:$csvFile" | Out-Null
Check "CSV 导出退出码为 0" ($LASTEXITCODE -eq 0)
Check "CSV 文件含 UTF-8 表头" ((Test-Path $csvFile) -and ((Get-Content $csvFile -TotalCount 1) -match "端口,协议"))
& $exe -l --export "json:$jsonFile" | Out-Null
Check "JSON 导出退出码为 0" ($LASTEXITCODE -eq 0)
$jsonOk = $false
try { $parsed = Get-Content $jsonFile -Raw | ConvertFrom-Json; $jsonOk = @($parsed).Count -gt 0 } catch { $jsonOk = $false }
Check "JSON 导出可被解析" $jsonOk
Remove-Item $csvFile, $jsonFile -Force -ErrorAction SilentlyContinue
& $exe -l --export xml 2>&1 | Out-Null
Check "未知导出格式退出码为 2" ($LASTEXITCODE -eq 2)

# 批量释放参数校验
& $exe -k abc 2>&1 | Out-Null
Check "-k 非法参数退出码为 2" ($LASTEXITCODE -eq 2)
& $exe -k 2>&1 | Out-Null
Check "-k 缺参数退出码为 2" ($LASTEXITCODE -eq 2)

# 历史记录
$out = & $exe --history
Check "--history 退出码为 0 或 1" ($LASTEXITCODE -eq 0 -or $LASTEXITCODE -eq 1)

# 定向监控缺参数
& $exe -w 2>&1 | Out-Null
Check "-w 缺端口退出码为 2" ($LASTEXITCODE -eq 2)

# 还原现场
if ($null -ne $favBackup) { [System.IO.File]::WriteAllText($favPath, $favBackup) }
else { Remove-Item $favPath -Force -ErrorAction SilentlyContinue }
if ($null -ne $histBackup) { [System.IO.File]::WriteAllText($histPath, $histBackup) }

# PATH 安装 / 卸载
$exeDir = (Get-Item $exe).DirectoryName
$before = GetUserPath
$hadDir = $before -like "*$exeDir*"

& $exe --install-path | Out-Null
Check "--install-path 退出码为 0" ($LASTEXITCODE -eq 0)

$after = GetUserPath
Check "--install-path 后 PATH 包含程序目录" ("$after" -like "*$exeDir*")

# port: 协议随 --install-path 一并注册
$protoCmd = (Get-ItemProperty -Path "HKCU:\Software\Classes\port\shell\open\command" -ErrorAction SilentlyContinue).'(default)'
Check "--install-path 注册 port: 协议指向 port.exe" ($protoCmd -like "*port.exe*")

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
    $protoGone = ($null -eq (Get-Item "HKCU:\Software\Classes\port" -ErrorAction SilentlyContinue))
    Check "--uninstall-path 注销 port: 协议" $protoGone
} else {
    Check "--uninstall-path 跳过（目录原本已在 PATH，避免破坏用户环境）" $true
}

Write-Host ""
Write-Host "通过: $Pass  失败: $Fail"
if ($Fail -gt 0) { exit 1 } else { exit 0 }
