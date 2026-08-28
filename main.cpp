#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <windows.h>
#include <shellapi.h>
#include <conio.h>
#include <stdlib.h>
#include "console_util.h"
#include "port_checker.h"
#include "process_manager.h"

// ========== 输入工具函数（全部用 _getch，避免 cin 缓冲问题） ==========

static bool readInt(const char* prompt, int defaultValue, int minVal, int maxVal, int& result) {
    std::cout << "  " << prompt << " [默认 " << defaultValue << "]: ";
    std::cout.flush();
    
    char buf[32] = {0};
    int i = 0;
    
    while (i < 30) {
        int ch = _getch();
        if (ch == '\r') {
            std::cout << std::endl;
            break;
        }
        if (ch == '\b' && i > 0) {
            i--;
            buf[i] = 0;
            std::cout << "\b \b";
            continue;
        }
        if (ch >= '0' && ch <= '9') {
            buf[i++] = (char)ch;
            std::cout << (char)ch;
        }
    }
    
    if (i == 0) {
        result = defaultValue;
        return false;
    }
    
    int val = atoi(buf);
    if (val < minVal || val > maxVal) {
        printError("输入无效 (范围: " + std::to_string(minVal) + "-" + std::to_string(maxVal) + ")");
        return false;
    }
    
    result = val;
    return true;
}

static bool readYesNo(const char* prompt) {
    std::cout << "  " << prompt << " (y/N): ";
    std::cout.flush();
    
    while (true) {
        int ch = _getch();
        if (ch == 'y' || ch == 'Y') {
            std::cout << (char)ch << std::endl;
            return true;
        }
        if (ch == 'n' || ch == 'N' || ch == '\r' || ch == '\n') {
            std::cout << "n" << std::endl;
            return false;
        }
    }
}

// 读取字符串输入（字母、数字、常见符号，用于进程名等）
static std::string readString(const char* prompt, int maxLen = 64) {
    std::cout << "  " << prompt << ": ";
    std::cout.flush();
    
    std::string result;
    while ((int)result.size() < maxLen) {
        int ch = _getch();
        if (ch == '\r') {
            std::cout << std::endl;
            break;
        }
        if (ch == '\b' && !result.empty()) {
            result.pop_back();
            std::cout << "\b \b";
            continue;
        }
        if (ch >= 32 && ch < 127) {
            result += (char)ch;
            std::cout << (char)ch;
        }
    }
    return result;
}

