#include "log.h"
#include <QDebug>
#include <QDateTime>
#include <cstdio>
#include <QString>

Log::Log() : m_curLevel(LogLevel::LOG_DEBUG), m_isFileOpen(false) {}

Log::~Log() {
    if (m_file.is_open()) {
        m_file.close();
    }
}

Log& Log::getInstance() {
    static Log instance;
    return instance;
}

void Log::setLevel(LogLevel level) {
    m_curLevel = level;
}

void Log::setFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file.is_open()) {
        m_file.close();
    }
    m_file.open(path, std::ios::out | std::ios::app);
    m_isFileOpen = m_file.is_open();
}

// Qt 格式化时间
QString Log::getTime() {
    return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
}

QString Log::level2Str(LogLevel level) {
    switch (level) {
    case LogLevel::LOG_DEBUG: return "DEBUG";
    case LogLevel::LOG_INFO:  return "INFO";
    case LogLevel::LOG_WARN:  return "WARN";
    case LogLevel::LOG_ERROR: return "ERROR";
    default: return "UNKNOWN";
    }
}

QString Log::getFileName(const char* filePath) {
    QString path = QString::fromLocal8Bit(filePath);
    // 兼容 Windows(\) 和 Linux(/) 路径，Qt5/Qt6 通用
    qsizetype pos1 = path.lastIndexOf('/');
    qsizetype pos2 = path.lastIndexOf('\\');
    qsizetype pos = qMax(pos1, pos2);

    return (pos == -1) ? path : path.mid(pos + 1);
}

void Log::print(LogLevel level, const char* file, int line, const char* fmt, ...) {
    if (static_cast<int>(level) < static_cast<int>(m_curLevel)) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    char msg[1024] = {0};
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    QString log = QString("[%1] [%2] [%3:%4] %5")
                      .arg(getTime())
                      .arg(level2Str(level))
                      .arg(getFileName(file))
                      .arg(line)
                      .arg(msg);

    switch (level) {
    case LogLevel::LOG_DEBUG:
        qDebug() << "\033[37m" << log << "\033[0m";
        break;
    case LogLevel::LOG_INFO:
        qInfo() << "\033[32m" << log << "\033[0m";
        break;
    case LogLevel::LOG_WARN:
        qWarning() << "\033[33m" << log << "\033[0m";
        break;
    case LogLevel::LOG_ERROR:
        qCritical() << "\033[31m" << log << "\033[0m";
        break;
    default:
        break;
    }

    if (m_isFileOpen) {
        m_file << log.toLocal8Bit().data() << std::endl;
        m_file.flush();
    }
}
