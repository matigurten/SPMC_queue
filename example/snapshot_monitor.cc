#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>
#include <iomanip>
#include "snapshot_shm.h"

volatile bool running = true;

void signal_handler(int sig) {
    running = false;
}

void print_stats(const SnapshotSHM* shm, uint64_t last_tob_seq, uint64_t last_fod_seq, 
                 uint64_t last_write_seq, uint64_t start_time) {
    if (!shm) return;
    
    uint64_t current_time = get_ns_since_epoch();
    uint64_t uptime_ns = current_time - start_time;
    double uptime_sec = uptime_ns / 1e9;
    
    std::cout << "\033[2J\033[H"; // Clear screen and move cursor to top
    std::cout << "=== SNAPSHOT MONITOR ===" << std::endl;
    std::cout << "Uptime: " << std::fixed << std::setprecision(1) << uptime_sec << "s" << std::endl;
    std::cout << "Current Time: " << current_time << std::endl;
    std::cout << std::endl;
    
    // Producer stats
    std::cout << "--- PRODUCER STATS ---" << std::endl;
    std::cout << "Total Writes: " << shm->write_seq.load() << std::endl;
    std::cout << "TOB Updates: " << shm->last_tob_seq.load() << std::endl;
    std::cout << "FOD Updates: " << shm->last_fod_seq.load() << std::endl;
    std::cout << "Last Write: " << shm->write_timestamp.load() << std::endl;
    
    // Consumer stats
    std::cout << "\n--- CONSUMER STATS ---" << std::endl;
    std::cout << "Active Consumers: " << shm->active_consumers.load() << std::endl;
    std::cout << "Last Consumer Read: " << shm->last_consumer_read.load() << std::endl;
    
    // This consumer's stats
    std::cout << "\n--- THIS CONSUMER ---" << std::endl;
    std::cout << "Last TOB Seq Seen: " << last_tob_seq << std::endl;
    std::cout << "Last FOD Seq Seen: " << last_fod_seq << std::endl;
    std::cout << "Last Write Seq Seen: " << last_write_seq << std::endl;
    
    // Calculate rates
    if (uptime_sec > 0) {
        double tob_rate = shm->last_tob_seq.load() / uptime_sec;
        double fod_rate = shm->last_fod_seq.load() / uptime_sec;
        double write_rate = shm->write_seq.load() / uptime_sec;
        
        std::cout << "\n--- RATES (per second) ---" << std::endl;
        std::cout << "TOB Updates: " << std::fixed << std::setprecision(2) << tob_rate << std::endl;
        std::cout << "FOD Updates: " << std::fixed << std::setprecision(2) << fod_rate << std::endl;
        std::cout << "Total Writes: " << std::fixed << std::setprecision(2) << write_rate << std::endl;
    }
    
    // Latency stats
    uint64_t last_write = shm->write_timestamp.load();
    uint64_t last_read = shm->last_consumer_read.load();
    if (last_write > 0 && last_read > 0) {
        uint64_t latency = last_read - last_write;
        std::cout << "\n--- LATENCY ---" << std::endl;
        std::cout << "Last Consumer Latency: " << latency << " ns" << std::endl;
    }
    
    std::cout << "\nPress Ctrl+C to exit" << std::endl;
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
    uint64_t last_write_seq = 0;
    uint64_t start_time = get_ns_since_epoch();
    
    std::cout << "Starting monitoring..." << std::endl;
    
    while (running) {
        // Update our sequence numbers
        consumer.has_new_tob(last_tob_seq);
        consumer.has_new_fod(last_fod_seq);
        
        // Print stats every second
        print_stats(shm, last_tob_seq, last_fod_seq, last_write_seq, start_time);
        
        // Sleep for 1 second
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "\nShutting down monitor" << std::endl;
    return 0;
} 