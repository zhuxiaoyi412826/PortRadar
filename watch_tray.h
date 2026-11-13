#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <cstdint>

// 定向监控单个端口: 状态变化时响铃提醒（可选弹窗），Ctrl+C 退出
// intervalMs: 轮询间隔毫秒
int runPortWatch(uint16_t port, int intervalMs, bool popup);

// 系统托盘常驻: 监控多个端口（为空时由调用方填入收藏夹），状态变化时气泡提醒
int runTray(const std::vector<uint16_t>& ports, int intervalMs);

// 异步弹窗通知（不阻塞调用方）
void showDesktopNotification(const std::wstring& title, const std::wstring& msg);

// 响铃提醒
void beepAlert(bool occupied);
