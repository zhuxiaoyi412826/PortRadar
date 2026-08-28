#include "process_manager.h"
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <sstream>
#include <iomanip>
#include <vector>

#pragma comment(lib, "psapi.lib")

std::string ProcessManager::getProcessName(uint32_t pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) return "未知进程";

    char name[MAX_PATH] = {0};
    HMODULE hMod;
    DWORD cbNeeded;
    if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
        GetModuleBaseNameA(hProcess, hMod, name, MAX_PATH);
    }
    CloseHandle(hProcess);
    return std::string(name);
}

std::string ProcessManager::getProcessPath(uint32_t pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) return "";

    char path[MAX_PATH] = {0};
    DWORD size = MAX_PATH;
    if (!QueryFullProcessImageNameA(hProcess, 0, path, &size)) {
        // 备用方法
        HMODULE hMod;
        DWORD cbNeeded;
        if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
            GetModuleFileNameExA(hProcess, hMod, path, MAX_PATH);
        }
    }
    CloseHandle(hProcess);
    return std::string(path);
}

ProcessDetail ProcessManager::getProcessDetail(uint32_t pid) {
    ProcessDetail detail;
    detail.pid = pid;
    detail.name = "未知进程";
    detail.path = "";
    detail.memoryKb = 0;
    detail.peakMemoryKb = 0;
    detail.threadCount = 0;
    detail.parentPid = 0;
    detail.startTime = "未知";
    detail.company = "";
    detail.description = "";

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) return detail;

    // 进程名
    char name[MAX_PATH] = {0};
    char path[MAX_PATH] = {0};
    HMODULE hMod;
    DWORD cbNeeded;
    if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
        GetModuleBaseNameA(hProcess, hMod, name, MAX_PATH);
        GetModuleFileNameExA(hProcess, hMod, path, MAX_PATH);
    }
    detail.name = name;
    detail.path = path;

    // 内存信息
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        detail.memoryKb = pmc.WorkingSetSize / 1024;
        detail.peakMemoryKb = pmc.PeakWorkingSetSize / 1024;
    }

    // 启动时间
    FILETIME creationTime, exitTime, kernelTime, userTime;
    if (GetProcessTimes(hProcess, &creationTime, &exitTime, &kernelTime, &userTime)) {
        SYSTEMTIME stUTC, stLocal;
        FileTimeToSystemTime(&creationTime, &stUTC);
        SystemTimeToTzSpecificLocalTime(nullptr, &stUTC, &stLocal);
        
        char buf[64];
        sprintf_s(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
            stLocal.wYear, stLocal.wMonth, stLocal.wDay,
            stLocal.wHour, stLocal.wMinute, stLocal.wSecond);
        detail.startTime = buf;
    }

    CloseHandle(hProcess);

    // 通过 CreateToolhelp32Snapshot 获取线程数和父进程ID
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);
        if (Process32First(hSnapshot, &pe32)) {
            do {
                if (pe32.th32ProcessID == pid) {
                    detail.parentPid = pe32.th32ParentProcessID;
                    break;
                }
            } while (Process32Next(hSnapshot, &pe32));
        }

        // 线程数
        THREADENTRY32 te32;
        te32.dwSize = sizeof(THREADENTRY32);
        DWORD threadCount = 0;
        if (Thread32First(hSnapshot, &te32)) {
            do {
                if (te32.th32OwnerProcessID == pid) {
                    threadCount++;
                }
            } while (Thread32Next(hSnapshot, &te32));
        }
        detail.threadCount = threadCount;

        CloseHandle(hSnapshot);
    }

    // 文件版本信息
    if (!detail.path.empty()) {
        detail.company = getFileCompany(detail.path);
        detail.description = getFileDescription(detail.path);
    }

    return detail;
}

bool ProcessManager::killProcess(uint32_t pid) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProcess) return false;
    
    BOOL result = TerminateProcess(hProcess, 0);
    CloseHandle(hProcess);
    return result == TRUE;
}

std::string ProcessManager::getFileVersionInfo(const std::string& path, const std::wstring& field) {
    // 转换为宽字符
    std::wstring wpath(path.begin(), path.end());
    
    DWORD dummy;
    DWORD size = GetFileVersionInfoSizeW(wpath.c_str(), &dummy);
    if (size == 0) return "";

    std::vector<BYTE> buffer(size);
    if (!GetFileVersionInfoW(wpath.c_str(), 0, size, buffer.data())) {
        return "";
    }

    // 获取翻译信息
    struct LANGANDCODEPAGE {
        WORD wLanguage;
        WORD wCodePage;
    } *lpTranslate;
    UINT cbTranslate;

    if (!VerQueryValueW(buffer.data(), L"\\VarFileInfo\\Translation",
            (LPVOID*)&lpTranslate, &cbTranslate)) {
        return "";
    }

    if (cbTranslate < sizeof(LANGANDCODEPAGE)) return "";

    // 构造查询字符串
    wchar_t subBlock[128];
    swprintf_s(subBlock, 128, L"\\StringFileInfo\\%04x%04x\\%s",
        lpTranslate[0].wLanguage, lpTranslate[0].wCodePage, field.c_str());

    LPWSTR value = nullptr;
    UINT valueLen = 0;
    if (VerQueryValueW(buffer.data(), subBlock, (LPVOID*)&value, &valueLen)) {
        if (value && valueLen > 0) {
            std::wstring wstr(value);
            return std::string(wstr.begin(), wstr.end());
        }
    }
    return "";
}

std::string ProcessManager::getFileCompany(const std::string& path) {
    return getFileVersionInfo(path, L"CompanyName");
}

std::string ProcessManager::getFileDescription(const std::string& path) {
    return getFileVersionInfo(path, L"FileDescription");
}
