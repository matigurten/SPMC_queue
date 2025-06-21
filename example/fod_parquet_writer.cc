#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <string>
#include <thread>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <cstring>
#include "structs.h"
#include "snapshot_shm.h"

// Simple CSV-based Parquet-like encoding for FOD snapshots
// This provides a foundation that can be upgraded to full Parquet later

class FODDataWriter {
public:
    FODDataWriter(const std::string& output_dir, size_t batch_size = 10000)
        : output_dir_(output_dir), batch_size_(batch_size), current_batch_(0), total_snapshots_(0) {
        
        // Create output directory
        create_directory(output_dir_);
        
        // Initialize CSV file with headers
        open_new_file();
        write_headers();
    }
    
    ~FODDataWriter() {
        flush_batch();
        if (file_.is_open()) {
            file_.close();
        }
    }
    
    void add_snapshot(const SnapshotFOD& snapshot) {
        // Calculate derived features for ML
        double best_bid = snapshot.bids[0].price;
        double best_ask = snapshot.asks[0].price;
        double mid_price = (best_bid + best_ask) / 2.0;
        double spread = best_ask - best_bid;
        
        double bid_volume = 0.0, ask_volume = 0.0;
        for (int i = 0; i < 10; ++i) {
            bid_volume += snapshot.bids[i].amount;
            ask_volume += snapshot.asks[i].amount;
        }
        
        double total_volume = bid_volume + ask_volume;
        double order_imbalance = total_volume > 0 ? (bid_volume - ask_volume) / total_volume : 0.0;
        
        // Write to CSV (simplified for now, can be upgraded to Parquet)
        file_ << snapshot.instrument_id << ","
              << snapshot.seq << ","
              << snapshot.arrival_time << ","
              << snapshot.publish_time << ","
              << (int)snapshot.trading_phase << ","
              << (int)snapshot.last_aggression << ","
              << (int)snapshot.side_changed << ","
              << (int)snapshot.level_changed << ","
              << std::fixed << std::setprecision(6) << snapshot.last_price << ","
              << snapshot.volume << ",";
        
        // Bid data (flattened)
        for (int i = 0; i < 10; ++i) {
            file_ << std::fixed << std::setprecision(6) << snapshot.bids[i].price << ","
                  << snapshot.bids[i].amount << ","
                  << snapshot.bids[i].orders << ",";
        }
        
        // Ask data (flattened)
        for (int i = 0; i < 10; ++i) {
            file_ << std::fixed << std::setprecision(6) << snapshot.asks[i].price << ","
                  << snapshot.asks[i].amount << ","
                  << snapshot.asks[i].orders << ",";
        }
        
        // Derived features for ML
        file_ << std::fixed << std::setprecision(6) << spread << ","
              << mid_price << ","
              << bid_volume << ","
              << ask_volume << ","
              << total_volume << ","
              << order_imbalance << ","
              << 0.0  // price_volatility placeholder
              << std::endl;
        
        current_batch_++;
        total_snapshots_++;
        
        // Flush when batch is full
        if (current_batch_ >= batch_size_) {
            flush_batch();
        }
    }
    
    void flush_batch() {
        if (current_batch_ > 0) {
            file_.flush();
            std::cout << "Flushed " << current_batch_ << " FOD snapshots to " 
                      << current_filename_ << " (Total: " << total_snapshots_ << ")" << std::endl;
            current_batch_ = 0;
        }
    }
    
    uint64_t get_total_snapshots() const { return total_snapshots_; }

private:
    void create_directory(const std::string& path) {
        // Simple directory creation using mkdir
        if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
            std::cerr << "Failed to create directory " << path << ": " << strerror(errno) << std::endl;
        }
    }
    
    void open_new_file() {
        if (file_.is_open()) {
            file_.close();
        }
        
        current_filename_ = generate_filename();
        std::string filepath = output_dir_ + "/" + current_filename_;
        file_.open(filepath);
        
        if (!file_.is_open()) {
            throw std::runtime_error("Failed to open file: " + filepath);
        }
    }
    
    void write_headers() {
        file_ << "instrument_id,sequence,arrival_time,publish_time,"
              << "trading_phase,last_aggression,side_changed,level_changed,"
              << "last_price,volume,";
        
        // Bid headers
        for (int i = 0; i < 10; ++i) {
            file_ << "bid_price_" << i << ",bid_amount_" << i << ",bid_orders_" << i << ",";
        }
        
        // Ask headers
        for (int i = 0; i < 10; ++i) {
            file_ << "ask_price_" << i << ",ask_amount_" << i << ",ask_orders_" << i << ",";
        }
        
        // Derived feature headers
        file_ << "bid_ask_spread,mid_price,bid_volume,ask_volume,"
              << "total_volume,order_imbalance,price_volatility" << std::endl;
    }
    
    std::string generate_filename() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << "fod_snapshots_" 
           << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S")
           << "_" << std::setfill('0') << std::setw(3) << ms.count()
           << ".csv";
        return ss.str();
    }

private:
    std::string output_dir_;
    std::string current_filename_;
    std::ofstream file_;
    size_t batch_size_;
    size_t current_batch_;
    uint64_t total_snapshots_;
};

// Integration with existing snapshot consumer
class FODParquetConsumer {
public:
    FODParquetConsumer(const std::string& shm_name, const std::string& output_dir)
        : consumer_(shm_name.c_str()), writer_(output_dir) {
        
        if (!consumer_.is_valid()) {
            throw std::runtime_error("Failed to connect to FOD shared memory: " + shm_name);
        }
        
        std::cout << "FOD Parquet consumer started. Writing to: " << output_dir << std::endl;
    }
    
    void run() {
        uint64_t last_seq = 0;
        
        while (true) {
            if (consumer_.has_new_snapshot(last_seq)) {
                const SnapshotFOD* snapshot = consumer_.get_latest_snapshot();
                if (snapshot) {
                    writer_.add_snapshot(*snapshot);
                }
            }
            
            // Small sleep to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
    
    void stop() {
        writer_.flush_batch();
        std::cout << "FOD Parquet consumer stopped. Total snapshots: " 
                  << writer_.get_total_snapshots() << std::endl;
    }

private:
    FODConsumer consumer_;
    FODDataWriter writer_;
};

// Main function for standalone FOD Parquet writer
int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <fod_shm_name> <output_dir> [batch_size]" << std::endl;
        std::cout << "Example: " << argv[0] << " /fod_snapshots /data/fod_parquet 10000" << std::endl;
        return 1;
    }
    
    const char* shm_name = argv[1];
    const char* output_dir = argv[2];
    size_t batch_size = (argc > 3) ? std::atoi(argv[3]) : 10000;
    
    try {
        FODParquetConsumer consumer(shm_name, output_dir);
        
        // Set up signal handler for graceful shutdown
        signal(SIGINT, [](int) { 
            std::cout << "Received SIGINT, shutting down..." << std::endl;
            exit(0);
        });
        
        std::cout << "FOD Parquet consumer running. Press Ctrl+C to stop." << std::endl;
        consumer.run();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 