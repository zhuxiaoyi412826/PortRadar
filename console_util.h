#pragma once
#include <windows.h>
#include <iostream>
#include <string>

// 控制台颜色
enum Color {
    BLACK = 0,
    DARK_BLUE = 1,
    DARK_GREEN = 2,
    DARK_CYAN = 3,
    DARK_RED = 4,
    DARK_MAGENTA = 5,
    DARK_YELLOW = 6,
    GRAY = 7,
    DARK_GRAY = 8,
    BLUE = 9,
    GREEN = 10,
    CYAN = 11,
    RED = 12,
    MAGENTA = 13,
    YELLOW = 14,
    WHITE = 15
};

inline WORD g_defaultAttr = 0;
inline bool g_initialized = false;

inline HANDLE getConsole() {
    return GetStdHandle(STD_OUTPUT_HANDLE);
}

inline void saveColor() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(getConsole(), &csbi)) {
        g_defaultAttr = csbi.wAttributes;
        g_initialized = true;
    } else {
        g_defaultAttr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // 灰色
        g_initialized = true;
    }
}

inline void restoreColor() {
    if (g_initialized) {
        SetConsoleTextAttribute(getConsole(), g_defaultAttr);
    }
}

inline void setColor(Color fg, Color bg = BLACK) {
    SetConsoleTextAttribute(getConsole(), fg | (bg << 4));
}

// 清屏（用 API 代替 system("cls")）
inline void clearScreen() {
    HANDLE hConsole = getConsole();
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return;
    
    DWORD size = csbi.dwSize.X * csbi.dwSize.Y;
    COORD coord = {0, 0};
    DWORD count;
    
    // 填充空格
    FillConsoleOutputCharacter(hConsole, ' ', size, coord, &count);
    // 填充属性
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, size, coord, &count);
    // 移动光标到左上角
    SetConsoleCursorPosition(hConsole, coord);
}

// 彩色输出
inline void printColor(const std::string& text, Color color) {
    setColor(color);
    std::cout << text;
    restoreColor();
}

inline void printlnColor(const std::string& text, Color color) {
    printColor(text, color);
    std::cout << std::endl;
}

// 打印标题
inline void printTitle(const std::string& title) {
    setColor(YELLOW);
    std::cout << "\n+-------------------------------------------------------------+\n";
    std::cout << "|  " << title;
    int padding = 58 - (int)title.length() * 2; // 中文占2位
    if (padding < 0) padding = 2;
    for (int i = 0; i < padding; i++) std::cout << " ";
    std::cout << "|\n";
    std::cout << "+-------------------------------------------------------------+\n";
    restoreColor();
}

// 打印分隔线
inline void printSeparator() {
    setColor(DARK_GRAY);
    std::cout << " -------------------------------------------------------------\n";
    restoreColor();
}

// 成功/错误/警告 标签
inline void printSuccess(const std::string& msg) {
    setColor(GREEN);
    std::cout << "[成功] ";
    restoreColor();
    std::cout << msg << std::endl;
}

inline void printError(const std::string& msg) {
    setColor(RED);
    std::cout << "[错误] ";
    restoreColor();
    std::cout << msg << std::endl;
}

inline void printWarning(const std::string& msg) {
    setColor(YELLOW);
    std::cout << "[注意] ";
    restoreColor();
    std::cout << msg << std::endl;
}

inline void printInfo(const std::string& msg) {
    setColor(CYAN);
    std::cout << "[信息] ";
    restoreColor();
    std::cout << msg << std::endl;
}

// 高亮显示关键字
inline void highlight(const std::string& text) {
    setColor(YELLOW);
    std::cout << text;
    restoreColor();
}
