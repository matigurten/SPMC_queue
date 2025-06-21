#include "parquet_encoder.h"
#include <arrow/array/builder_primitive.h>
#include <arrow/array/builder_binary.h>
#include <arrow/table.h>
#include <arrow/io/file.h>
#include <arrow/filesystem/localfs.h>
#include <parquet/exception.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <cmath>

// Utility function to get current timestamp
inline std::string get_timestamp_str() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S");
    ss << "_" << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

FODParquetEncoder::FODParquetEncoder(const std::string& output_dir) 
    : output_dir_(output_dir), current_batch_size_(0), total_snapshots_(0), total_files_(0) {
    
    // Create output directory if it doesn't exist
    std::filesystem::create_directories(output_dir_);
    
    // Initialize schema
    schema_ = create_schema();
    
    // Pre-allocate vectors for efficiency
    size_t initial_capacity = BATCH_SIZE;
    instrument_ids_.reserve(initial_capacity);
    sequences_.reserve(initial_capacity);
    arrival_times_.reserve(initial_capacity);
    publish_times_.reserve(initial_capacity);
    trading_phases_.reserve(initial_capacity);
    last_aggressions_.reserve(initial_capacity);
    side_changed_.reserve(initial_capacity);
    level_changed_.reserve(initial_capacity);
    last_prices_.reserve(initial_capacity);
    volumes_.reserve(initial_capacity);
    
    for (int i = 0; i < 10; ++i) {
        bid_prices_[i].reserve(initial_capacity);
        bid_amounts_[i].reserve(initial_capacity);
        bid_orders_[i].reserve(initial_capacity);
        ask_prices_[i].reserve(initial_capacity);
        ask_amounts_[i].reserve(initial_capacity);
        ask_orders_[i].reserve(initial_capacity);
    }
    
    bid_ask_spreads_.reserve(initial_capacity);
    mid_prices_.reserve(initial_capacity);
    bid_volumes_.reserve(initial_capacity);
    ask_volumes_.reserve(initial_capacity);
    total_volumes_.reserve(initial_capacity);
    order_imbalances_.reserve(initial_capacity);
    price_volatility_.reserve(initial_capacity);
}

FODParquetEncoder::~FODParquetEncoder() {
    close();
}

std::shared_ptr<arrow::Schema> FODParquetEncoder::create_schema() {
    std::vector<std::shared_ptr<arrow::Field>> fields;
    
    // Core metadata (highly compressed)
    fields.push_back(arrow::field("instrument_id", arrow::uint32(), false));
    fields.push_back(arrow::field("sequence", arrow::uint32(), false));
    fields.push_back(arrow::field("arrival_time", arrow::uint64(), false));
    fields.push_back(arrow::field("publish_time", arrow::uint64(), false));
    
    // Trading context (dictionary encoded)
    fields.push_back(arrow::field("trading_phase", arrow::uint8(), false));
    fields.push_back(arrow::field("last_aggression", arrow::uint8(), false));
    fields.push_back(arrow::field("side_changed", arrow::int8(), false));
    fields.push_back(arrow::field("level_changed", arrow::int8(), false));
    
    // Price and volume data (delta encoded)
    fields.push_back(arrow::field("last_price", arrow::float64(), false));
    fields.push_back(arrow::field("volume", arrow::uint64(), false));
    
    // Bid data (flattened for efficient querying)
    for (int i = 0; i < 10; ++i) {
        fields.push_back(arrow::field("bid_price_" + std::to_string(i), arrow::float64(), false));
        fields.push_back(arrow::field("bid_amount_" + std::to_string(i), arrow::uint32(), false));
        fields.push_back(arrow::field("bid_orders_" + std::to_string(i), arrow::uint16(), false));
    }
    
    // Ask data (flattened for efficient querying)
    for (int i = 0; i < 10; ++i) {
        fields.push_back(arrow::field("ask_price_" + std::to_string(i), arrow::float64(), false));
        fields.push_back(arrow::field("ask_amount_" + std::to_string(i), arrow::uint32(), false));
        fields.push_back(arrow::field("ask_orders_" + std::to_string(i), arrow::uint16(), false));
    }
    
    // Derived features for ML (pre-computed)
    fields.push_back(arrow::field("bid_ask_spread", arrow::float64(), false));
    fields.push_back(arrow::field("mid_price", arrow::float64(), false));
    fields.push_back(arrow::field("bid_volume", arrow::float64(), false));
    fields.push_back(arrow::field("ask_volume", arrow::float64(), false));
    fields.push_back(arrow::field("total_volume", arrow::float64(), false));
    fields.push_back(arrow::field("order_imbalance", arrow::float64(), false));
    fields.push_back(arrow::field("price_volatility", arrow::float64(), false));
    
    return std::make_shared<arrow::Schema>(fields);
}

