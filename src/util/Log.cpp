// About: Log implementation — writes formatted messages to stderr.

#include "util/Log.h"
#include <cstdio>
#include <cstdarg>

namespace luna::util {

void Log::init() {
    // Placeholder — future: configure log level, output targets
}

void Log::log(LogLevel level, const char* file, int line, const char* fmt, ...) {
    const char* levelStr = "?";
    switch (level) {
        case LogLevel::DEBUG: levelStr = "DEBUG"; break;
        case LogLevel::INFO:  levelStr = "INFO";  break;
        case LogLevel::WARN:  levelStr = "WARN";  break;
        case LogLevel::ERROR: levelStr = "ERROR"; break;
    }

    std::fprintf(stderr, "[%s] ", levelStr);

    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);

    std::fprintf(stderr, "\n");
}

} // namespace luna::util
