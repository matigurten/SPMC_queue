#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>
#include <iomanip>
#include <ctime>
#include "structs.h"
#include "snapshot_shm.h"
#include "logger.h"

std::atomic<bool> running{true};

void signal_handler(int sig) {
    running = false;
    Logger::getInstance().shutdown();
}

// Use the same clock as the rest of the system
inline uint64_t get_ns_since_epoch() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL + ts.tv_nsec;
}

void print_tob_snapshot(const SnapshotTOB& tob, uint64_t read_time) {
    uint64_t latency = read_time > tob.arrival_time ? read_time - tob.arrival_time : 0;
    
    LOG_INFO_STREAM() << "TOB [seq=" << tob.seq
                      << ", bid=" << tob.bid_amount << "@" << tob.bid_price << " #" << tob.bid_orders
                      << ", ask=" << tob.ask_amount << "@" << tob.ask_price << " #" << tob.ask_orders
                      << ", last=" << tob.last_price
                      << ", volume=" << tob.volume
                      << ", agg=" << (int)tob.last_aggression
                      << ", phase=" << (int)tob.trading_phase
                      << ", level_changed=" << (int)tob.level_changed
                      << ", side_changed=" << (int)tob.side_changed
                      << ", latency_to_consumer=" << latency << " ns"
                      << "]";
}

void print_fod_snapshot(const SnapshotFOD& fod, int max_levels, uint64_t read_time) {
    uint64_t latency = read_time > fod.publish_time ? read_time - fod.publish_time : 0;
    
    LOG_INFO_STREAM() << "FOD [seq=" << fod.seq
                      << ", last=" << fod.last_price
                      << ", volume=" << fod.volume
                      << ", agg=" << (int)fod.last_aggression
                      << ", phase=" << (int)fod.trading_phase
                      << ", level_changed=" << (int)fod.level_changed
                      << ", side_changed=" << (int)fod.side_changed
                      << ", latency_to_consumer=" << latency << " ns"
                      << "]";
    
    // Log bid levels
    std::ostringstream bids_oss;
    bids_oss << "  Bids: ";
    int printed = 0;
    for (int i = 0; i < 10 && printed < max_levels; i++) {
        if (fod.bids[i].amount > 0) {
            bids_oss << std::setw(8) << fod.bids[i].amount << "@"
                     << std::setw(8) << fod.bids[i].price << " #"
                     << std::setw(3) << fod.bids[i].orders << "\t";
            printed++;
        }
    }
    LOG_INFO(bids_oss.str());
    
    // Log ask levels
    std::ostringstream asks_oss;
    asks_oss << "  Asks: ";
    printed = 0;
    for (int i = 0; i < 10 && printed < max_levels; i++) {
        if (fod.asks[i].amount > 0) {
            asks_oss << std::setw(8) << fod.asks[i].amount << "@"
                     << std::setw(8) << fod.asks[i].price << " #"
                     << std::setw(3) << fod.asks[i].orders << "\t";
            printed++;
        }
    }
    LOG_INFO(asks_oss.str());
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <type> <shm_name> [consumer_id] [levels] [--log-level=LEVEL] [--log-file=FILE]" << std::endl;
        std::cout << "  type: 'tob' or 'fod'" << std::endl;
        std::cout << "Example: " << argv[0] << " tob /tob_snapshots consumer1 --log-level=INFO" << std::endl;
        std::cout << "Example: " << argv[0] << " fod /fod_snapshots consumer1 4 --log-level=INFO" << std::endl;
        std::cout << "Log levels: TRACE, DEBUG, INFO, WARN, ERROR, FATAL" << std::endl;
        return 1;
    }
    
    std::string type = argv[1];
    const char* shm_name = argv[2];
    std::string consumer_id = (argc > 3) ? argv[3] : "consumer";
    int max_levels = 4; // Default for TOB, can be overridden for FOD
    
    // Adjust argument parsing based on type
    int arg_offset = 4;
    if (type == "fod" && argc > 4) {
        max_levels = std::atoi(argv[4]);
        if (max_levels < 1 || max_levels > 10) max_levels = 4;
        arg_offset = 5;
    }
    
    // Parse logger options
    LogLevel log_level = LogLevel::INFO;
    std::string log_file = "";
    bool console_output = true;
    
    for (int i = arg_offset; i < argc; i++) {
        std::string arg = argv[i];
        if (arg.substr(0, 12) == "--log-level=") {
            std::string level_str = arg.substr(12);
            if (level_str == "TRACE") log_level = LogLevel::TRACE;
            else if (level_str == "DEBUG") log_level = LogLevel::DEBUG;
            else if (level_str == "INFO") log_level = LogLevel::INFO;
            else if (level_str == "WARN") log_level = LogLevel::WARN;
            else if (level_str == "ERROR") log_level = LogLevel::ERROR;
            else if (level_str == "FATAL") log_level = LogLevel::FATAL;
        } else if (arg.substr(0, 10) == "--log-file=") {
            log_file = arg.substr(10);
        } else if (arg == "--no-console") {
            console_output = false;
        }
    }
    
    // Initialize logger
    Logger::getInstance().init(log_level, true, log_file, console_output);
    
    // Set up signal handler for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    LOG_INFO_STREAM() << "Starting " << type << " consumer: " << consumer_id;
    LOG_INFO_STREAM() << "Connecting to " << type << " shared memory: " << shm_name;
    
    uint64_t last_seq = 0;
    
    if (type == "tob") {
        // TOB Consumer
        TOBConsumer consumer(shm_name);
        if (!consumer.is_valid()) {
            LOG_ERROR_STREAM() << "Failed to connect to TOB shared memory: " << shm_name;
            LOG_ERROR("Make sure the reader is running and has created the TOB shared memory.");
            return 1;
        }
        
        LOG_INFO("Successfully connected to TOB shared memory");
        LOG_INFO("Waiting for TOB snapshots...");
        
        while (running) {
            if (consumer.has_new_snapshot(last_seq)) {
                uint64_t read_time = get_ns_since_epoch();
                const SnapshotTOB* tob = consumer.get_latest_snapshot();
                if (tob) {
                    print_tob_snapshot(*tob, read_time);
                }
            }
            // Busy waiting - no sleep for minimal latency
        }
        
    } else if (type == "fod") {
        // FOD Consumer
        FODConsumer consumer(shm_name);
        if (!consumer.is_valid()) {
            LOG_ERROR_STREAM() << "Failed to connect to FOD shared memory: " << shm_name;
            LOG_ERROR("Make sure the reader is running and has created the FOD shared memory.");
            return 1;
        }
        
        LOG_INFO("Successfully connected to FOD shared memory");
        LOG_INFO_STREAM() << "Waiting for FOD snapshots (max " << max_levels << " levels)...";
        
        while (running) {
            if (consumer.has_new_snapshot(last_seq)) {
                uint64_t read_time = get_ns_since_epoch();
                const SnapshotFOD* fod = consumer.get_latest_snapshot();
                if (fod) {
                    print_fod_snapshot(*fod, max_levels, read_time);
                }
            }
            // Busy waiting - no sleep for minimal latency
        }
        
    } else {
        LOG_ERROR_STREAM() << "Invalid type: " << type << ". Use 'tob' or 'fod'.";
        return 1;
    }
    
    LOG_INFO_STREAM() << "Shutting down " << type << " consumer: " << consumer_id;
    return 0;
} 