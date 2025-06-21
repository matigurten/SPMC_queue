#pragma once

#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/writer.h>
#include <parquet/arrow/reader.h>
#include <arrow/filesystem/api.h>
#include <memory>
#include <vector>
#include <string>
#include "structs.h"

// Efficient Parquet schema for FOD snapshots optimized for research
class FODParquetEncoder {
public:
    FODParquetEncoder(const std::string& output_dir);
    ~FODParquetEncoder();
    
    // Add a single FOD snapshot to the batch
    void add_snapshot(const SnapshotFOD& snapshot, uint64_t publish_time = 0);
    
    // Flush current batch to Parquet file
    void flush_batch();
    
    // Close and finalize
    void close();
    
    // Get statistics
    uint64_t get_total_snapshots() const { return total_snapshots_; }
    uint64_t get_total_files() const { return total_files_; }

private:
    // Create optimized Arrow schema for FOD data
    std::shared_ptr<arrow::Schema> create_schema();
    
    // Convert FOD snapshot to Arrow record batch
    std::shared_ptr<arrow::RecordBatch> create_batch();
    
    // Write batch to Parquet with optimal settings
    void write_batch_to_parquet(const std::shared_ptr<arrow::RecordBatch>& batch);
    
    // Generate filename with timestamp
    std::string generate_filename() const;

private:
    std::string output_dir_;
    std::shared_ptr<arrow::Schema> schema_;
    
    // Batch data storage (optimized for memory efficiency)
    std::vector<uint32_t> instrument_ids_;
    std::vector<uint32_t> sequences_;
    std::vector<uint64_t> arrival_times_;
    std::vector<uint64_t> publish_times_;
    std::vector<uint8_t> trading_phases_;
    std::vector<uint8_t> last_aggressions_;
    std::vector<int8_t> side_changed_;
    std::vector<int8_t> level_changed_;
    std::vector<double> last_prices_;
    std::vector<uint64_t> volumes_;
    
    // Bid/Ask data (flattened for efficient querying)
    std::vector<double> bid_prices_[10];
    std::vector<uint32_t> bid_amounts_[10];
    std::vector<uint16_t> bid_orders_[10];
    std::vector<double> ask_prices_[10];
    std::vector<uint32_t> ask_amounts_[10];
    std::vector<uint16_t> ask_orders_[10];
    
    // Derived features for ML (pre-computed)
    std::vector<double> bid_ask_spreads_;
    std::vector<double> mid_prices_;
    std::vector<double> bid_volumes_;
    std::vector<double> ask_volumes_;
    std::vector<double> total_volumes_;
    std::vector<double> order_imbalances_;
    std::vector<double> price_volatility_;
    
    // Batch management
    static constexpr size_t BATCH_SIZE = 10000; // Optimal batch size for Parquet
    size_t current_batch_size_;
    uint64_t total_snapshots_;
    uint64_t total_files_;
    
    // File management
    std::shared_ptr<arrow::io::OutputStream> current_file_;
    std::unique_ptr<parquet::arrow::FileWriter> current_writer_;
};

// Utility class for reading and analyzing FOD Parquet files
class FODParquetReader {
public:
    FODParquetReader(const std::string& input_dir);
    
    // Read all files and return as Arrow table
    std::shared_ptr<arrow::Table> read_all_files();
    
    // Read specific time range
    std::shared_ptr<arrow::Table> read_time_range(uint64_t start_time, uint64_t end_time);
    
    // Get statistics about the data
    void print_statistics();
    
    // Export to CSV for external analysis
    void export_to_csv(const std::string& output_file);

private:
    std::string input_dir_;
    std::vector<std::string> parquet_files_;
};

// Feature engineering utilities for ML
class FODFeatureEngineer {
public:
    // Calculate technical indicators from FOD data
    static std::vector<double> calculate_bid_ask_spread(const std::vector<double>& bid_prices, 
                                                       const std::vector<double>& ask_prices);
    
    static std::vector<double> calculate_order_imbalance(const std::vector<double>& bid_volumes,
                                                        const std::vector<double>& ask_volumes);
    
    static std::vector<double> calculate_price_volatility(const std::vector<double>& mid_prices, 
                                                         int window = 20);
    
    static std::vector<double> calculate_volume_profile(const std::vector<double>& volumes,
                                                       int window = 50);
    
    // Create ML-ready features
    static std::shared_ptr<arrow::Table> create_ml_features(const std::shared_ptr<arrow::Table>& fod_data);
};

// Configuration for optimal Parquet encoding
struct ParquetConfig {
    // Compression settings
    parquet::Compression::type compression = parquet::Compression::SNAPPY;
    
    // Row group size (optimal for query performance)
    int64_t row_group_size = 100000;
    
    // Dictionary encoding threshold
    int dictionary_page_size_limit = 1024 * 1024; // 1MB
    
    // Column encoding
    bool enable_dictionary = true;
    bool enable_statistics = true;
    
    // Batch settings
    size_t batch_size = 10000;
    
    // File rotation
    size_t max_file_size_mb = 100; // Rotate files at 100MB
    size_t max_records_per_file = 1000000; // Or 1M records
}; 