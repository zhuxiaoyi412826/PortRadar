#include "watch_tray.h"
#include "port_checker.h"
#include "process_manager.h"
#include "special_process.h"
#include "console_util.h"
#include <shellapi.h>
#include <string>
#include <cstdio>
#include <cstring>

// ========== 通用通知 ==========

void beepAlert(bool occupied) {
    if (occupied) {
        Beep(880, 180);
        Beep(660, 180);
    } else {
        Beep(660, 180);
        Beep(990, 180);
    }
}

namespace {
struct NotifyArgs {
    std::wstring title;
    std::wstring msg;
};

DWORD WINAPI notifyThreadProc(LPVOID param) {
    NotifyArgs* args = static_cast<NotifyArgs*>(param);
    MessageBoxW(NULL, args->msg.c_str(), args->title.c_str(),
                MB_OK | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND);
    delete args;
    return 0;
}
}  // namespace

void showDesktopNotification(const std::wstring& title, const std::wstring& msg) {
    NotifyArgs* args = new NotifyArgs();
    args->title = title;
    args->msg = msg;
    HANDLE h = CreateThread(NULL, 0, notifyThreadProc, args, 0, NULL);
    if (h) {
        CloseHandle(h);
    } else {
        delete args;
    }
}

// ========== 定向监控 ==========

static volatile LONG g_stopWatch = 0;

static BOOL WINAPI watchCtrlHandler(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
        InterlockedExchange(&g_stopWatch, 1);
        return TRUE;
    }
    return FALSE;
}

static bool isPortOccupied(uint16_t port) {
    PortInfo tcp = PortChecker::checkPort(port, "TCP");
    PortInfo udp = PortChecker::checkPort(port, "UDP");
    return tcp.pid > 0 || udp.pid > 0;
}

// 输出端口当前状态（占用时带进程信息）
static void printPortState(uint16_t port, bool occupied) {
    SYSTEMTIME st;
    GetLocalTime(&st);

    if (!occupied) {
        setColor(GREEN);
        printf("  [%02d:%02d:%02d] 端口 %d 空闲\n",
               st.wHour, st.wMinute, st.wSecond, port);
        restoreColor();
        return;
    }

    PortInfo tcp = PortChecker::checkPort(port, "TCP");
    PortInfo udp = PortChecker::checkPort(port, "UDP");
    uint32_t pid = tcp.pid > 0 ? tcp.pid : udp.pid;
    std::string name = ProcessManager::getProcessName(pid);
    std::string path = ProcessManager::getProcessPath(pid);
    std::string tag = SpecialProcessDetector::detect(name, port);

    setColor(RED);
    printf("  [%02d:%02d:%02d] 端口 %d 被占用", st.wHour, st.wMinute, st.wSecond, port);
    restoreColor();
    printf("  %s%s (PID: %u)\n", name.c_str(), tag.c_str(), pid);
    if (!path.empty()) {
        printf("               路径: %s\n", path.c_str());
    }
}

int runPortWatch(uint16_t port, int intervalMs, bool popup) {
    if (intervalMs < 200) intervalMs = 200;

    SetConsoleCtrlHandler(watchCtrlHandler, TRUE);
    InterlockedExchange(&g_stopWatch, 0);

    printTitle("定向端口监控");
    printf("  监控端口: %d    间隔: %d ms    退出: Ctrl+C\n\n", port, intervalMs);

    bool last = isPortOccupied(port);
    printPortState(port, last);
    if (last) {
        printInfo("该端口当前被占用，等待其释放...");
    } else {
        printInfo("该端口当前空闲，等待被占用...");
    }
    std::cout.flush();

    while (InterlockedCompareExchange(&g_stopWatch, 0, 0) == 0) {
        for (int slept = 0; slept < intervalMs; slept += 100) {
            if (InterlockedCompareExchange(&g_stopWatch, 0, 0) != 0) break;
            Sleep(100);
        }
        if (InterlockedCompareExchange(&g_stopWatch, 0, 0) != 0) break;

        bool cur = isPortOccupied(port);
        if (cur == last) continue;

        std::cout << "\n";
        printPortState(port, cur);
        beepAlert(cur);

        if (popup) {
            std::wstring title = L"PortLens 端口监控 - " + std::to_wstring(port);
            std::wstring msg = cur
                ? L"端口 " + std::to_wstring(port) + L" 已被占用"
                : L"端口 " + std::to_wstring(port) + L" 已释放（空闲）";
            showDesktopNotification(title, msg);
        }

        last = cur;
        std::cout.flush();
    }

    SetConsoleCtrlHandler(watchCtrlHandler, FALSE);
    std::cout << "\n";
    printInfo("监控已结束");
    return 0;
}

