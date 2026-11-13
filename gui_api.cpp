#include "gui_api.h"
#include "port_checker.h"
#include "process_manager.h"
#include "config_manager.h"
#include "special_process.h"

#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>

// main.cpp 中已有的实现
std::string getPortDesc(int port);
extern "C" int portlens_kill_one(unsigned int pid, const char* procName, const char* portsText);
extern "C" int portlens_install_path(void);

namespace {

std::string jesc(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    o += buf;
                } else {
                    o += (char)c;
                }
        }
    }
    return o;
}

std::string js(const std::string& s) { return "\"" + jesc(s) + "\""; }

char* dup(const std::string& s) {
    char* p = (char*)malloc(s.size() + 1);
    if (!p) return nullptr;
    memcpy(p, s.c_str(), s.size() + 1);
    return p;
}

std::string lower(std::string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

std::string portRowJson(const PortInfo& p, const std::string& name, const std::string& path,
                        const std::string& special, const std::string& desc, bool fav) {
    std::ostringstream o;
    o << "{"
      << "\"port\":" << p.port
      << ",\"protocol\":" << js(p.protocol)
      << ",\"state\":" << js(p.state)
      << ",\"localAddr\":" << js(p.localAddr)
      << ",\"remoteAddr\":" << js(p.remoteAddr)
      << ",\"remotePort\":" << p.remotePort
      << ",\"pid\":" << p.pid
      << ",\"name\":" << js(name)
      << ",\"path\":" << js(path)
      << ",\"special\":" << js(special)
      << ",\"desc\":" << js(desc)
      << ",\"fav\":" << (fav ? "true" : "false")
      << "}";
    return o.str();
}

struct Enriched {
    PortInfo info;
    std::string name;
    std::string path;
    std::string special;
    std::string desc;
    bool fav;
};

std::vector<Enriched> collect(const std::string& proto) {
    std::vector<PortInfo> list = PortChecker::getAllListeningPorts(proto);
    std::map<uint32_t, std::string> nameCache, pathCache;
    std::vector<Enriched> out;
    out.reserve(list.size());
    for (const auto& p : list) {
        Enriched e;
        e.info = p;
        auto it = nameCache.find(p.pid);
        if (it == nameCache.end()) {
            e.name = ProcessManager::getProcessName(p.pid);
            nameCache[p.pid] = e.name;
        } else {
            e.name = it->second;
        }
        auto ip = pathCache.find(p.pid);
        if (ip == pathCache.end()) {
            e.path = ProcessManager::getProcessPath(p.pid);
            pathCache[p.pid] = e.path;
        } else {
            e.path = ip->second;
        }
        e.special = SpecialProcessDetector::detect(e.name, p.port);
        e.desc = getPortDesc(p.port);
        e.fav = FavoritesManager::contains(p.port);
        out.push_back(e);
    }
    return out;
}

std::string rowsToJson(const std::vector<Enriched>& rows) {
    std::ostringstream o;
    o << "[";
    for (size_t i = 0; i < rows.size(); i++) {
        if (i) o << ",";
        o << portRowJson(rows[i].info, rows[i].name, rows[i].path, rows[i].special, rows[i].desc, rows[i].fav);
    }
    o << "]";
    return o.str();
}

} // namespace

// 该工具链未提供 libstrsafe.a，winpthread 需要此符号，这里做最小实现
extern "C" long StringCchCopyNA(char* dest, size_t cchDest, const char* src, size_t cchToCopy) {
    if (!dest || cchDest == 0) return -2147024809L; // E_INVALIDARG
    size_t n = 0;
    if (src) {
        while (n < cchToCopy && src[n]) n++;
    }
    if (n >= cchDest) n = cchDest - 1;
    if (n) memcpy(dest, src, n);
    dest[n] = 0;
    return 0; // S_OK
}

