#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <cstdint>

// 一次结束进程的历史记录
struct KillRecord {
    std::string time;      // 记录时间
    uint32_t pid;
    std::string procName;
    std::string path;
    std::string ports;     // 涉及的端口，如 "TCP:8001"
};

// 配置文件管理（port.conf）
// 查找顺序: 程序目录 port.conf → %LOCALAPPDATA%\PortLens\port.conf
class ConfigManager {
public:
    static void load();
    static void reload();

    // 返回实际使用的配置文件路径（exe 目录优先，其次数据目录）
    static std::string getConfigPath();

    // 配置文件不存在时，在 exe 目录生成一份带注释的模板
    static bool writeDefaultConfigIfMissing();

    // 端口别名: port mysql → 3306；未找到返回 0
    static int resolveAlias(const std::string& name);

    // 自定义端口说明；未配置返回空串
    static std::string getCustomPortDesc(int port);

    // 全部别名（按名称排序）
    static std::vector<std::pair<std::string, int>> listAliases();

    // 写文本文件（UTF-8 内容，路径为 UTF-8）
    static bool writeTextFile(const std::string& utf8Path, const std::string& data);

    // 配置项（带默认值）
    static int defaultScanStart();
    static int defaultScanEnd();
    static int historyLimit();
    static int watchIntervalMs();

    // 数据目录 %LOCALAPPDATA%\PortLens
    static std::wstring getDataDirW();
    static std::string getDataDir();

    // 工具: UTF-8 与宽字符互转
    static std::string toUtf8(const std::wstring& s);
    static std::wstring toWide(const std::string& s);

private:
    static bool s_loaded;
    static std::map<std::string, int> s_aliases;
    static std::map<int, std::string> s_descs;
    static int s_scanStart;
    static int s_scanEnd;
    static int s_historyLimit;
    static int s_watchInterval;
    static std::string s_configPath;
};

// 结束进程历史记录
class HistoryManager {
public:
    static std::wstring getHistoryPathW();
    static std::string getHistoryPath();

    // 记录一次结束操作（自动裁剪到保留条数）
    static void record(uint32_t pid, const std::string& procName,
                       const std::string& path, const std::string& ports);

    // 按时间倒序返回；limit <= 0 表示使用配置中的保留条数
    static std::vector<KillRecord> list(int limit = 0);

    static int count();
    static bool clear();
};

// 端口收藏夹
class FavoritesManager {
public:
    static std::wstring getFavoritesPathW();
    static std::string getFavoritesPath();

    static bool add(int port);
    static bool remove(int port);
    static bool contains(int port);
    static std::vector<int> list();
};