void FODParquetEncoder::add_snapshot(const SnapshotFOD& snapshot, uint64_t publish_time) {
    // Add core data
    instrument_ids_.push_back(snapshot.instrument_id);
    sequences_.push_back(snapshot.seq);
    arrival_times_.push_back(snapshot.arrival_time);
    publish_times_.push_back(publish_time > 0 ? publish_time : snapshot.publish_time);
    trading_phases_.push_back(snapshot.trading_phase);
    last_aggressions_.push_back(snapshot.last_aggression);
    side_changed_.push_back(snapshot.side_changed);
    level_changed_.push_back(snapshot.level_changed);
    last_prices_.push_back(snapshot.last_price);
    volumes_.push_back(snapshot.volume);
    
    // Add bid/ask data
    for (int i = 0; i < 10; ++i) {
        bid_prices_[i].push_back(snapshot.bids[i].price);
        bid_amounts_[i].push_back(snapshot.bids[i].amount);
        bid_orders_[i].push_back(snapshot.bids[i].orders);
        ask_prices_[i].push_back(snapshot.asks[i].price);
        ask_amounts_[i].push_back(snapshot.asks[i].amount);
        ask_orders_[i].push_back(snapshot.asks[i].orders);
    }
    
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
    double order_imbalance = (bid_volume - ask_volume) / total_volume;
    
    // Simple volatility calculation (could be enhanced with rolling window)
    double price_vol = 0.0;
    if (last_prices_.size() > 1) {
        double price_change = snapshot.last_price - last_prices_[last_prices_.size() - 2];
        price_vol = std::abs(price_change);
    }
    
    bid_ask_spreads_.push_back(spread);
    mid_prices_.push_back(mid_price);
    bid_volumes_.push_back(bid_volume);
    ask_volumes_.push_back(ask_volume);
    total_volumes_.push_back(total_volume);
    order_imbalances_.push_back(order_imbalance);
    price_volatility_.push_back(price_vol);
    
    current_batch_size_++;
    total_snapshots_++;
    
    // Flush when batch is full
    if (current_batch_size_ >= BATCH_SIZE) {
        flush_batch();
    }
}