extern "C" {

int pl_init(void) {
    ConfigManager::load();
    return PortChecker::init() ? 1 : 0;
}

void pl_cleanup(void) {
    PortChecker::cleanup();
}

static std::string exeNameOf(DWORD pid, HANDLE snap) {
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (!Process32FirstW(snap, &pe)) return "";
    do {
        if (pe.th32ProcessID == pid) {
            char buf[512] = {0};
            WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, buf, sizeof(buf) - 1, NULL, NULL);
            std::string s = buf;
            for (char& c : s) c = (char)tolower((unsigned char)c);
            return s;
        }
    } while (Process32NextW(snap, &pe));
    return "";
}

int pl_should_gui(void) {
    DWORD myPid = GetCurrentProcessId();
    DWORD parentPid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap, &pe)) {
            do {
                if (pe.th32ProcessID == myPid) {
                    parentPid = pe.th32ParentProcessID;
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }
        std::string parent = parentPid ? exeNameOf(parentPid, snap) : "";
        CloseHandle(snap);
        // 资源管理器双击启动
        if (parent == "explorer.exe") return 1;
    }
    // 独占控制台（双击时系统新建的黑窗）也视为 GUI
    DWORD pids[8];
    DWORD n = GetConsoleProcessList(pids, 8);
    if (n <= 1) return 1;
    return 0;
}

void pl_free_console(void) {
    FreeConsole();
}

char* pl_list_ports(int proto) {
    const char* p = proto == 0 ? "TCP" : (proto == 1 ? "UDP" : "ALL");
    return dup(rowsToJson(collect(p)));
}

char* pl_scan_ports(int startPort, int endPort) {
    if (startPort < 1) startPort = 1;
    if (endPort > 65535) endPort = 65535;
    if (endPort < startPort) return dup("[]");
    std::vector<PortInfo> occupied = PortChecker::scanPorts((uint16_t)startPort, (uint16_t)endPort);
    std::vector<Enriched> rows;
    for (const auto& p : occupied) {
        Enriched e;
        e.info = p;
        e.name = ProcessManager::getProcessName(p.pid);
        e.path = ProcessManager::getProcessPath(p.pid);
        e.special = SpecialProcessDetector::detect(e.name, p.port);
        e.desc = getPortDesc(p.port);
        e.fav = FavoritesManager::contains(p.port);
        rows.push_back(e);
    }
    return dup(rowsToJson(rows));
}

char* pl_check_port(int port) {
    std::ostringstream o;
    PortInfo tcp = PortChecker::checkPort((uint16_t)port, "TCP");
    PortInfo udp = PortChecker::checkPort((uint16_t)port, "UDP");

    auto one = [&](const PortInfo& p, bool occupied) {
        std::ostringstream x;
        x << "{\"occupied\":" << (occupied ? "true" : "false")
          << ",\"pid\":" << p.pid
          << ",\"name\":" << js(p.pid ? ProcessManager::getProcessName(p.pid) : "")
          << ",\"path\":" << js(p.pid ? ProcessManager::getProcessPath(p.pid) : "")
          << ",\"localAddr\":" << js(p.localAddr)
          << ",\"special\":" << js(p.pid ? SpecialProcessDetector::detect(ProcessManager::getProcessName(p.pid), port) : "")
          << "}";
        return x.str();
    };

    bool tcpOcc = tcp.pid != 0;
    bool udpOcc = udp.pid != 0;

    o << "{\"port\":" << port
      << ",\"desc\":" << js(getPortDesc(port))
      << ",\"fav\":" << (FavoritesManager::contains(port) ? "true" : "false")
      << ",\"tcp\":" << one(tcp, tcpOcc)
      << ",\"udp\":" << one(udp, udpOcc)
      << ",\"connections\":[";

    std::vector<PortInfo> conns = PortChecker::getPortConnections((uint16_t)port);
    for (size_t i = 0; i < conns.size(); i++) {
        if (i) o << ",";
        o << "{\"protocol\":" << js(conns[i].protocol)
          << ",\"state\":" << js(conns[i].state)
          << ",\"localAddr\":" << js(conns[i].localAddr)
          << ",\"remoteAddr\":" << js(conns[i].remoteAddr)
          << ",\"remotePort\":" << conns[i].remotePort
          << ",\"pid\":" << conns[i].pid << "}";
    }
    o << "],\"occupied\":" << ((tcpOcc || udpOcc) ? "true" : "false") << "}";
    return dup(o.str());
}