// ========== 系统托盘 ==========

namespace {
const UINT TRAY_ID = 1;
const UINT TIMER_ID = 1;
const UINT WM_TRAYICON = WM_APP + 1;
const UINT CMD_SHOW_CONSOLE = 1001;
const UINT CMD_REFRESH = 1002;
const UINT CMD_EXIT = 1003;

std::vector<uint16_t> g_trayPorts;
std::vector<bool> g_trayStates;
HWND g_trayWnd = NULL;
UINT g_taskbarCreatedMsg = 0;
HICON g_trayIcon = NULL;

bool isPortOccupiedForTray(uint16_t port) {
    PortInfo tcp = PortChecker::checkPort(port, "TCP");
    return tcp.pid > 0 || PortChecker::checkPort(port, "UDP").pid > 0;
}

std::wstring buildTooltip() {
    std::wstring tip = L"PortLens 端口监控\n";
    for (size_t i = 0; i < g_trayPorts.size() && i < 8; i++) {
        tip += std::to_wstring(g_trayPorts[i]);
        tip += (i < g_trayStates.size() && g_trayStates[i]) ? L": 占用\n" : L": 空闲\n";
    }
    if (g_trayPorts.size() > 8) tip += L"...\n";
    return tip;
}

void fillIconData(NOTIFYICONDATAW& nid, DWORD flags) {
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = g_trayWnd;
    nid.uID = TRAY_ID;
    nid.uFlags = flags;
    nid.uCallbackMessage = WM_TRAYICON;
    if (g_trayIcon) nid.hIcon = g_trayIcon;
    if (flags & NIF_TIP) {
        std::wstring tip = buildTooltip();
        lstrcpynW(nid.szTip, tip.c_str(), ARRAYSIZE(nid.szTip));
    }
}

bool addTrayIcon() {
    if (!g_trayIcon) g_trayIcon = LoadIconW(NULL, MAKEINTRESOURCEW(32512));  // IDI_APPLICATION
    NOTIFYICONDATAW nid;
    fillIconData(nid, NIF_ICON | NIF_MESSAGE | NIF_TIP);
    return Shell_NotifyIconW(NIM_ADD, &nid) != FALSE;
}

void removeTrayIcon() {
    NOTIFYICONDATAW nid;
    fillIconData(nid, 0);
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void updateTrayTip() {
    NOTIFYICONDATAW nid;
    fillIconData(nid, NIF_TIP);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void showBalloon(const std::wstring& title, const std::wstring& msg, DWORD flags) {
    NOTIFYICONDATAW nid;
    fillIconData(nid, NIF_INFO);
    nid.dwInfoFlags = flags;
    lstrcpynW(nid.szInfoTitle, title.c_str(), ARRAYSIZE(nid.szInfoTitle));
    lstrcpynW(nid.szInfo, msg.c_str(), ARRAYSIZE(nid.szInfo));
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void pollTrayPorts(bool initial) {
    for (size_t i = 0; i < g_trayPorts.size(); i++) {
        bool cur = isPortOccupiedForTray(g_trayPorts[i]);
        if (i >= g_trayStates.size()) {
            g_trayStates.resize(g_trayPorts.size(), false);
        }
        bool last = g_trayStates[i];
        if (initial || cur == last) {
            g_trayStates[i] = cur;
            continue;
        }
        g_trayStates[i] = cur;

        std::wstring title = L"端口 " + std::to_wstring(g_trayPorts[i]) +
                             (cur ? L" 被占用" : L" 已释放");
        std::wstring msg;
        if (cur) {
            PortInfo tcp = PortChecker::checkPort(g_trayPorts[i], "TCP");
            uint32_t pid = tcp.pid;
            std::string name = (pid > 0) ? ProcessManager::getProcessName(pid) : "";
            msg = L"占用进程: " + std::wstring(name.begin(), name.end()) +
                  L" (PID: " + std::to_wstring(pid) + L")";
        } else {
            msg = L"端口现已空闲，可以使用了";
        }
        showBalloon(title, msg, cur ? NIIF_WARNING : NIIF_INFO);
        beepAlert(cur);
    }
    updateTrayTip();
}

void showConsoleWindow() {
    HWND console = GetConsoleWindow();
    if (console) {
        ShowWindow(console, SW_SHOW);
        ShowWindow(console, SW_RESTORE);
        SetForegroundWindow(console);
    }
}

void showTrayMenu() {
    POINT pt;
    GetCursorPos(&pt);
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, CMD_SHOW_CONSOLE, L"显示控制台");
    AppendMenuW(menu, MF_STRING, CMD_REFRESH, L"刷新状态");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, CMD_EXIT, L"退出");

    SetForegroundWindow(g_trayWnd);
    UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                              pt.x, pt.y, 0, g_trayWnd, NULL);
    DestroyMenu(menu);

    if (cmd == CMD_SHOW_CONSOLE) {
        showConsoleWindow();
    } else if (cmd == CMD_REFRESH) {
        pollTrayPorts(false);
        showBalloon(L"PortLens", L"状态已刷新", NIIF_INFO);
    } else if (cmd == CMD_EXIT) {
        DestroyWindow(g_trayWnd);
    }
}

LRESULT CALLBACK trayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TIMER:
            if (wParam == TIMER_ID) pollTrayPorts(false);
            return 0;

