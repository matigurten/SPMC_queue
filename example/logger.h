#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <sstream>
#include <chrono>
#include <iomanip>

enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    FATAL = 5
};

class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    // Initialize logger with configuration
    void init(LogLevel level = LogLevel::INFO, 
              bool async = true, 
              const std::string& filename = "",
              bool console_output = true);

    // Logging methods
    void trace(const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);
    void fatal(const std::string& message);

    // Shutdown logger
    void shutdown();

    // Set log level at runtime
    void setLevel(LogLevel level) { current_level = level; }

    // Public log method for LogStream
    void log(LogLevel level, const std::string& message);

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    struct LogMessage {
        LogLevel level;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
        std::thread::id thread_id;
    };

    void workerThread();
    std::string formatMessage(const LogMessage& msg);
    std::string levelToString(LogLevel level);
    std::string timestampToString(const std::chrono::system_clock::time_point& tp);

    std::atomic<LogLevel> current_level{LogLevel::INFO};
    std::atomic<bool> running{false};
    std::atomic<bool> async_mode{true};
    std::atomic<bool> console_output{true};
    
    std::queue<LogMessage> message_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::thread worker_thread;
    
    std::ofstream log_file;
    std::mutex file_mutex;
};

// Convenience macros for logging
#define LOG_TRACE(msg) Logger::getInstance().trace(msg)
#define LOG_DEBUG(msg) Logger::getInstance().debug(msg)
#define LOG_INFO(msg)  Logger::getInstance().info(msg)
#define LOG_WARN(msg)  Logger::getInstance().warn(msg)
#define LOG_ERROR(msg) Logger::getInstance().error(msg)
#define LOG_FATAL(msg) Logger::getInstance().fatal(msg)

// Macro for streaming (like log4j)
#define LOG_STREAM(level) LogStream(level)
#define LOG_TRACE_STREAM() LOG_STREAM(LogLevel::TRACE)
#define LOG_DEBUG_STREAM() LOG_STREAM(LogLevel::DEBUG)
#define LOG_INFO_STREAM()  LOG_STREAM(LogLevel::INFO)
#define LOG_WARN_STREAM()  LOG_STREAM(LogLevel::WARN)
#define LOG_ERROR_STREAM() LOG_STREAM(LogLevel::ERROR)
#define LOG_FATAL_STREAM() LOG_STREAM(LogLevel::FATAL)

// Streaming logger class
class LogStream {
public:
    LogStream(LogLevel level) : level_(level) {}
    ~LogStream() {
        Logger::getInstance().log(level_, stream_.str());
    }

    template<typename T>
    LogStream& operator<<(const T& value) {
        stream_ << value;
        return *this;
    }

private:
    LogLevel level_;
    std::ostringstream stream_;
}; 