char* pl_port_connections(int port) {
    std::vector<PortInfo> conns = PortChecker::getPortConnections((uint16_t)port);
    std::ostringstream o;
    o << "[";
    for (size_t i = 0; i < conns.size(); i++) {
        if (i) o << ",";
        o << "{\"protocol\":" << js(conns[i].protocol)
          << ",\"state\":" << js(conns[i].state)
          << ",\"localAddr\":" << js(conns[i].localAddr)
          << ",\"remoteAddr\":" << js(conns[i].remoteAddr)
          << ",\"remotePort\":" << conns[i].remotePort
          << ",\"pid\":" << conns[i].pid << "}";
    }
    o << "]";
    return dup(o.str());
}

char* pl_process_detail(unsigned int pid) {
    ProcessDetail d = ProcessManager::getProcessDetail(pid);
    std::ostringstream o;
    o << "{\"pid\":" << d.pid
      << ",\"name\":" << js(d.name)
      << ",\"path\":" << js(d.path)
      << ",\"memoryKb\":" << d.memoryKb
      << ",\"peakMemoryKb\":" << d.peakMemoryKb
      << ",\"threadCount\":" << d.threadCount
      << ",\"parentPid\":" << d.parentPid
      << ",\"startTime\":" << js(d.startTime)
      << ",\"company\":" << js(d.company)
      << ",\"description\":" << js(d.description)
      << "}";
    return dup(o.str());
}

char* pl_find_process(const char* name, int portFilter) {
    std::string needle = lower(name ? name : "");
    auto rows = collect("ALL");

    struct Group {
        uint32_t pid = 0;
        std::string name, path, special;
        std::vector<std::string> ports;
    };
    std::map<uint32_t, Group> groups;
    for (const auto& e : rows) {
        if (e.info.pid == 0) continue;
        if (!needle.empty() && lower(e.name).find(needle) == std::string::npos) continue;
        if (portFilter > 0 && e.info.port != portFilter) continue;
        Group& g = groups[e.info.pid];
        g.pid = e.info.pid;
        g.name = e.name;
        g.path = e.path;
        g.special = e.special;
        g.ports.push_back(e.info.protocol + ":" + std::to_string(e.info.port));
    }

    std::ostringstream o;
    o << "[";
    bool first = true;
    for (auto& kv : groups) {
        if (!first) o << ",";
        first = false;
        o << "{\"pid\":" << kv.second.pid
          << ",\"name\":" << js(kv.second.name)
          << ",\"path\":" << js(kv.second.path)
          << ",\"special\":" << js(kv.second.special)
          << ",\"ports\":[";
        for (size_t i = 0; i < kv.second.ports.size(); i++) {
            if (i) o << ",";
            o << js(kv.second.ports[i]);
        }
        o << "]}";
    }
    o << "]";
    return dup(o.str());
}

int pl_kill_pid(unsigned int pid) {
    std::string name = ProcessManager::getProcessName(pid);
    if (name.empty()) return 0;
    auto rows = collect("ALL");
    std::string ports;
    for (const auto& e : rows) {
        if (e.info.pid == pid) {
            if (!ports.empty()) ports += ",";
            ports += e.info.protocol + ":" + std::to_string(e.info.port);
        }
    }
    return portlens_kill_one(pid, name.c_str(), ports.c_str()) ? 1 : 0;
}

int pl_find_free_port(int startPort) {
    if (startPort < 1) startPort = 1;
    for (int p = startPort; p <= 65535; p++) {
        PortInfo tcp = PortChecker::checkPort((uint16_t)p, "TCP");
        if (tcp.pid == 0) return p;
    }
    return 0;
}