std::shared_ptr<arrow::RecordBatch> FODParquetEncoder::create_batch() {
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    
    // Build core metadata arrays
    arrow::UInt32Builder instrument_builder;
    arrow::UInt32Builder sequence_builder;
    arrow::UInt64Builder arrival_builder;
    arrow::UInt64Builder publish_builder;
    
    PARQUET_THROW_NOT_OK(instrument_builder.AppendValues(instrument_ids_.data(), current_batch_size_));
    PARQUET_THROW_NOT_OK(sequence_builder.AppendValues(sequences_.data(), current_batch_size_));
    PARQUET_THROW_NOT_OK(arrival_builder.AppendValues(arrival_times_.data(), current_batch_size_));
    PARQUET_THROW_NOT_OK(publish_builder.AppendValues(publish_times_.data(), current_batch_size_));
    
    std::shared_ptr<arrow::Array> instrument_array, sequence_array, arrival_array, publish_array;
    PARQUET_THROW_NOT_OK(instrument_builder.Finish(&instrument_array));
    PARQUET_THROW_NOT_OK(sequence_builder.Finish(&sequence_array));
    PARQUET_THROW_NOT_OK(arrival_builder.Finish(&arrival_array));
    PARQUET_THROW_NOT_OK(publish_builder.Finish(&publish_array));
    
    arrays.push_back(instrument_array);
    arrays.push_back(sequence_array);
    arrays.push_back(arrival_array);
    arrays.push_back(publish_array);
    
    // Build trading context arrays
    arrow::UInt8Builder phase_builder, aggression_builder;
    arrow::Int8Builder side_builder, level_builder;
    
    PARQUET_THROW_NOT_OK(phase_builder.AppendValues(trading_phases_.data(), current_batch_size_));
    PARQUET_THROW_NOT_OK(aggression_builder.AppendValues(last_aggressions_.data(), current_batch_size_));
    PARQUET_THROW_NOT_OK(side_builder.AppendValues(side_changed_.data(), current_batch_size_));
    PARQUET_THROW_NOT_OK(level_builder.AppendValues(level_changed_.data(), current_batch_size_));
    
    std::shared_ptr<arrow::Array> phase_array, aggression_array, side_array, level_array;
    PARQUET_THROW_NOT_OK(phase_builder.Finish(&phase_array));
    PARQUET_THROW_NOT_OK(aggression_builder.Finish(&aggression_array));
    PARQUET_THROW_NOT_OK(side_builder.Finish(&side_array));
    PARQUET_THROW_NOT_OK(level_builder.Finish(&level_array));
    
    arrays.push_back(phase_array);
    arrays.push_back(aggression_array);
    arrays.push_back(side_array);
    arrays.push_back(level_array);
    
    // Build price and volume arrays
    arrow::DoubleBuilder price_builder;
    arrow::UInt64Builder volume_builder;
    
    PARQUET_THROW_NOT_OK(price_builder.AppendValues(last_prices_.data(), current_batch_size_));
    PARQUET_THROW_NOT_OK(volume_builder.AppendValues(volumes_.data(), current_batch_size_));
    
    std::shared_ptr<arrow::Array> price_array, volume_array;
    PARQUET_THROW_NOT_OK(price_builder.Finish(&price_array));
    PARQUET_THROW_NOT_OK(volume_builder.Finish(&volume_array));
    
    arrays.push_back(price_array);
    arrays.push_back(volume_array);
    
    // Build bid/ask arrays
    for (int i = 0; i < 10; ++i) {
        arrow::DoubleBuilder bid_price_builder;
        arrow::UInt32Builder bid_amount_builder;
        arrow::UInt16Builder bid_order_builder;
        
        PARQUET_THROW_NOT_OK(bid_price_builder.AppendValues(bid_prices_[i].data(), current_batch_size_));
        PARQUET_THROW_NOT_OK(bid_amount_builder.AppendValues(bid_amounts_[i].data(), current_batch_size_));
        PARQUET_THROW_NOT_OK(bid_order_builder.AppendValues(bid_orders_[i].data(), current_batch_size_));
        
        std::shared_ptr<arrow::Array> bid_price_array, bid_amount_array, bid_order_array;
        PARQUET_THROW_NOT_OK(bid_price_builder.Finish(&bid_price_array));
        PARQUET_THROW_NOT_OK(bid_amount_builder.Finish(&bid_amount_array));
        PARQUET_THROW_NOT_OK(bid_order_builder.Finish(&bid_order_array));
        
        arrays.push_back(bid_price_array);
        arrays.push_back(bid_amount_array);
        arrays.push_back(bid_order_array);
    }
    
    for (int i = 0; i < 10; ++i) {
        arrow::DoubleBuilder ask_price_builder;
        arrow::UInt32Builder ask_amount_builder;
        arrow::UInt16Builder ask_order_builder;
        
        PARQUET_THROW_NOT_OK(ask_price_builder.AppendValues(ask_prices_[i].data(), current_batch_size_));
        PARQUET_THROW_NOT_OK(ask_amount_builder.AppendValues(ask_amounts_[i].data(), current_batch_size_));
        PARQUET_THROW_NOT_OK(ask_order_builder.AppendValues(ask_orders_[i].data(), current_batch_size_));
        
        std::shared_ptr<arrow::Array> ask_price_array, ask_amount_array, ask_order_array;
        PARQUET_THROW_NOT_OK(ask_price_builder.Finish(&ask_price_array));
        PARQUET_THROW_NOT_OK(ask_amount_builder.Finish(&ask_amount_array));
        PARQUET_THROW_NOT_OK(ask_order_builder.Finish(&ask_order_array));
        
        arrays.push_back(ask_price_array);
        arrays.push_back(ask_amount_array);
        arrays.push_back(ask_order_array);
    }
    
    // Build derived feature arrays
    arrow::DoubleBuilder spread_builder, mid_builder, bid_vol_builder, ask_vol_builder;
    arrow::DoubleBuilder total_vol_builder, imbalance_builder, volatility_builder;
    
    PARQUET_THROW_NOT_OK(spread_builder.AppendValues(bid_ask_spreads_.data(), current_batch_size_));
    PARQUET_THROW_NOT_OK(mid_builder.AppendValues(mid_prices_.data(), current_batch_size_));
    PARQUET_THROW_NOT_OK(bid_vol_builder.AppendValues(bid_volumes_.data(), current_batch_size_));
    PARQUET_THROW_NOT_OK(ask_vol_builder.AppendValues(ask_volumes_.data(), current_batch_size_));
    PARQUET_THROW_NOT_OK(total_vol_builder.AppendValues(total_volumes_.data(), current_batch_size_));
    PARQUET_THROW_NOT_OK(imbalance_builder.AppendValues(order_imbalances_.data(), current_batch_size_));
    PARQUET_THROW_NOT_OK(volatility_builder.AppendValues(price_volatility_.data(), current_batch_size_));
    
    std::shared_ptr<arrow::Array> spread_array, mid_array, bid_vol_array, ask_vol_array;
    std::shared_ptr<arrow::Array> total_vol_array, imbalance_array, volatility_array;
    
    PARQUET_THROW_NOT_OK(spread_builder.Finish(&spread_array));
    PARQUET_THROW_NOT_OK(mid_builder.Finish(&mid_array));
    PARQUET_THROW_NOT_OK(bid_vol_builder.Finish(&bid_vol_array));
    PARQUET_THROW_NOT_OK(ask_vol_builder.Finish(&ask_vol_array));
    PARQUET_THROW_NOT_OK(total_vol_builder.Finish(&total_vol_array));
    PARQUET_THROW_NOT_OK(imbalance_builder.Finish(&imbalance_array));
    PARQUET_THROW_NOT_OK(volatility_builder.Finish(&volatility_array));
    
    arrays.push_back(spread_array);
    arrays.push_back(mid_array);
    arrays.push_back(bid_vol_array);
    arrays.push_back(ask_vol_array);
    arrays.push_back(total_vol_array);
    arrays.push_back(imbalance_array);
    arrays.push_back(volatility_array);
    
    return arrow::RecordBatch::Make(schema_, current_batch_size_, arrays);
}

