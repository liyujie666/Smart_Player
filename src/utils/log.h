#pragma once

#include <QString>
#include <fstream>
#include <mutex>
#include <cstdarg>

// 日志级别枚举
enum class LogLevel : int {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
};

// 单例日志类
class Log {
private:
    static Log* m_instance;
    LogLevel m_curLevel;
    std::ofstream m_file;
    bool m_isFileOpen;
    std::mutex m_mutex; // 线程安全锁（多线程打印必备）

    Log();
    ~Log();

    QString getTime();
    QString level2Str(LogLevel level);
    QString getFileName(const char* filePath);

public:
    static Log* getInstance();
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;

    void setLevel(LogLevel level);
    void setFile(const std::string& path);

    // 核心打印函数
    void print(LogLevel level, const char* file, int line, const char* fmt, ...);
};

// 宏定义（完全兼容原有调用方式）
#define LOG_DEBUG(fmt, ...)  Log::getInstance()->print(LogLevel::LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)   Log::getInstance()->print(LogLevel::LOG_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)   Log::getInstance()->print(LogLevel::LOG_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)  Log::getInstance()->print(LogLevel::LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
