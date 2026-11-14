#include "special_process.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <algorithm>

static std::map<int, std::string> s_dockerPorts;  // 宿主机端口 → 容器名
static bool s_dockerLoaded = false;

static std::string lowerStr(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

// 去掉 .exe 后缀，便于统一比较
static std::string normalizeName(const std::string& name) {
    std::string n = lowerStr(name);
    if (n.size() > 4 && n.compare(n.size() - 4, 4, ".exe") == 0) {
        n = n.substr(0, n.size() - 4);
    }
    return n;
}

static std::string trimStr(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

bool SpecialProcessDetector::isDockerProcess(const std::string& procName) {
    std::string n = normalizeName(procName);
    return n == "com.docker.backend" || n == "com.docker.cli" ||
           n == "docker-proxy" || n == "dockerd" || n == "com.docker.build" ||
           n == "docker-desktop";
}

bool SpecialProcessDetector::isWslProcess(const std::string& procName) {
    std::string n = normalizeName(procName);
    return n == "wslrelay" || n == "wslhost" || n == "wslservice" ||
           n == "vmmemwsl" || n == "wsl";
}

bool SpecialProcessDetector::isVmProcess(const std::string& procName) {
    std::string n = normalizeName(procName);
    return n == "vmmem" || n == "vmwp" || n == "vmcompute";
}

void SpecialProcessDetector::resetCache() {
    s_dockerPorts.clear();
    s_dockerLoaded = false;
}

// 解析 docker ps 的一行: "容器名<TAB>0.0.0.0:6379->6379/tcp, :::6379->6379/tcp"
static void parseDockerLine(const std::string& raw) {
    std::string line = raw;
    size_t tab = line.find('\t');
    if (tab == std::string::npos) return;

    std::string name = trimStr(line.substr(0, tab));
    std::string ports = line.substr(tab + 1);
    if (name.empty()) return;

    size_t start = 0;
    while (start <= ports.size()) {
        size_t comma = ports.find(',', start);
        std::string seg = (comma == std::string::npos)
                              ? ports.substr(start)
                              : ports.substr(start, comma - start);
        start = (comma == std::string::npos) ? ports.size() + 1 : comma + 1;

        size_t arrow = seg.find("->");
        if (arrow == std::string::npos) continue;

        std::string host = seg.substr(0, arrow);   // "0.0.0.0:6379" 或 "[::]:6379"
        size_t colon = host.rfind(':');
        if (colon == std::string::npos) continue;

        std::string portStr = host.substr(colon + 1);
        size_t dash = portStr.find('-');
        int p1 = atoi((dash == std::string::npos ? portStr : portStr.substr(0, dash)).c_str());
        int p2 = (dash == std::string::npos) ? p1 : atoi(portStr.substr(dash + 1).c_str());
        if (p1 < 1 || p1 > 65535) continue;
        if (p2 < p1) p2 = p1;
        if (p2 - p1 > 64) p2 = p1 + 64;  // 防止异常范围导致循环过大

        for (int p = p1; p <= p2; p++) {
            s_dockerPorts[p] = name;
        }
    }
}

void SpecialProcessDetector::ensureDockerMap() {
    if (s_dockerLoaded) return;
    s_dockerLoaded = true;

    // Docker 未安装或未运行时命令会失败，直接跳过
    FILE* fp = _popen("docker ps --format \"{{.Names}}\t{{.Ports}}\" 2>nul", "r");
    if (!fp) return;

    char buf[4096];
    while (fgets(buf, sizeof(buf), fp)) {
        parseDockerLine(buf);
    }
    _pclose(fp);
}

std::string SpecialProcessDetector::detect(const std::string& procName, int port) {
    if (isDockerProcess(procName)) {
        ensureDockerMap();
        auto it = s_dockerPorts.find(port);
        if (it != s_dockerPorts.end()) {
            return "[Docker: " + it->second + "]";
        }
        return "[Docker]";
    }
    if (isWslProcess(procName)) return "[WSL]";
    if (isVmProcess(procName)) return "[Hyper-V]";
    return "";
}
