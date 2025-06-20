#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>
#include "snapshot_shm.h"
#include "shm.h"

volatile bool running = true;

void signal_handler(int sig) {
    running = false;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <snapshot_shm_name> [consumer_id]" << std::endl;
        return 1;
    }
    
    const char* snapshot_name = argv[1];
    std::string consumer_id = (argc > 2) ? argv[2] : "simple";
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "[" << consumer_id << "] Starting simple consumer" << std::endl;
    std::cout << "[" << consumer_id << "] Connecting to: " << snapshot_name << std::endl;
    
    SnapshotConsumer consumer(snapshot_name);
    if (!consumer.is_valid()) {
        std::cerr << "[" << consumer_id << "] Failed to connect" << std::endl;
        return 1;
    }
    
    std::cout << "[" << consumer_id << "] Connected successfully" << std::endl;
    
    uint64_t last_tob_seq = 0;
    uint64_t last_fod_seq = 0;
    uint64_t tob_count = 0;
    uint64_t fod_count = 0;
    
    while (running) {
        if (consumer.has_new_tob(last_tob_seq)) {
            tob_count++;
            std::cout << "[" << consumer_id << "] TOB #" << tob_count 
                      << " (seq=" << last_tob_seq << ")" << std::endl;
            consumer.update_read_timestamp();
        }
        
        if (consumer.has_new_fod(last_fod_seq)) {
            fod_count++;
            std::cout << "[" << consumer_id << "] FOD #" << fod_count 
                      << " (seq=" << last_fod_seq << ")" << std::endl;
            consumer.update_read_timestamp();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    std::cout << "[" << consumer_id << "] Shutting down. Received " 
              << tob_count << " TOB and " << fod_count << " FOD snapshots" << std::endl;
    return 0;
} 