int pl_export(const char* path, int proto, int fmt) {
    const char* p = proto == 0 ? "TCP" : (proto == 1 ? "UDP" : "ALL");
    auto rows = collect(p);
    std::ostringstream o;
    if (fmt == 0) {
        o << "\xEF\xBB\xBF";  // UTF-8 BOM，Excel 直接打开
        o << "端口,协议,状态,进程,PID,路径,说明\r\n";
        for (const auto& e : rows) {
            o << e.info.port << "," << e.info.protocol << "," << e.info.state << ","
              << e.name << "," << e.info.pid << "," << e.path << "," << e.desc << "\r\n";
        }
    } else {
        o << "[";
        for (size_t i = 0; i < rows.size(); i++) {
            if (i) o << ",";
            o << portRowJson(rows[i].info, rows[i].name, rows[i].path, rows[i].special, rows[i].desc, rows[i].fav);
        }
        o << "]";
    }
    return ConfigManager::writeTextFile(path ? path : "", o.str()) ? 1 : 0;
}

char* pl_config_info(void) {
    std::ostringstream o;
    o << "{\"configPath\":" << js(ConfigManager::getConfigPath())
      << ",\"dataDir\":" << js(ConfigManager::getDataDir())
      << ",\"scanStart\":" << ConfigManager::defaultScanStart()
      << ",\"scanEnd\":" << ConfigManager::defaultScanEnd()
      << ",\"historyLimit\":" << ConfigManager::historyLimit()
      << ",\"watchInterval\":" << ConfigManager::watchIntervalMs()
      << ",\"aliases\":[";
    auto aliases = ConfigManager::listAliases();
    for (size_t i = 0; i < aliases.size(); i++) {
        if (i) o << ",";
        o << "{\"name\":" << js(aliases[i].first) << ",\"port\":" << aliases[i].second << "}";
    }
    o << "],\"favorites\":[";
    auto favs = FavoritesManager::list();
    for (size_t i = 0; i < favs.size(); i++) {
        if (i) o << ",";
        o << favs[i];
    }
    o << "]}";
    return dup(o.str());
}

char* pl_favorites(void) {
    std::ostringstream o;
    o << "[";
    auto favs = FavoritesManager::list();
    for (size_t i = 0; i < favs.size(); i++) {
        int port = favs[i];
        PortInfo tcp = PortChecker::checkPort((uint16_t)port, "TCP");
        std::string name = tcp.pid ? ProcessManager::getProcessName(tcp.pid) : "";
        std::string special = tcp.pid ? SpecialProcessDetector::detect(name, port) : "";
        if (i) o << ",";
        o << "{\"port\":" << port
          << ",\"occupied\":" << (tcp.pid ? "true" : "false")
          << ",\"pid\":" << tcp.pid
          << ",\"name\":" << js(name)
          << ",\"path\":" << js(tcp.pid ? ProcessManager::getProcessPath(tcp.pid) : "")
          << ",\"special\":" << js(special)
          << ",\"desc\":" << js(getPortDesc(port))
          << "}";
    }
    o << "]";
    return dup(o.str());
}

int pl_fav_add(int port) {
    if (port < 1 || port > 65535) return 0;
    return FavoritesManager::add(port) ? 1 : 0;
}

int pl_fav_remove(int port) {
    return FavoritesManager::remove(port) ? 1 : 0;
}

char* pl_history(void) {
    std::ostringstream o;
    o << "[";
    auto list = HistoryManager::list(0);
    for (size_t i = 0; i < list.size(); i++) {
        if (i) o << ",";
        o << "{\"time\":" << js(list[i].time)
          << ",\"pid\":" << list[i].pid
          << ",\"procName\":" << js(list[i].procName)
          << ",\"path\":" << js(list[i].path)
          << ",\"ports\":" << js(list[i].ports) << "}";
    }
    o << "]";
    return dup(o.str());
}

int pl_history_clear(void) {
    return HistoryManager::clear() ? 1 : 0;
}

int pl_install_path(void) {
    return portlens_install_path();
}

void pl_free_string(char* s) {
    free(s);
}

} // extern "C"