// 转小写（不区分大小写匹配用）
static std::string toLowerStr(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

// UTF-8 转 UTF-16
static std::wstring utf8ToWstring(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    std::wstring ws(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
    return ws;
}

// 在资源管理器中打开并选中文件
static void openFileLocation(const std::string& path) {
    std::wstring wpath = utf8ToWstring(path);
    std::wstring params = L"/select,\"" + wpath + L"\"";
    HINSTANCE h = ShellExecuteW(NULL, L"open", L"explorer.exe", params.c_str(), NULL, SW_SHOWNORMAL);
    if ((INT_PTR)h <= 32) {
        printError("打开文件位置失败");
    } else {
        printSuccess("已在资源管理器中打开");
    }
}

// ========== 端口说明数据 ==========

struct PortDesc {
    int port;
    const char* name;
    const char* desc;
};

PortDesc PORT_DESCS[] = {
    {21, "FTP", "文件传输协议"},
    {22, "SSH", "安全外壳协议"},
    {23, "Telnet", "远程登录协议"},
    {25, "SMTP", "简单邮件传输协议"},
    {53, "DNS", "域名系统"},
    {80, "HTTP", "超文本传输协议"},
    {110, "POP3", "邮局协议v3"},
    {135, "RPC", "远程过程调用"},
    {139, "NetBIOS", "Windows文件共享"},
    {143, "IMAP", "互联网消息访问协议"},
    {443, "HTTPS", "安全超文本传输协议"},
    {445, "SMB", "Windows服务器消息块"},
    {465, "SMTPS", "加密SMTP"},
    {587, "SMTP-submission", "邮件提交端口"},
    {993, "IMAPS", "加密IMAP"},
    {995, "POP3S", "加密POP3"},
    {1080, "SOCKS", "SOCKS代理"},
    {1433, "MSSQL", "Microsoft SQL Server"},
    {1521, "Oracle", "Oracle数据库"},
    {3306, "MySQL", "MySQL数据库"},
    {3389, "RDP", "远程桌面协议"},
    {5432, "PostgreSQL", "PostgreSQL数据库"},
    {5900, "VNC", "VNC远程桌面"},
    {6379, "Redis", "Redis缓存"},
    {7000, "Cassandra", "Cassandra数据库"},
    {8080, "HTTP-Proxy", "HTTP代理/备用端口"},
    {8443, "HTTPS-Alt", "HTTPS备用端口"},
    {27017, "MongoDB", "MongoDB数据库"},
    {0, nullptr, nullptr}
};

std::string getPortDesc(int port) {
    for (int i = 0; PORT_DESCS[i].name; i++) {
        if (PORT_DESCS[i].port == port) {
            return std::string(PORT_DESCS[i].name) + " - " + PORT_DESCS[i].desc;
        }
    }
    return "";
}

// ========== 界面函数 ==========

void printHeader() {
    clearScreen();
    setColor(CYAN);
    std::cout << "\n";
    std::cout << "    +-------------------------------------------------------+\n";
    std::cout << "    |                                                       |\n";
    std::cout << "    |    PORT CHECKER  -  端口占用检测工具 C++版 v1.0       |\n";
    std::cout << "    |                                                       |\n";
    std::cout << "    +-------------------------------------------------------+\n";
    std::cout << "\n";
    restoreColor();
}

void printMenu() {
    printSeparator();
    setColor(YELLOW);
    std::cout << "  主菜单\n";
    restoreColor();
    printSeparator();
    
    setColor(GREEN);
    std::cout << "  [1] ";
    restoreColor();
    std::cout << "单端口检测\n";
    
    setColor(GREEN);
    std::cout << "  [2] ";
    restoreColor();
    std::cout << "全部端口列表\n";
    
    setColor(GREEN);
    std::cout << "  [3] ";
    restoreColor();
    std::cout << "端口扫描（范围）\n";
    
    setColor(GREEN);
    std::cout << "  [4] ";
    restoreColor();
    std::cout << "杀死进程（释放端口）\n";
    
    setColor(GREEN);
    std::cout << "  [5] ";
    restoreColor();
    std::cout << "查看进程详情\n";
    
    setColor(GREEN);
    std::cout << "  [6] ";
    restoreColor();
    std::cout << "查看端口连接详情\n";
    
    setColor(GREEN);
    std::cout << "  [7] ";
    restoreColor();
    std::cout << "进程反查端口（输入进程名查占用）\n";
    
    setColor(GREEN);
    std::cout << "  [8] ";
    restoreColor();
    std::cout << "实时监控（端口变化高亮提醒）\n";
    
    setColor(GREEN);
    std::cout << "  [9] ";
    restoreColor();
    std::cout << "自动找可用端口（被占自动试下一个）\n";
    
    setColor(GREEN);
    std::cout << "  [0] ";
    restoreColor();
    std::cout << "退出程序\n";
    
    printSeparator();
}

void printPortTable(const std::vector<PortInfo>& ports, bool showProcess = true) {
    if (ports.empty()) {
        printWarning("没有找到匹配的端口");
        return;
    }

    setColor(YELLOW);
    printf("  %-7s %-8s %-15s %-7s %-10s %-20s\n",
        "端口", "协议", "本地地址", "PID", "状态", "进程名");
    restoreColor();
    setColor(DARK_GRAY);
    std::cout << "  " << std::string(70, '-') << "\n";
    restoreColor();

    for (const auto& p : ports) {
        std::string procName = "未知";
        if (showProcess && p.pid > 0) {
            procName = ProcessManager::getProcessName(p.pid);
        }

        setColor(CYAN);
        printf("  %-7d", p.port);
        restoreColor();

        if (p.protocol == "TCP") {
            setColor(GREEN);
        } else {
            setColor(MAGENTA);
        }
        printf(" %-8s", p.protocol.c_str());
        restoreColor();

        printf(" %-15s %-7d ", p.localAddr.c_str(), p.pid);
        
        if (p.state == "LISTENING") {
            setColor(GREEN);
        } else if (p.state == "ESTABLISHED") {
            setColor(CYAN);
        } else if (p.state == "TIME_WAIT") {
            setColor(DARK_GRAY);
        } else {
            setColor(YELLOW);
        }
        printf("%-10s", p.state.c_str());
        restoreColor();

        setColor(WHITE);
        printf(" %-20s", procName.c_str());
        restoreColor();
        
        std::cout << "\n";
    }

    setColor(DARK_GRAY);
    std::cout << "  " << std::string(70, '-') << "\n";
    restoreColor();
    
    setColor(CYAN);
    std::cout << "  共 " << ports.size() << " 条记录\n";
    restoreColor();
}

void pauseForKey() {
    setColor(DARK_GRAY);
    std::cout << "\n  按 回车键 或 空格键 返回菜单...";
    restoreColor();
    std::cout.flush();
    
    while (true) {
        int ch = _getch();
        if (ch == '\r' || ch == ' ' || ch == '\n') {
            break;
        }
    }
}

// ========== 功能函数 ==========

void checkSinglePort() {
    printTitle("单端口检测");
    
    int portVal;
    readInt("请输入端口号", 5400, 1, 65535, portVal);
    uint16_t port = static_cast<uint16_t>(portVal);

    std::string desc = getPortDesc(port);
    if (!desc.empty()) {
        printInfo("端口说明: " + desc);
    }

    std::cout << "\n  正在检测端口 " << port << " ...\n\n";

    PortInfo tcpInfo = PortChecker::checkPort(port, "TCP");
    PortInfo udpInfo = PortChecker::checkPort(port, "UDP");

    bool tcpOccupied = tcpInfo.pid > 0;
    bool udpOccupied = udpInfo.pid > 0;

    if (!tcpOccupied && !udpOccupied) {
        printSuccess("端口 " + std::to_string(port) + " 未被占用");
        return;
    }

    if (tcpOccupied) {
        setColor(RED);
        std::cout << "  [TCP] 端口 " << port << " 被占用\n";
        restoreColor();
        
        std::string procName = ProcessManager::getProcessName(tcpInfo.pid);
        std::string procPath = ProcessManager::getProcessPath(tcpInfo.pid);
        
        printf("    进程名: "); setColor(YELLOW); printf("%s\n", procName.c_str()); restoreColor();
        printf("    PID:    "); setColor(YELLOW); printf("%d\n", tcpInfo.pid); restoreColor();
        if (!procPath.empty()) {
            printf("    路径:   "); setColor(CYAN); printf("%s\n", procPath.c_str()); restoreColor();
        }
        std::cout << "\n";
    }

    if (udpOccupied) {
        setColor(RED);
        std::cout << "  [UDP] 端口 " << port << " 被占用\n";
        restoreColor();
        
        std::string procName = ProcessManager::getProcessName(udpInfo.pid);
        std::string procPath = ProcessManager::getProcessPath(udpInfo.pid);
        
        printf("    进程名: "); setColor(YELLOW); printf("%s\n", procName.c_str()); restoreColor();
        printf("    PID:    "); setColor(YELLOW); printf("%d\n", udpInfo.pid); restoreColor();
        if (!procPath.empty()) {
            printf("    路径:   "); setColor(CYAN); printf("%s\n", procPath.c_str()); restoreColor();
        }
        std::cout << "\n";
    }
}

void listAllPorts() {
    printTitle("全部监听端口列表");
    
    std::cout << "  请选择协议: \n";
    std::cout << "  [1] TCP + UDP (全部)\n";
    std::cout << "  [2] 仅 TCP\n";
    std::cout << "  [3] 仅 UDP\n";
    std::cout << "  请选择 [1]: ";
    std::cout.flush();
    
    int ch = _getch();
    int choice = (ch == '2') ? 2 : ((ch == '3') ? 3 : 1);
    std::cout << choice << "\n";

    std::string protocol = "ALL";
    if (choice == 2) protocol = "TCP";
    else if (choice == 3) protocol = "UDP";

    std::cout << "\n  正在获取端口列表...\n\n";

    auto ports = PortChecker::getAllListeningPorts(protocol);
    
    std::sort(ports.begin(), ports.end(), [](const PortInfo& a, const PortInfo& b) {
        if (a.port != b.port) return a.port < b.port;
        return a.protocol < b.protocol;
    });

    printPortTable(ports);
}

void scanPorts() {
    printTitle("端口范围扫描");
    
    int startPort, endPort;
    readInt("起始端口", 1, 1, 65535, startPort);
    readInt("结束端口", 1000, 1, 65535, endPort);

    if (startPort > endPort) {
        printError("起始端口不能大于结束端口");
        return;
    }

    if (endPort - startPort > 10000) {
        printWarning("扫描范围超过10000个端口，可能需要较长时间");
        if (!readYesNo("确定继续吗")) return;
    }

    std::cout << "\n  正在扫描 " << startPort << " - " << endPort << " ...\n\n";

    auto ports = PortChecker::scanPorts(static_cast<uint16_t>(startPort), static_cast<uint16_t>(endPort));
    
    std::sort(ports.begin(), ports.end(), [](const PortInfo& a, const PortInfo& b) {
        if (a.port != b.port) return a.port < b.port;
        return a.protocol < b.protocol;
    });

    printPortTable(ports);
}

void killProcessMenu() {
    printTitle("杀死进程（释放端口）");
    
    int pid;
    if (!readInt("请输入进程 PID", 0, 1, 999999, pid)) {
        printError("请输入有效的 PID");
        return;
    }

    std::string procName = ProcessManager::getProcessName(pid);
    if (procName == "未知进程") {
        printError("找不到 PID 为 " + std::to_string(pid) + " 的进程");
        return;
    }

    setColor(YELLOW);
    std::cout << "\n  ! 确定要杀死以下进程吗？\n\n";
    restoreColor();
    printf("    进程名: "); setColor(RED); printf("%s\n", procName.c_str()); restoreColor();
    printf("    PID:    "); setColor(RED); printf("%d\n", pid); restoreColor();
    
    std::string path = ProcessManager::getProcessPath(pid);
    if (!path.empty()) {
        printf("    路径:   %s\n", path.c_str());
    }
    
    std::cout << "\n";
    if (!readYesNo("此操作不可恢复！输入 y 确认")) {
        printInfo("已取消操作");
        return;
    }

    if (ProcessManager::killProcess(pid)) {
        printSuccess("进程已终止");
    } else {
        printError("杀死进程失败（可能权限不足）");
    }
}

void showProcessDetail() {
    printTitle("进程详情");
    
    int pid;
    if (!readInt("请输入进程 PID", 0, 1, 999999, pid)) {
        printError("请输入有效的 PID");
        return;
    }

    std::cout << "\n  正在获取进程信息...\n\n";

    ProcessDetail detail = ProcessManager::getProcessDetail(pid);
    if (detail.name == "未知进程" && detail.path.empty()) {
        printError("找不到该进程（可能已退出或权限不足）");
        return;
    }

    printSeparator();
    setColor(YELLOW);
    std::cout << "  进程基本信息\n";
    restoreColor();
    printSeparator();
    
    printf("  进程名:   "); setColor(CYAN); printf("%s\n", detail.name.c_str()); restoreColor();
    printf("  PID:      "); setColor(CYAN); printf("%d\n", detail.pid); restoreColor();
    printf("  父进程ID: "); setColor(CYAN); printf("%d\n", detail.parentPid); restoreColor();
    printf("  线程数:   "); setColor(CYAN); printf("%d\n", detail.threadCount); restoreColor();
    printf("  启动时间: "); setColor(CYAN); printf("%s\n", detail.startTime.c_str()); restoreColor();
    
    printSeparator();
    setColor(YELLOW);
    std::cout << "  内存使用\n";
    restoreColor();
    printSeparator();
    
    printf("  当前内存: "); setColor(GREEN); printf("%.2f MB\n", detail.memoryKb / 1024.0); restoreColor();
    printf("  峰值内存: "); setColor(YELLOW); printf("%.2f MB\n", detail.peakMemoryKb / 1024.0); restoreColor();
    
    if (!detail.path.empty()) {
        printSeparator();
        setColor(YELLOW);
        std::cout << "  文件信息\n";
        restoreColor();
        printSeparator();
        
        printf("  完整路径: "); setColor(CYAN); printf("%s\n", detail.path.c_str()); restoreColor();
        if (!detail.company.empty()) {
            printf("  公司:     "); setColor(WHITE); printf("%s\n", detail.company.c_str()); restoreColor();
        }
        if (!detail.description.empty()) {
            printf("  描述:     "); setColor(WHITE); printf("%s\n", detail.description.c_str()); restoreColor();
        }
    }
    
    printSeparator();
    
    // 打开文件位置选项
    if (!detail.path.empty()) {
        std::cout << "\n";
        if (readYesNo("是否在资源管理器中打开文件位置")) {
            openFileLocation(detail.path);
        }
    }
}

void showConnectionDetail() {
    printTitle("端口连接详情");
    
    int portVal;
    if (!readInt("请输入端口号", 80, 1, 65535, portVal)) {
        printError("请输入有效的端口号");
        return;
    }
    uint16_t port = static_cast<uint16_t>(portVal);

    std::cout << "\n  正在获取连接信息...\n\n";

    auto connections = PortChecker::getPortConnections(port);
    
    if (connections.empty()) {
        printInfo("端口 " + std::to_string(port) + " 没有活跃连接");
        return;
    }

    setColor(YELLOW);
    printf("  %-22s %-22s %-15s %-10s %-6s\n",
        "本地地址:端口", "远程地址:端口", "状态", "进程名", "PID");
    restoreColor();
    setColor(DARK_GRAY);
    std::cout << "  " << std::string(85, '-') << "\n";
    restoreColor();

    for (const auto& c : connections) {
        char local[32], remote[32];
        sprintf_s(local, "%s:%d", c.localAddr.c_str(), c.port);
        sprintf_s(remote, "%s:%d", c.remoteAddr.c_str(), c.remotePort);
        
        std::string procName = c.pid > 0 ? ProcessManager::getProcessName(c.pid) : "N/A";

        printf("  %-22s %-22s ", local, remote);
        
        if (c.state == "LISTENING") {
            setColor(GREEN);
        } else if (c.state == "ESTABLISHED") {
            setColor(CYAN);
        } else if (c.state == "TIME_WAIT") {
            setColor(DARK_GRAY);
        } else if (c.state == "CLOSE_WAIT") {
            setColor(RED);
        } else {
            setColor(YELLOW);
        }
        printf("%-15s", c.state.c_str());
        restoreColor();
        
        printf(" %-10s %-6d\n", procName.c_str(), c.pid);
    }

    setColor(DARK_GRAY);
    std::cout << "  " << std::string(85, '-') << "\n";
    restoreColor();
    
    int listenCount = 0, estabCount = 0, timeWaitCount = 0, otherCount = 0;
    for (const auto& c : connections) {
        if (c.state == "LISTENING") listenCount++;
        else if (c.state == "ESTABLISHED") estabCount++;
        else if (c.state == "TIME_WAIT") timeWaitCount++;
        else otherCount++;
    }
    
    setColor(CYAN);
    printf("  总计: %d 条连接  (LISTENING: %d, ESTABLISHED: %d, TIME_WAIT: %d, 其他: %d)\n",
        (int)connections.size(), listenCount, estabCount, timeWaitCount, otherCount);
    restoreColor();
}

// ========== 功能7: 进程反查端口 ==========

void findPortsByProcess() {
    printTitle("进程反查端口");
    
    std::string name = readString("请输入进程名（支持部分匹配，如 python）");
    if (name.empty()) {
        printError("进程名不能为空");
        return;
    }
    
    std::cout << "\n  正在查询进程包含 '" << name << "' 的端口占用...\n\n";
    
    std::string lowerQuery = toLowerStr(name);
    auto ports = PortChecker::getAllListeningPorts("ALL");
    
    std::vector<PortInfo> results;
    for (const auto& p : ports) {
        if (p.pid == 0) continue;
        std::string pname = ProcessManager::getProcessName(p.pid);
        if (toLowerStr(pname).find(lowerQuery) != std::string::npos) {
            results.push_back(p);
        }
    }
    
    if (results.empty()) {
        printInfo("没有找到进程名包含 '" + name + "' 的端口占用");
        return;
    }
    
    printPortTable(results);
}

// ========== 功能8: 实时监控 ==========

void monitorPort() {
    printTitle("实时端口监控");
    
    int portVal;
    readInt("请输入要监控的端口", 5400, 1, 65535, portVal);
    uint16_t port = static_cast<uint16_t>(portVal);
    
    int interval;
    readInt("刷新间隔(秒)", 2, 1, 60, interval);
    
    printInfo("监控中... 按 ESC 或 Q 键停止");
    Sleep(800);
    
    bool prevOccupied = false;
    uint32_t prevPid = 0;
    bool first = true;
    int refreshCount = 0;
    
    while (true) {
        PortInfo tcpInfo = PortChecker::checkPort(port, "TCP");
        PortInfo udpInfo = PortChecker::checkPort(port, "UDP");
        
        bool occupied = tcpInfo.pid > 0 || udpInfo.pid > 0;
        uint32_t curPid = tcpInfo.pid > 0 ? tcpInfo.pid : udpInfo.pid;
        
        // 状态变化检测
        std::string changeMsg;
        if (!first) {
            if (!prevOccupied && occupied) {
                changeMsg = "端口被占用!";
            } else if (prevOccupied && !occupied) {
                changeMsg = "端口已释放!";
            } else if (occupied && curPid != prevPid) {
                changeMsg = "占用进程已变化!";
            }
        }
        bool changed = !changeMsg.empty();
        
        refreshCount++;
        
        // 显示当前状态
        clearScreen();
        printTitle("实时端口监控 - 端口 " + std::to_string(port));
        
        SYSTEMTIME st;
        GetLocalTime(&st);
        setColor(DARK_GRAY);
        printf("  刷新: %d 次    时间: %02d:%02d:%02d    间隔: %d 秒    [ESC/Q 停止]\n\n",
            refreshCount, st.wHour, st.wMinute, st.wSecond, interval);
        restoreColor();
        
        if (changed) {
            setColor(YELLOW);
            std::cout << "  *** 状态变化: " << changeMsg << " ***\n\n";
            restoreColor();
            std::cout << "\a";  // 响铃提醒
        }
        
        if (tcpInfo.pid > 0) {
            setColor(RED);
            std::cout << "  [TCP] 被占用\n";
            restoreColor();
            printf("        进程: ");
            setColor(YELLOW);
            printf("%s (PID: %d)\n", ProcessManager::getProcessName(tcpInfo.pid).c_str(), tcpInfo.pid);
            restoreColor();
        } else {
            setColor(GREEN);
            std::cout << "  [TCP] 空闲\n";
            restoreColor();
        }
        
        if (udpInfo.pid > 0) {
            setColor(RED);
            std::cout << "  [UDP] 被占用\n";
            restoreColor();
            printf("        进程: ");
            setColor(YELLOW);
            printf("%s (PID: %d)\n", ProcessManager::getProcessName(udpInfo.pid).c_str(), udpInfo.pid);
            restoreColor();
        } else {
            setColor(GREEN);
            std::cout << "  [UDP] 空闲\n";
            restoreColor();
        }
        
        std::cout << "\n";
        
        prevOccupied = occupied;
        prevPid = curPid;
        first = false;
        
        // 等待间隔，期间检测退出键
        bool quit = false;
        for (int i = 0; i < interval * 10 && !quit; i++) {
            if (_kbhit()) {
                int ch = _getch();
                if (ch == 27 || ch == 'q' || ch == 'Q') quit = true;
            }
            if (!quit) Sleep(100);
        }
        if (quit) break;
    }
    
    printInfo("监控已停止");
}

// ========== 功能9: 自动找可用端口 ==========

void findAvailablePortMenu() {
    printTitle("自动查找可用端口");
    
    int preferred;
    readInt("首选端口", 5400, 1, 65535, preferred);
    
    int maxTries;
    readInt("最多尝试次数", 10, 1, 100, maxTries);
    
    std::cout << "\n";
    
    for (int i = 0; i < maxTries; i++) {
        int tryPort = preferred + i;
        if (tryPort > 65535) break;
        uint16_t port = static_cast<uint16_t>(tryPort);
        
        PortInfo tcpInfo = PortChecker::checkPort(port, "TCP");
        PortInfo udpInfo = PortChecker::checkPort(port, "UDP");
        
        if (tcpInfo.pid > 0 || udpInfo.pid > 0) {
            uint32_t pid = tcpInfo.pid > 0 ? tcpInfo.pid : udpInfo.pid;
            std::string who = ProcessManager::getProcessName(pid);
            setColor(YELLOW);
            printf("  端口 %d: 已被占用 (%s)\n", tryPort, who.c_str());
            restoreColor();
        } else {
            setColor(GREEN);
            printf("  端口 %d: 可用\n", tryPort);
            restoreColor();
            
            std::cout << "\n";
            printSuccess("推荐使用端口: " + std::to_string(tryPort));
            printInfo("共尝试 " + std::to_string(i + 1) + " 个端口");
            return;
        }
    }
    
    printError("从 " + std::to_string(preferred) + " 起连续 " + std::to_string(maxTries) + " 个端口都被占用");
}

// ========== 主函数 ==========

int main() {
    // 设置 UTF-8 代码页
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    
    // 初始化控制台颜色
    saveColor();
    
    if (!PortChecker::init()) {
        printError("Winsock 初始化失败");
        _getch();
        return 1;
    }

    while (true) {
        printHeader();
        printMenu();
        
        setColor(GREEN);
        std::cout << "  请选择功能 (输入数字): ";
        restoreColor();
        std::cout.flush();

        int ch = _getch();
        char choice = (char)ch;
        std::cout << choice << std::endl;

        switch (choice) {
            case '1':
                checkSinglePort();
                pauseForKey();
                break;
            case '2':
                listAllPorts();
                pauseForKey();
                break;
            case '3':
                scanPorts();
                pauseForKey();
                break;
            case '4':
                killProcessMenu();
                pauseForKey();
                break;
            case '5':
                showProcessDetail();
                pauseForKey();
                break;
            case '6':
                showConnectionDetail();
                pauseForKey();
                break;
            case '7':
                findPortsByProcess();
                pauseForKey();
                break;
            case '8':
                monitorPort();
                pauseForKey();
                break;
            case '9':
                findAvailablePortMenu();
                pauseForKey();
                break;
            case '0':
                printInfo("正在退出...");
                PortChecker::cleanup();
                Sleep(300);
                return 0;
            default:
                // 无效输入，不做任何事，重新显示菜单
                break;
        }
    }

    PortChecker::cleanup();
    return 0;
}
