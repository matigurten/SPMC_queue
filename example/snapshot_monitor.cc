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
#include "structs.h"
#include "snapshot_shm.h"

std::atomic<bool> running{true};

void signal_handler(int sig) {
    running = false;
}

void print_stats(const TOBSHM* tob_shm, const FODSHM* fod_shm, uint64_t last_tob_seq, uint64_t last_fod_seq,
                uint64_t tob_count, uint64_t fod_count) {
    std::cout << "\n=== SNAPSHOT STATISTICS ===" << std::endl;
    if (tob_shm) {
        std::cout << "TOB Active consumers: " << tob_shm->active_consumers.load() << std::endl;
        std::cout << "TOB Last sequence: " << tob_shm->last_seq.load() << std::endl;
    }
    if (fod_shm) {
        std::cout << "FOD Active consumers: " << fod_shm->active_consumers.load() << std::endl;
        std::cout << "FOD Last sequence: " << fod_shm->last_seq.load() << std::endl;
    }
    std::cout << "TOB snapshots processed: " << tob_count << std::endl;
    std::cout << "FOD snapshots processed: " << fod_count << std::endl;
    std::cout << "Last TOB sequence seen: " << last_tob_seq << std::endl;
    std::cout << "Last FOD sequence seen: " << last_fod_seq << std::endl;
    std::cout << "===========================" << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <tob_shm_name> <fod_shm_name> [interval_ms]" << std::endl;
        std::cout << "Example: " << argv[0] << " /tob_snapshots /fod_snapshots 1000" << std::endl;
        return 1;
    }
    
    const char* tob_name = argv[1];
    const char* fod_name = argv[2];
    int interval_ms = (argc > 3) ? std::atoi(argv[3]) : 1000;
    
    // Set up signal handler for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "Starting snapshot monitor..." << std::endl;
    std::cout << "TOB shared memory: " << tob_name << std::endl;
    std::cout << "FOD shared memory: " << fod_name << std::endl;
    std::cout << "Monitor interval: " << interval_ms << "ms" << std::endl;
    
    // Open TOB and FOD shared memory
    TOBSHM* tob_shm = open_tob_shm(tob_name);
    FODSHM* fod_shm = open_fod_shm(fod_name);
    
    if (!tob_shm && !fod_shm) {
        std::cerr << "Failed to open both TOB and FOD shared memory" << std::endl;
        std::cerr << "Make sure the reader is running and has created the shared memory files." << std::endl;
        return 1;
    }
    
    TOBConsumer tob_consumer(tob_name);
    FODConsumer fod_consumer(fod_name);
    
    if (!tob_consumer.is_valid() && !fod_consumer.is_valid()) {
        std::cerr << "Failed to connect to both TOB and FOD consumers" << std::endl;
        return 1;
    }
    
    std::cout << "Successfully connected to shared memory" << std::endl;
    
    uint64_t last_tob_seq = 0;
    uint64_t last_fod_seq = 0;
    uint64_t tob_count = 0;
    uint64_t fod_count = 0;
    
    std::cout << "Monitoring snapshots..." << std::endl;
    
    while (running) {
        // Check for new TOB snapshots
        if (tob_consumer.has_new_snapshot(last_tob_seq)) {
            tob_count++;
        }
        
        // Check for new FOD snapshots
        if (fod_consumer.has_new_snapshot(last_fod_seq)) {
            fod_count++;
        }
        
        // Print statistics periodically
        print_stats(tob_shm, fod_shm, last_tob_seq, last_fod_seq, tob_count, fod_count);
        
        // Sleep for the specified interval
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
    
    std::cout << "Shutting down monitor..." << std::endl;
    return 0;
} 