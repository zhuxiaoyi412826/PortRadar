#pragma once
#include <string>
#include <cstdint>
#include "port_checker.h"

// 进程操作类
class ProcessManager {
public:
    // 根据 PID 获取进程名
    static std::string getProcessName(uint32_t pid);

    // 根据 PID 获取进程完整路径
    static std::string getProcessPath(uint32_t pid);

    // 获取进程详细信息
    static ProcessDetail getProcessDetail(uint32_t pid);

    // 杀死进程
    static bool killProcess(uint32_t pid);

    // 获取文件版本信息（公司名、描述等）
    static std::string getFileCompany(const std::string& path);
    static std::string getFileDescription(const std::string& path);

private:
    static std::string getFileVersionInfo(const std::string& path, const std::wstring& field);
};
