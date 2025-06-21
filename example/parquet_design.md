# Optimal Parquet Design for FOD Snapshots

## Schema Design for Maximum Efficiency

### 1. **Flattened Structure** (vs. Nested)
Instead of nested bid/ask arrays, flatten into individual columns:
```parquet
// ❌ Nested (inefficient for queries)
bids: [
  {price: 100.0, amount: 1000, orders: 5},
  {price: 99.9, amount: 500, orders: 3}
]

// ✅ Flattened (optimal for queries)
bid_price_0: 100.0, bid_amount_0: 1000, bid_orders_0: 5
bid_price_1: 99.9, bid_amount_1: 500, bid_orders_1: 3
```

### 2. **Column Order Optimization**
Order columns by query frequency:
```parquet
// High-frequency query columns first
instrument_id, sequence, arrival_time, publish_time
trading_phase, last_aggression, side_changed, level_changed
last_price, volume

// Bid data (flattened)
bid_price_0, bid_amount_0, bid_orders_0
bid_price_1, bid_amount_1, bid_orders_1
...
bid_price_9, bid_amount_9, bid_orders_9

// Ask data (flattened)
ask_price_0, ask_amount_0, ask_orders_0
ask_price_1, ask_amount_1, ask_orders_1
...
ask_price_9, ask_amount_9, ask_orders_9

// Pre-computed ML features
bid_ask_spread, mid_price, bid_volume, ask_volume
total_volume, order_imbalance, price_volatility
```

### 3. **Data Types Optimization**
```parquet
// Use smallest possible types
instrument_id: uint32 (not uint64)
sequence: uint32 (not uint64)
trading_phase: uint8 (not uint16)
last_aggression: uint8
side_changed: int8
level_changed: int8
bid_orders_X: uint16 (not uint32)
ask_orders_X: uint16 (not uint32)
```

## Compression Strategy

### 1. **Column-Specific Encoding**
```parquet
// Dictionary encoding for categorical data
trading_phase: DICTIONARY (only 4-5 values)
last_aggression: DICTIONARY (only 2-3 values)
side_changed: DICTIONARY (only 3 values: -1, 0, 1)
level_changed: DICTIONARY (only 11 values: -10 to 0)

// Delta encoding for sequential data
sequence: DELTA_BINARY_PACKED
arrival_time: DELTA_BINARY_PACKED
publish_time: DELTA_BINARY_PACKED

// Plain encoding for floating point
bid_price_X: PLAIN
ask_price_X: PLAIN
last_price: PLAIN
```

### 2. **Compression Algorithm**
```parquet
// Snappy for speed (good compression, fast decompression)
compression: SNAPPY

// Alternative: ZSTD for better compression if storage is concern
compression: ZSTD
```

## File Organization

### 1. **Partitioning Strategy**
```
/data/fod_snapshots/
  /instrument_id=1/
    /date=20250621/
      /hour=09/
        part-00000.parquet
        part-00001.parquet
      /hour=10/
        part-00000.parquet
  /instrument_id=2/
    /date=20250621/
      /hour=09/
        part-00000.parquet
```

### 2. **File Sizing**
```parquet
// Optimal file sizes
row_group_size: 100,000 rows
max_file_size: 100MB
target_records_per_file: 1,000,000

// Benefits:
// - Fast predicate pushdown
// - Efficient parallel processing
// - Good compression ratios
```

## Pre-computed Features for ML

### 1. **Technical Indicators**
```parquet
// Pre-computed to avoid runtime calculation
bid_ask_spread: ask_price_0 - bid_price_0
mid_price: (bid_price_0 + ask_price_0) / 2
bid_volume: sum(bid_amount_0 to bid_amount_9)
ask_volume: sum(ask_amount_0 to ask_amount_9)
total_volume: bid_volume + ask_volume
order_imbalance: (bid_volume - ask_volume) / total_volume
price_volatility: abs(last_price - previous_last_price)
```

### 2. **Advanced Features**
```parquet
// Volume-weighted average price
vwap_bid: sum(bid_price_X * bid_amount_X) / bid_volume
vwap_ask: sum(ask_price_X * ask_amount_X) / ask_volume

// Order book depth
depth_bid_5: sum(bid_amount_0 to bid_amount_4)
depth_ask_5: sum(ask_amount_0 to ask_amount_4)

// Price impact
price_impact_1000: price change for 1000 unit order
```

## Query Optimization

### 1. **Statistics and Predicate Pushdown**
```parquet
// Enable statistics for all columns
enable_statistics: true

// Column statistics help with:
// - Predicate pushdown (skip irrelevant row groups)
// - Query planning optimization
// - Partition pruning
```

### 2. **Indexing Strategy**
```parquet
// Primary index on time
arrival_time: SORTED

// Secondary indexes on common filters
instrument_id: SORTED
trading_phase: DICTIONARY
sequence: SORTED
```

## Example Usage for Research

### 1. **Python/Pandas Integration**
```python
import pandas as pd
import pyarrow.parquet as pq

# Read specific time range efficiently
df = pq.read_table(
    'data/fod_snapshots/',
    filters=[
        ('arrival_time', '>=', start_timestamp),
        ('arrival_time', '<', end_timestamp),
        ('instrument_id', '=', 1)
    ]
).to_pandas()

# Fast aggregations
spread_stats = df.groupby('trading_phase')['bid_ask_spread'].agg(['mean', 'std'])
volume_profile = df.groupby('hour')['total_volume'].sum()
```

### 2. **ML Feature Engineering**
```python
# Efficient feature extraction
features = df[[
    'bid_ask_spread', 'mid_price', 'order_imbalance',
    'bid_volume', 'ask_volume', 'price_volatility',
    'bid_price_0', 'ask_price_0', 'last_price'
]].values

# Time-series features
df['price_momentum'] = df['last_price'].diff()
df['volume_momentum'] = df['total_volume'].diff()
df['spread_trend'] = df['bid_ask_spread'].rolling(20).mean()
```

## Performance Benchmarks

### Expected Performance:
- **Compression Ratio**: 5-10x (vs. CSV)
- **Query Speed**: 10-50x faster than CSV
- **Storage Cost**: 80-90% reduction vs. raw data
- **Write Speed**: 100K-1M records/second
- **Read Speed**: 1M-10M records/second

### Memory Usage:
- **Batch Size**: 10K records optimal
- **Row Group Size**: 100K records optimal
- **File Size**: 100MB optimal

## Implementation Considerations

### 1. **Real-time Streaming**
```cpp
// Batch writing for real-time data
class FODParquetStreamer {
    void add_snapshot(const SnapshotFOD& snapshot) {
        batch_.push_back(snapshot);
        if (batch_.size() >= BATCH_SIZE) {
            flush_batch();
        }
    }
};
```

### 2. **Backfill Strategy**
```cpp
// Efficient backfill from existing data
void backfill_parquet(const std::vector<SnapshotFOD>& snapshots) {
    // Sort by time for optimal compression
    std::sort(snapshots.begin(), snapshots.end(), 
              [](const auto& a, const auto& b) {
                  return a.arrival_time < b.arrival_time;
              });
    
    // Write in batches
    for (size_t i = 0; i < snapshots.size(); i += BATCH_SIZE) {
        write_batch(snapshots.begin() + i, 
                   snapshots.begin() + std::min(i + BATCH_SIZE, snapshots.size()));
    }
}
```

This design provides maximum efficiency for research teams to investigate market microstructure and build predictive models while maintaining excellent query performance and storage efficiency. 