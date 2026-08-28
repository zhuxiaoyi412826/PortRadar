#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

// 端口信息
struct PortInfo {
    uint16_t port;
    uint32_t pid;
    std::string protocol;  // TCP / UDP
    std::string state;     // LISTENING / ESTABLISHED 等
    std::string localAddr;
    std::string remoteAddr;
    uint16_t remotePort;
};

// 进程详情
struct ProcessDetail {
    uint32_t pid;
    std::string name;
    std::string path;
    uint64_t memoryKb;     // 工作集内存(KB)
    uint64_t peakMemoryKb; // 峰值内存(KB)
    uint32_t threadCount;
    uint32_t parentPid;
    std::string startTime; // 启动时间
    std::string company;   // 公司名
    std::string description; // 文件描述
};

// 端口检测类
class PortChecker {
public:
    static bool init();
    static void cleanup();

    // 获取所有监听端口
    static std::vector<PortInfo> getAllListeningPorts(const std::string& protocol = "ALL");

    // 检查单个端口
    static PortInfo checkPort(uint16_t port, const std::string& protocol = "TCP");

    // 获取指定端口的所有连接
    static std::vector<PortInfo> getPortConnections(uint16_t port);

    // 端口扫描
    static std::vector<PortInfo> scanPorts(uint16_t startPort, uint16_t endPort);

    // 端口状态转字符串
    static std::string stateToString(DWORD state);

private:
    static std::string ipToString(DWORD ip);
};
