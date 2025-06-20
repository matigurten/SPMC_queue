#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>
#include <iomanip>
#include "snapshot_shm.h"
#include "trading_phases.h"

volatile bool running = true;

void signal_handler(int sig) {
    running = false;
}

void print_tob_snapshot(const SnapshotTOB& tob, uint64_t seq) {
    std::cout << "\n=== TOP OF BOOK SNAPSHOT (Seq " << seq << ") ===" << std::endl;
    std::cout << "Timestamp: " << tob.timestamp << std::endl;
    std::cout << "Trading Phase: " << TradingPhase::get_phase_name(tob.trading_phase) << std::endl;
    std::cout << "Last Trade: price=" << tob.last_price 
              << ", aggression=" << tob.last_aggression << std::endl;
    std::cout << "Volume: " << tob.volume << std::endl;
    std::cout << "Best Bid: " << tob.bid_price << " x " << tob.bid_amount 
              << " (" << tob.bid_orders << " orders)" << std::endl;
    std::cout << "Best Ask: " << tob.ask_price << " x " << tob.ask_amount 
              << " (" << tob.ask_orders << " orders)" << std::endl;
    std::cout << "Spread: " << (tob.ask_price - tob.bid_price) << std::endl;
    std::cout << "=====================================" << std::endl;
}

void print_fod_snapshot(const SnapshotFOD& fod, uint64_t seq) {
    std::cout << "\n=== FULL ORDER DEPTH SNAPSHOT (Seq " << seq << ") ===" << std::endl;
    std::cout << "Timestamp: " << fod.timestamp << std::endl;
    std::cout << "Trading Phase: " << TradingPhase::get_phase_name(fod.trading_phase) << std::endl;
    std::cout << "Last Trade: price=" << fod.last_price 
              << ", aggression=" << fod.last_aggression << std::endl;
    std::cout << "Volume: " << fod.volume << std::endl;
    
    std::cout << "\nLevel |    Bids     |    Asks     |" << std::endl;
    std::cout << "      | Price x Qty | Price x Qty |" << std::endl;
    std::cout << "------|-------------|-------------|" << std::endl;
    
    for (int i = 0; i < 10; ++i) {  // Show top 10 levels
        std::cout << std::setw(5) << i << " | ";
        
        // Print bid level
        if (fod.bids[i].amount > 0) {
            std::cout << std::fixed << std::setprecision(1) 
                      << std::setw(6) << fod.bids[i].price << " x" 
                      << std::setw(4) << fod.bids[i].amount;
        } else {
            std::cout << "         ";
        }
        
        std::cout << " | ";
        
        // Print ask level
        if (fod.asks[i].amount > 0) {
            std::cout << std::fixed << std::setprecision(1) 
                      << std::setw(6) << fod.asks[i].price << " x" 
                      << std::setw(4) << fod.asks[i].amount;
        } else {
            std::cout << "         ";
        }
        
        std::cout << " |" << std::endl;
    }
    std::cout << "=====================================" << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <snapshot_shm_name> [consumer_id]" << std::endl;
        std::cout << "Example: " << argv[0] << " /snapshots consumer1" << std::endl;
        return 1;
    }
    
    const char* snapshot_name = argv[1];
    std::string consumer_id = (argc > 2) ? argv[2] : "consumer";
    
    // Set up signal handler for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "Starting snapshot consumer: " << consumer_id << std::endl;
    std::cout << "Connecting to snapshot shared memory: " << snapshot_name << std::endl;
    
    // Create consumer
    SnapshotConsumer consumer(snapshot_name);
    if (!consumer.is_valid()) {
        std::cerr << "Failed to connect to snapshot shared memory: " << snapshot_name << std::endl;
        return 1;
    }
    
    std::cout << "Successfully connected to snapshot shared memory" << std::endl;
    
    uint64_t last_tob_seq = 0;
    uint64_t last_fod_seq = 0;
    uint64_t last_write_seq = 0;
    
    std::cout << "Waiting for snapshots..." << std::endl;
    
    while (running) {
        // Check for new TOB snapshots
        if (consumer.has_new_tob(last_tob_seq)) {
            const SnapshotTOB* tob = consumer.get_latest_tob();
            if (tob) {
                print_tob_snapshot(*tob, last_tob_seq);
                consumer.update_read_timestamp();
            }
        }
        
        // Check for new FOD snapshots
        if (consumer.has_new_fod(last_fod_seq)) {
            const SnapshotFOD* fod = consumer.get_latest_fod();
            if (fod) {
                print_fod_snapshot(*fod, last_fod_seq);
                consumer.update_read_timestamp();
            }
        }
        
        // Sleep to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    std::cout << "\nShutting down consumer: " << consumer_id << std::endl;
    return 0;
} 