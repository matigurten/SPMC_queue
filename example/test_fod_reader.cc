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
    std::cout << "Received signal " << sig << ", shutting down..." << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <fod_shm_name>" << std::endl;
        std::cout << "Example: " << argv[0] << " /fod_snapshots" << std::endl;
        return 1;
    }
    
    const char* shm_name = argv[1];
    
    // Set up signal handler for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "Test FOD Reader starting..." << std::endl;
    std::cout << "Connecting to FOD shared memory: " << shm_name << std::endl;
    
    // Try to open the shared memory directly first
    std::cout << "Attempting to open FOD shared memory directly..." << std::endl;
    FODSHM* fod_shm = open_fod_shm(shm_name);
    
    if (!fod_shm) {
        std::cerr << "Failed to open FOD shared memory directly: " << shm_name << std::endl;
        std::cerr << "Make sure the reader is running and has created the FOD shared memory." << std::endl;
        return 1;
    }
    
    std::cout << "Successfully opened FOD shared memory directly!" << std::endl;
    std::cout << "Active consumers: " << fod_shm->active_consumers.load() << std::endl;
    std::cout << "Last sequence: " << fod_shm->last_seq.load() << std::endl;
    
    // Now try the consumer class
    std::cout << "\nAttempting to create FODConsumer..." << std::endl;
    FODConsumer consumer(shm_name);
    
    if (!consumer.is_valid()) {
        std::cerr << "Failed to create FODConsumer for: " << shm_name << std::endl;
        std::cerr << "This is likely the source of the segmentation fault." << std::endl;
        return 1;
    }
    
    std::cout << "Successfully created FODConsumer!" << std::endl;
    
    uint64_t last_seq = 0;
    uint64_t snapshot_count = 0;
    
    std::cout << "Starting to read FOD snapshots..." << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;
    
    while (running) {
        if (consumer.has_new_snapshot(last_seq)) {
            snapshot_count++;
            const SnapshotFOD* snapshot = consumer.get_latest_snapshot();
            
            if (snapshot) {
                std::cout << "FOD #" << snapshot_count 
                          << " [seq=" << snapshot->seq 
                          << ", inst=" << snapshot->instrument_id
                          << ", last_price=" << snapshot->last_price
                          << ", volume=" << snapshot->volume
                          << ", phase=" << (int)snapshot->trading_phase
                          << ", bid0=" << snapshot->bids[0].price << "@" << snapshot->bids[0].amount
                          << ", ask0=" << snapshot->asks[0].price << "@" << snapshot->asks[0].amount
                          << "]" << std::endl;
            } else {
                std::cout << "FOD #" << snapshot_count << " [seq=" << last_seq << "] - NULL snapshot!" << std::endl;
            }
        }
        
        // Small sleep to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "Test FOD Reader stopped. Total snapshots read: " << snapshot_count << std::endl;
    return 0;
} 