void FODParquetEncoder::write_batch_to_parquet(const std::shared_ptr<arrow::RecordBatch>& batch) {
    std::string filename = generate_filename();
    std::string filepath = output_dir_ + "/" + filename;
    
    // Create output stream
    std::shared_ptr<arrow::io::FileOutputStream> outfile;
    PARQUET_THROW_NOT_OK(arrow::io::FileOutputStream::Open(filepath, &outfile));
    
    // Configure Parquet writer properties for optimal compression and query performance
    parquet::WriterProperties::Builder builder;
    builder.compression(parquet::Compression::SNAPPY)  // Fast compression/decompression
           .encoding(parquet::Encoding::PLAIN_DICTIONARY)  // Dictionary encoding for repeated values
           .max_row_group_length(100000)  // Optimal row group size for query performance
           .enable_statistics()  // Enable statistics for predicate pushdown
           .data_page_size_limit(1024 * 1024);  // 1MB page size
    
    auto properties = builder.build();
    
    // Create schema properties
    parquet::schema::GroupNode::Make("schema", parquet::Repetition::REQUIRED, {});
    auto schema_properties = parquet::schema::GroupNode::Make("schema", parquet::Repetition::REQUIRED, {});
    
    // Create writer
    std::unique_ptr<parquet::arrow::FileWriter> writer;
    PARQUET_THROW_NOT_OK(parquet::arrow::FileWriter::Open(*schema_, 
                                                          arrow::default_memory_pool(),
                                                          outfile, 
                                                          properties, 
                                                          &writer));
    
    // Write batch
    PARQUET_THROW_NOT_OK(writer->WriteRecordBatch(*batch));
    PARQUET_THROW_NOT_OK(writer->Close());
    
    total_files_++;
    std::cout << "Written " << current_batch_size_ << " records to " << filename 
              << " (Total: " << total_snapshots_ << " snapshots, " << total_files_ << " files)" << std::endl;
}

std::string FODParquetEncoder::generate_filename() const {
    return "fod_snapshots_" + get_timestamp_str() + ".parquet";
}

void FODParquetEncoder::flush_batch() {
    if (current_batch_size_ == 0) return;
    
    auto batch = create_batch();
    write_batch_to_parquet(batch);
    
    // Clear batch data
    clear_batch_data();
}

void FODParquetEncoder::clear_batch_data() {
    current_batch_size_ = 0;
    
    instrument_ids_.clear();
    sequences_.clear();
    arrival_times_.clear();
    publish_times_.clear();
    trading_phases_.clear();
    last_aggressions_.clear();
    side_changed_.clear();
    level_changed_.clear();
    last_prices_.clear();
    volumes_.clear();
    
    for (int i = 0; i < 10; ++i) {
        bid_prices_[i].clear();
        bid_amounts_[i].clear();
        bid_orders_[i].clear();
        ask_prices_[i].clear();
        ask_amounts_[i].clear();
        ask_orders_[i].clear();
    }
    
    bid_ask_spreads_.clear();
    mid_prices_.clear();
    bid_volumes_.clear();
    ask_volumes_.clear();
    total_volumes_.clear();
    order_imbalances_.clear();
    price_volatility_.clear();
}

void FODParquetEncoder::close() {
    flush_batch();
    std::cout << "FOD Parquet encoder closed. Total: " << total_snapshots_ 
              << " snapshots written to " << total_files_ << " files." << std::endl;
} 