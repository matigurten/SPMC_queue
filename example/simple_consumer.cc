#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>
#include "structs.h"
#include "snapshot_shm.h"

std::atomic<bool> running{true};

void signal_handler(int sig) {
    running = false;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <tob_shm_name> <fod_shm_name> [consumer_id]" << std::endl;
        std::cout << "Example: " << argv[0] << " /tob_snapshots /fod_snapshots consumer1" << std::endl;
        return 1;
    }
    
    const char* tob_name = argv[1];
    const char* fod_name = argv[2];
    const char* consumer_id = (argc > 3) ? argv[3] : "default";
    
    // Set up signal handler for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "Starting simple consumer [" << consumer_id << "]..." << std::endl;
    std::cout << "TOB shared memory: " << tob_name << std::endl;
    std::cout << "FOD shared memory: " << fod_name << std::endl;
    
    // Create consumers
    TOBConsumer tob_consumer(tob_name);
    FODConsumer fod_consumer(fod_name);
    
    if (!tob_consumer.is_valid() && !fod_consumer.is_valid()) {
        std::cerr << "Failed to connect to both TOB and FOD shared memory" << std::endl;
        std::cerr << "Make sure the reader is running and has created the shared memory files." << std::endl;
        return 1;
    }
    
    std::cout << "Successfully connected to shared memory" << std::endl;
    
    uint64_t last_tob_seq = 0;
    uint64_t last_fod_seq = 0;
    uint64_t tob_count = 0;
    uint64_t fod_count = 0;
    
    std::cout << "Waiting for snapshots..." << std::endl;
    
    while (running) {
        // Check for new TOB snapshots
        if (tob_consumer.has_new_snapshot(last_tob_seq)) {
            tob_count++;
            std::cout << "[" << consumer_id << "] TOB #" << tob_count 
                      << " [seq=" << last_tob_seq << "]" << std::endl;
        }
        
        // Check for new FOD snapshots
        if (fod_consumer.has_new_snapshot(last_fod_seq)) {
            fod_count++;
            std::cout << "[" << consumer_id << "] FOD #" << fod_count 
                      << " [seq=" << last_fod_seq << "]" << std::endl;
        }
        
        // Small sleep to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    std::cout << "[" << consumer_id << "] Shutting down..." << std::endl;
    std::cout << "[" << consumer_id << "] Final stats - TOB: " << tob_count << ", FOD: " << fod_count << std::endl;
    return 0;
} 