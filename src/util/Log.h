// About: Lightweight logging with severity levels and formatted output.

#pragma once

#include <cstdio>
#include <cstdarg>

namespace luna::util {

enum class LogLevel { DEBUG, INFO, WARN, ERROR };

class Log {
public:
    static void init();
    static void log(LogLevel level, const char* file, int line, const char* fmt, ...);
};

} // namespace luna::util

#define LOG_DEBUG(...) ::luna::util::Log::log(::luna::util::LogLevel::DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  ::luna::util::Log::log(::luna::util::LogLevel::INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  ::luna::util::Log::log(::luna::util::LogLevel::WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) ::luna::util::Log::log(::luna::util::LogLevel::ERROR, __FILE__, __LINE__, __VA_ARGS__)