        case WM_TRAYICON: {
            UINT mouseMsg = (UINT)(lParam & 0xFFFF);
            if (mouseMsg == WM_RBUTTONUP || mouseMsg == WM_CONTEXTMENU) {
                showTrayMenu();
            } else if (mouseMsg == WM_LBUTTONDBLCLK) {
                showConsoleWindow();
            }
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, TIMER_ID);
            removeTrayIcon();
            PostQuitMessage(0);
            return 0;

        default:
            if (g_taskbarCreatedMsg != 0 && msg == g_taskbarCreatedMsg) {
                addTrayIcon();
                updateTrayTip();
                return 0;
            }
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
}  // namespace

int runTray(const std::vector<uint16_t>& ports, int intervalMs) {
    if (ports.empty()) {
        printWarning("没有可监控的端口，请先添加收藏夹: port fav add <端口>");
        return 2;
    }
    if (intervalMs < 500) intervalMs = 500;

    g_trayPorts = ports;
    g_trayStates.assign(ports.size(), false);
    g_taskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");

    // 仅在独占控制台时隐藏窗口；从 CMD 启动时保留用户的终端窗口
    HWND console = GetConsoleWindow();
    if (console) {
        DWORD owners[2] = {0};
        DWORD count = GetConsoleProcessList(owners, 2);
        if (count <= 1) ShowWindow(console, SW_HIDE);
    }

    HINSTANCE hInst = GetModuleHandleW(NULL);
    const wchar_t* CLASS_NAME = L"PortLensTrayWindow";

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = trayWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    RegisterClassExW(&wc);

    g_trayWnd = CreateWindowExW(0, CLASS_NAME, L"PortLens Tray", WS_OVERLAPPED,
                                0, 0, 0, 0, NULL, NULL, hInst, NULL);
    if (!g_trayWnd) {
        printError("创建托盘窗口失败");
        showConsoleWindow();
        return 2;
    }

    if (!addTrayIcon()) {
        printError("创建托盘图标失败");
        DestroyWindow(g_trayWnd);
        showConsoleWindow();
        return 2;
    }

    pollTrayPorts(true);  // 初始化状态，不弹提醒
    SetTimer(g_trayWnd, TIMER_ID, (UINT)intervalMs, NULL);

    std::wstring tip = L"PortLens 已开始监控 " + std::to_wstring(g_trayPorts.size()) +
                       L" 个端口，右键托盘图标可退出";
    showBalloon(L"PortLens 端口监控", tip, NIIF_INFO);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    showConsoleWindow();
    printInfo("托盘监控已退出");
    return 0;
}
