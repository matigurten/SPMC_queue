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

std::atomic<bool> running{true};

void signal_handler(int sig) {
    running = false;
}

// Use the same clock as the rest of the system
inline uint64_t get_ns_since_epoch() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL + ts.tv_nsec;
}

void print_tob_snapshot(const SnapshotTOB& tob) {
    uint64_t read_time = get_ns_since_epoch();
    uint64_t latency = read_time > tob.timestamp ? read_time - tob.timestamp : 0;
    std::cout << "TOB [seq=" << tob.seq
              << ", bid=" << tob.bid_amount << "@" << tob.bid_price << " #" << tob.bid_orders
              << ", ask=" << tob.ask_amount << "@" << tob.ask_price << " #" << tob.ask_orders
              << ", last=" << tob.last_price
              << ", volume=" << tob.volume
              << ", agg=" << (int)tob.last_aggression
              << ", phase=" << (int)tob.trading_phase
              << ", level_changed=" << (int)tob.level_changed
              << ", side_changed=" << (int)tob.side_changed
              << ", latency_to_consumer=" << latency << " ns"
              << "]" << std::endl;
}

void print_fod_snapshot(const SnapshotFOD& fod, int max_levels) {
    uint64_t read_time = get_ns_since_epoch();
    uint64_t latency = read_time > fod.timestamp ? read_time - fod.timestamp : 0;
    std::cout << "FOD [seq=" << fod.seq
              << ", last=" << fod.last_price
              << ", volume=" << fod.volume
              << ", agg=" << (int)fod.last_aggression
              << ", phase=" << (int)fod.trading_phase
              << ", level_changed=" << (int)fod.level_changed
              << ", side_changed=" << (int)fod.side_changed
              << ", latency_to_consumer=" << latency << " ns"
              << "]" << std::endl;
    
    // Print bid levels
    std::cout << "  Bids: ";
    int printed = 0;
    for (int i = 0; i < 10 && printed < max_levels; i++) {
        if (fod.bids[i].amount > 0) {
            std::cout << std::setw(8) << fod.bids[i].amount << "@"
                      << std::setw(8) << fod.bids[i].price << " #"
                      << std::setw(3) << fod.bids[i].orders << "\t";
            printed++;
        }
    }
    std::cout << std::endl;
    
    // Print ask levels
    std::cout << "  Asks: ";
    printed = 0;
    for (int i = 0; i < 10 && printed < max_levels; i++) {
        if (fod.asks[i].amount > 0) {
            std::cout << std::setw(8) << fod.asks[i].amount << "@"
                      << std::setw(8) << fod.asks[i].price << " #"
                      << std::setw(3) << fod.asks[i].orders << "\t";
            printed++;
        }
    }
    std::cout << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <snapshot_shm_name> [consumer_id] [levels]" << std::endl;
        std::cout << "Example: " << argv[0] << " snapqueue consumer1 4" << std::endl;
        return 1;
    }
    
    const char* shm_name = argv[1];
    std::string consumer_id = (argc > 2) ? argv[2] : "consumer";
    int max_levels = (argc > 3) ? std::atoi(argv[3]) : 4;
    if (max_levels < 1 || max_levels > 10) max_levels = 4;
    
    // Set up signal handler for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "Starting snapshot consumer: " << consumer_id << std::endl;
    std::cout << "Connecting to snapshot shared memory: " << shm_name << std::endl;
    
    // Open SnapshotSHM shared memory
    SnapshotSHM* shm = open_snapshot_shm(shm_name);
    if (!shm) {
        std::cerr << "Failed to connect to snapshot shared memory: " << shm_name << std::endl;
        std::cerr << "Make sure the reader is running and has created the shared memory." << std::endl;
        return 1;
    }
    
    std::cout << "Successfully connected to shared memory" << std::endl;
    
    uint64_t last_tob_seq = 0;
    uint64_t last_fod_seq = 0;
    
    std::cout << "Waiting for snapshots..." << std::endl;
    
    while (running) {
        // Check for new TOB snapshot
        uint64_t current_tob_seq = shm->last_tob_seq.load(std::memory_order_acquire);
        if (current_tob_seq > last_tob_seq) {
            print_tob_snapshot(shm->latest_tob);
            last_tob_seq = current_tob_seq;
        }
        
        // Check for new FOD snapshot
        uint64_t current_fod_seq = shm->last_fod_seq.load(std::memory_order_acquire);
        if (current_fod_seq > last_fod_seq) {
            print_fod_snapshot(shm->latest_fod, max_levels);
            last_fod_seq = current_fod_seq;
        }
        
        // Busy waiting - no sleep for minimal latency
    }
    
    std::cout << "\nShutting down consumer: " << consumer_id << std::endl;
    return 0;
} 