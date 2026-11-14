#include "config_manager.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <sstream>

// ========== 静态成员 ==========
bool ConfigManager::s_loaded = false;
std::map<std::string, int> ConfigManager::s_aliases;
std::map<int, std::string> ConfigManager::s_descs;
int ConfigManager::s_scanStart = 1;
int ConfigManager::s_scanEnd = 1024;
int ConfigManager::s_historyLimit = 50;
int ConfigManager::s_watchInterval = 1000;
std::string ConfigManager::s_configPath;

// ========== 内部工具 ==========

static std::wstring getExeDirW() {
    wchar_t buf[MAX_PATH] = {0};
    DWORD n = GetModuleFileNameW(NULL, buf, MAX_PATH);
    if (n == 0) return L".";
    std::wstring p(buf, n);
    size_t pos = p.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? L"." : p.substr(0, pos);
}

static std::wstring joinPathW(const std::wstring& dir, const std::wstring& name) {
    if (dir.empty()) return name;
    wchar_t last = dir[dir.size() - 1];
    if (last == L'\\' || last == L'/') return dir + name;
    return dir + L"\\" + name;
}

static bool fileExistsW(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

static void ensureDirW(const std::wstring& dir) {
    if (dir.empty()) return;
    DWORD attr = GetFileAttributesW(dir.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) return;
    CreateDirectoryW(dir.c_str(), NULL);
}

static bool readFileUtf8(const std::wstring& path, std::string& out) {
    out.clear();
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;

    char buf[8192];
    DWORD n = 0;
    while (ReadFile(h, buf, sizeof(buf), &n, NULL) && n > 0) {
        out.append(buf, n);
    }
    CloseHandle(h);

    // 去掉 UTF-8 BOM
    if (out.size() >= 3 && (unsigned char)out[0] == 0xEF &&
        (unsigned char)out[1] == 0xBB && (unsigned char)out[2] == 0xBF) {
        out.erase(0, 3);
    }
    return true;
}

static bool writeFileUtf8(const std::wstring& path, const std::string& data, bool append) {
    DWORD access = append ? FILE_APPEND_DATA : GENERIC_WRITE;
    DWORD disp = append ? OPEN_ALWAYS : CREATE_ALWAYS;
    HANDLE h = CreateFileW(path.c_str(), access, FILE_SHARE_READ,
                           NULL, disp, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;

    bool ok = true;
    DWORD written = 0;
    if (!data.empty()) {
        ok = WriteFile(h, data.data(), (DWORD)data.size(), &written, NULL) != 0;
    }
    // 追加模式下强制刷新到磁盘，避免被其他进程读到旧内容
    FlushFileBuffers(h);
    CloseHandle(h);
    return ok && written == data.size();
}

static std::string trimStr(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static std::string lowerStr(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    return r;
}

static std::string nowString() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return std::string(buf);
}

static std::string sanitizeField(const std::string& s) {
    std::string r = s;
    for (char& c : r) {
        if (c == '\t' || c == '\r' || c == '\n') c = ' ';
    }
    return r;
}

// ========== ConfigManager ==========

std::string ConfigManager::toUtf8(const std::wstring& s) {
    if (s.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0, NULL, NULL);
    std::string r(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &r[0], len, NULL, NULL);
    return r;
}

std::wstring ConfigManager::toWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    std::wstring r(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &r[0], len);
    return r;
}

std::wstring ConfigManager::getDataDirW() {
    wchar_t buf[MAX_PATH] = {0};
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    std::wstring base = (n > 0 && n < MAX_PATH) ? std::wstring(buf, n) : getExeDirW();
    std::wstring dir = joinPathW(base, L"PortLens");
    ensureDirW(dir);
    return dir;
}

std::string ConfigManager::getDataDir() {
    return toUtf8(getDataDirW());
}

std::string ConfigManager::getConfigPath() {
    return s_configPath;
}

void ConfigManager::reload() {
    s_loaded = false;
    s_aliases.clear();
    s_descs.clear();
    s_scanStart = 1;
    s_scanEnd = 1024;
    s_historyLimit = 50;
    s_watchInterval = 1000;
    s_configPath.clear();
    load();
}

void ConfigManager::load() {
    if (s_loaded) return;
    s_loaded = true;

    // 查找配置文件: exe 目录优先，其次数据目录
    std::wstring exeConf = joinPathW(getExeDirW(), L"port.conf");
    std::wstring dataConf = joinPathW(getDataDirW(), L"port.conf");

    std::wstring usePath;
    if (fileExistsW(exeConf)) usePath = exeConf;
    else if (fileExistsW(dataConf)) usePath = dataConf;
    else {
        // 无配置文件时启用内置常用别名，开箱即用
        s_aliases["mysql"] = 3306;
        s_aliases["redis"] = 6379;
        s_aliases["mongo"] = 27017;
        s_aliases["postgres"] = 5432;
        s_aliases["nginx"] = 80;
        s_configPath = toUtf8(exeConf);  // 默认展示 exe 目录位置
        return;
    }

    s_configPath = toUtf8(usePath);

    std::string content;
    if (!readFileUtf8(usePath, content)) return;

    std::string section;
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        line = trimStr(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        if (line.front() == '[' && line.back() == ']') {
            section = lowerStr(trimStr(line.substr(1, line.size() - 2)));
            continue;
        }

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trimStr(line.substr(0, eq));
        std::string val = trimStr(line.substr(eq + 1));
        if (key.empty() || val.empty()) continue;

        if (section == "alias") {
            int p = atoi(val.c_str());
            if (p >= 1 && p <= 65535) {
                s_aliases[lowerStr(key)] = p;
            }
        } else if (section == "desc") {
            int p = atoi(key.c_str());
            if (p >= 1 && p <= 65535) {
                s_descs[p] = val;
            }
        } else if (section == "settings") {
            std::string k = lowerStr(key);
            int v = atoi(val.c_str());
            if (k == "scan_start" && v >= 1 && v <= 65535) s_scanStart = v;
            else if (k == "scan_end" && v >= 1 && v <= 65535) s_scanEnd = v;
            else if (k == "history_limit" && v >= 1 && v <= 1000) s_historyLimit = v;
            else if (k == "watch_interval" && v >= 200 && v <= 60000) s_watchInterval = v;
        }
    }
}

bool ConfigManager::writeDefaultConfigIfMissing() {
    std::wstring exeConf = joinPathW(getExeDirW(), L"port.conf");
    if (fileExistsW(exeConf)) return false;

    const char* tmpl =
        "# PortLens 配置文件\n"
        "# 修改后立即生效，无需重启\n"
        "\n"
        "[alias]\n"
        "# 端口别名: port mysql 等价于 port 3306\n"
        "mysql = 3306\n"
        "redis = 6379\n"
        "mongo = 27017\n"
        "postgres = 5432\n"
        "nginx = 80\n"
        "\n"
        "[settings]\n"
        "# 扫描功能默认端口范围\n"
        "scan_start = 1\n"
        "scan_end = 1024\n"
        "# 历史记录保留条数\n"
        "history_limit = 50\n"
        "# 监控刷新间隔（毫秒）\n"
        "watch_interval = 1000\n"
        "\n"
        "[desc]\n"
        "# 自定义端口说明: 端口 = 说明\n"
        "3000 = 前端开发服务器\n"
        "5173 = Vite 开发服务器\n"
        "8001 = 本地测试服务\n";

    return writeFileUtf8(exeConf, tmpl, false);
}

int ConfigManager::resolveAlias(const std::string& name) {
    load();
    auto it = s_aliases.find(lowerStr(name));
    return (it == s_aliases.end()) ? 0 : it->second;
}

std::string ConfigManager::getCustomPortDesc(int port) {
    load();
    auto it = s_descs.find(port);
    return (it == s_descs.end()) ? "" : it->second;
}

std::vector<std::pair<std::string, int>> ConfigManager::listAliases() {
    load();
    std::vector<std::pair<std::string, int>> result(s_aliases.begin(), s_aliases.end());
    return result;
}

bool ConfigManager::writeTextFile(const std::string& utf8Path, const std::string& data) {
    return writeFileUtf8(toWide(utf8Path), data, false);
}

int ConfigManager::defaultScanStart() { load(); return s_scanStart; }
int ConfigManager::defaultScanEnd() { load(); return s_scanEnd; }
int ConfigManager::historyLimit() { load(); return s_historyLimit; }
int ConfigManager::watchIntervalMs() { load(); return s_watchInterval; }

// ========== HistoryManager ==========

std::wstring HistoryManager::getHistoryPathW() {
    return joinPathW(ConfigManager::getDataDirW(), L"history.tsv");
}

std::string HistoryManager::getHistoryPath() {
    return ConfigManager::toUtf8(getHistoryPathW());
}

void HistoryManager::record(uint32_t pid, const std::string& procName,
                            const std::string& path, const std::string& ports) {
    std::string line = nowString() + "\t" + std::to_string(pid) + "\t" +
                       sanitizeField(procName) + "\t" + sanitizeField(path) + "\t" +
                       sanitizeField(ports) + "\n";

    // 先追加，再裁剪，保证进程被系统回收后记录依然存在
    writeFileUtf8(getHistoryPathW(), line, true);

    int limit = ConfigManager::historyLimit();
    auto all = list(limit > 0 ? limit : 50);
    if ((int)all.size() <= limit) return;

    // 超出上限: 重写最新 limit 条（list 返回倒序，写回时反转）
    std::string rebuilt;
    for (int i = (int)all.size() - 1; i >= 0 && (int)rebuilt.size() < 1024 * 1024; i--) {
        const KillRecord& r = all[i];
        rebuilt += r.time + "\t" + std::to_string(r.pid) + "\t" +
                   sanitizeField(r.procName) + "\t" + sanitizeField(r.path) + "\t" +
                   sanitizeField(r.ports) + "\n";
    }
    writeFileUtf8(getHistoryPathW(), rebuilt, false);
}

std::vector<KillRecord> HistoryManager::list(int limit) {
    std::vector<KillRecord> result;
    std::string content;
    if (!readFileUtf8(getHistoryPathW(), content)) return result;

    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        line = trimStr(line);
        if (line.empty()) continue;

        std::vector<std::string> f;
        size_t start = 0;
        while (true) {
            size_t pos = line.find('\t', start);
            if (pos == std::string::npos) {
                f.push_back(line.substr(start));
                break;
            }
            f.push_back(line.substr(start, pos - start));
            start = pos + 1;
        }
        if (f.size() < 5) continue;

        KillRecord r;
        r.time = f[0];
        r.pid = (uint32_t)atol(f[1].c_str());
        r.procName = f[2];
        r.path = f[3];
        r.ports = f[4];
        result.push_back(r);
    }

    // 文件按时间正序存储，返回时倒序（最新在前）
    std::reverse(result.begin(), result.end());

    int n = (limit <= 0) ? ConfigManager::historyLimit() : limit;
    if (n > 0 && (int)result.size() > n) {
        result.resize(n);
    }
    return result;
}

