#include "logger.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>

void Logger::init(LogLevel level, bool async, const std::string& filename, bool console_output) {
    current_level = level;
    async_mode = async;
    console_output = console_output;
    
    if (!filename.empty()) {
        log_file.open(filename, std::ios::app);
    }
    
    if (async_mode) {
        running = true;
        worker_thread = std::thread(&Logger::workerThread, this);
    }
}

Logger::~Logger() {
    shutdown();
}

void Logger::trace(const std::string& message) { log(LogLevel::TRACE, message); }
void Logger::debug(const std::string& message) { log(LogLevel::DEBUG, message); }
void Logger::info(const std::string& message) { log(LogLevel::INFO, message); }
void Logger::warn(const std::string& message) { log(LogLevel::WARN, message); }
void Logger::error(const std::string& message) { log(LogLevel::ERROR, message); }
void Logger::fatal(const std::string& message) { log(LogLevel::FATAL, message); }

void Logger::shutdown() {
    running = false;
    if (queue_cv.notify_all(), worker_thread.joinable()) {
        worker_thread.join();
    }
    if (log_file.is_open()) {
        log_file.close();
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < current_level) return;
    
    if (async_mode) {
        LogMessage logMsg{level, message, std::chrono::system_clock::now(), std::this_thread::get_id()};
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            message_queue.push(logMsg);
        }
        queue_cv.notify_one();
    } else {
        // Synchronous logging
        LogMessage logMsg{level, message, std::chrono::system_clock::now(), std::this_thread::get_id()};
        std::string formatted = formatMessage(logMsg);
        
        if (console_output) {
            std::cout << formatted << std::endl;
        }
        
        if (log_file.is_open()) {
            std::lock_guard<std::mutex> lock(file_mutex);
            log_file << formatted << std::endl;
            log_file.flush();
        }
    }
}

void Logger::workerThread() {
    while (running) {
        LogMessage logMsg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [this] { return !message_queue.empty() || !running; });
            
            if (!running && message_queue.empty()) break;
            
            if (!message_queue.empty()) {
                logMsg = message_queue.front();
                message_queue.pop();
            }
        }
        
        std::string formatted = formatMessage(logMsg);
        
        if (console_output) {
            std::cout << formatted << std::endl;
        }
        
        if (log_file.is_open()) {
            std::lock_guard<std::mutex> lock(file_mutex);
            log_file << formatted << std::endl;
            log_file.flush();
        }
    }
}

std::string Logger::formatMessage(const LogMessage& msg) {
    std::stringstream ss;
    ss << timestampToString(msg.timestamp);
    ss << " [" << levelToString(msg.level) << "] ";
    ss << msg.message;
    return ss.str();
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string Logger::timestampToString(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
} 