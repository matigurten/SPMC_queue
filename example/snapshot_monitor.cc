#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>
#include <iomanip>
#include "snapshot_shm.h"
#include "latency_stats.h"
#include "shm.h"

std::atomic<bool> running{true};

void signal_handler(int sig) {
    running = false;
}

void print_stats(const SnapshotSHM* shm, uint64_t last_tob_seq, uint64_t last_fod_seq, 
                uint64_t last_consumer_read, uint64_t start_time) {
    uint64_t current_time = get_ns_since_epoch();
    uint64_t uptime_ns = current_time - start_time;
    double uptime_sec = uptime_ns / 1e9;
    
    std::cout << "\n=== Snapshot Monitor Stats ===" << std::endl;
    std::cout << "Uptime: " << std::fixed << std::setprecision(1) << uptime_sec << "s" << std::endl;
    std::cout << "Active consumers: " << shm->active_consumers.load() << std::endl;
    std::cout << "Last TOB sequence: " << shm->last_tob_seq.load() << std::endl;
    std::cout << "Last FOD sequence: " << shm->last_fod_seq.load() << std::endl;
    std::cout << "Last consumer read: " << shm->last_consumer_read.load() << std::endl;
    std::cout << "=============================" << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <snapshot_shm_name>" << std::endl;
        std::cout << "Example: " << argv[0] << " /snapshots" << std::endl;
        return 1;
    }
    
    const char* snapshot_name = argv[1];
    
    // Set up signal handler for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "Starting snapshot monitor" << std::endl;
    std::cout << "Connecting to snapshot shared memory: " << snapshot_name << std::endl;
    
    // Open shared memory directly for monitoring
    SnapshotSHM* shm = open_snapshot_shm(snapshot_name);
    if (!shm) {
        std::cerr << "Failed to connect to snapshot shared memory: " << snapshot_name << std::endl;
        return 1;
    }
    
    // Create consumer for sequence tracking
    SnapshotConsumer consumer(snapshot_name);
    if (!consumer.is_valid()) {
        std::cerr << "Failed to create consumer" << std::endl;
        return 1;
    }
    
    std::cout << "Successfully connected to snapshot shared memory" << std::endl;
    
    uint64_t last_tob_seq = 0;
    uint64_t last_fod_seq = 0;
    uint64_t start_time = get_ns_since_epoch();
    
    std::cout << "Starting monitoring..." << std::endl;
    
    while (running) {
        // Update our sequence numbers
        consumer.has_new_tob(last_tob_seq);
        consumer.has_new_fod(last_fod_seq);
        
        // Print stats every second
        print_stats(shm, last_tob_seq, last_fod_seq, 0, start_time);
        
        // Sleep for 1 second
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "\nShutting down monitor" << std::endl;
    return 0;
} 