int HistoryManager::count() {
    return (int)list(0).size();
}

bool HistoryManager::clear() {
    return writeFileUtf8(getHistoryPathW(), "", false);
}

// ========== FavoritesManager ==========

std::wstring FavoritesManager::getFavoritesPathW() {
    return joinPathW(ConfigManager::getDataDirW(), L"favorites.txt");
}

std::string FavoritesManager::getFavoritesPath() {
    return ConfigManager::toUtf8(getFavoritesPathW());
}

std::vector<int> FavoritesManager::list() {
    std::vector<int> result;
    std::string content;
    if (!readFileUtf8(getFavoritesPathW(), content)) return result;

    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        line = trimStr(line);
        if (line.empty()) continue;
        int p = atoi(line.c_str());
        if (p >= 1 && p <= 65535) {
            if (std::find(result.begin(), result.end(), p) == result.end()) {
                result.push_back((int)p);
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

static bool saveFavorites(const std::vector<int>& ports) {
    std::string data;
    for (int p : ports) {
        data += std::to_string(p) + "\n";
    }
    HANDLE h = CreateFileW(FavoritesManager::getFavoritesPathW().c_str(), GENERIC_WRITE,
                           FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = data.empty() || WriteFile(h, data.data(), (DWORD)data.size(), &written, NULL);
    CloseHandle(h);
    return ok;
}

bool FavoritesManager::add(int port) {
    if (port < 1 || port > 65535) return false;
    auto ports = list();
    if (std::find(ports.begin(), ports.end(), port) != ports.end()) return true;  // 已存在视为成功
    ports.push_back(port);
    std::sort(ports.begin(), ports.end());
    return saveFavorites(ports);
}

bool FavoritesManager::remove(int port) {
    auto ports = list();
    size_t before = ports.size();
    ports.erase(std::remove(ports.begin(), ports.end(), port), ports.end());
    if (ports.size() == before) return false;  // 不存在
    return saveFavorites(ports);
}

bool FavoritesManager::contains(int port) {
    auto ports = list();
    return std::find(ports.begin(), ports.end(), port) != ports.end();
}
