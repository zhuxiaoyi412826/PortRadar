#pragma once
#include <string>

// 识别端口归属的特殊环境（Docker 容器 / WSL / Hyper-V 虚拟机）
// 这些场景下端口表里只能看到代理进程，标注后便于判断真实来源
class SpecialProcessDetector {
public:
    // 返回标注标签: "[Docker: 容器名]" / "[Docker]" / "[WSL]" / "[Hyper-V]" / ""
    static std::string detect(const std::string& procName, int port);

    static bool isDockerProcess(const std::string& procName);
    static bool isWslProcess(const std::string& procName);
    static bool isVmProcess(const std::string& procName);

    // 清空 Docker 端口映射缓存（下次查询重新执行 docker ps）
    static void resetCache();

private:
    static void ensureDockerMap();
};
