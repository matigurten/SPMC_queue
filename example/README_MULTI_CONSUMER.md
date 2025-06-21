# Multi-Consumer Snapshot System

This system implements a high-performance multi-consumer architecture for distributing order book snapshots using shared memory.

## Architecture Overview

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   Writer    │───▶│   Reader    │───▶│  Snapshot   │
│ (shm_writer)│    │(shm_reader) │    │    SHM      │
└─────────────┘    └─────────────┘    └─────────────┘
                                              │
                                              ▼
                    ┌─────────────┬─────────────┬─────────────┐
                    │ Consumer 1  │ Consumer 2  │   Monitor   │
                    │(snapshot_   │(snapshot_   │(snapshot_   │
                    │ consumer)   │ consumer)   │ monitor)    │
                    └─────────────┴─────────────┴─────────────┘
```

## Components

### 1. Snapshot Shared Memory (`snapshot_shm.h`)
- **SnapshotSHM**: Shared memory structure containing latest snapshots
- **SnapshotConsumer**: C++ class for consuming snapshots
- Helper functions for creating/opening shared memory

### 2. Reader (`shm_reader.cc`)
- Reads events from the SPMC queue
- Processes events through the order book
- Writes snapshots to shared memory for consumers
- Usage: `./shm_reader <queue_name> <snapshot_shm_name>`

### 3. Consumers

#### Full Consumer (`snapshot_consumer.cc`)
- Displays detailed TOB and FOD snapshots
- Shows formatted order book depth
- Usage: `./snapshot_consumer <snapshot_shm_name> [consumer_id]`

#### Simple Consumer (`simple_consumer.cc`)
- Lightweight consumer showing only snapshot counts
- Minimal output for performance testing
- Usage: `./simple_consumer <snapshot_shm_name> [consumer_id]`

#### Monitor (`snapshot_monitor.cc`)
- Real-time statistics and monitoring
- Shows update rates, latency, and consumer activity
- Usage: `./snapshot_monitor <snapshot_shm_name>`

## Usage Examples

### Basic Setup
```bash
# Terminal 1: Start writer
./shm_writer myqueue

# Terminal 2: Start reader with snapshot shared memory
./shm_reader myqueue /snapshots

# Terminal 3: Start a consumer
./snapshot_consumer /snapshots consumer1

# Terminal 4: Start another consumer
./snapshot_consumer /snapshots consumer2

# Terminal 5: Start monitor
./snapshot_monitor /snapshots
```

### Using the Test Script
```bash
# Run the complete system test
./test_multi_consumer.sh
```

## Features

### Multi-Consumer Support
- Multiple consumers can connect to the same snapshot shared memory
- Each consumer tracks its own sequence numbers
- No message loss - all consumers receive all snapshots

### Performance Optimizations
- Lock-free atomic operations for sequence numbers
- Cache-aligned data structures
- Minimal memory copying
- Efficient polling with configurable sleep intervals

### Monitoring and Statistics
- Real-time update rates
- Consumer activity tracking
- Latency measurements
- Sequence number tracking

### Graceful Shutdown
- Signal handlers for clean termination
- Automatic consumer count tracking
- Shared memory cleanup

## Shared Memory Structure

```cpp
struct SnapshotSHM {
    // Header information
    std::atomic<uint64_t> write_seq{0};        // Sequence number for writes
    std::atomic<uint64_t> last_tob_seq{0};     // Last TOB snapshot sequence
    std::atomic<uint64_t> last_fod_seq{0};     // Last FOD snapshot sequence
    std::atomic<uint64_t> write_timestamp{0};  // Last write timestamp
    
    // Snapshot data
    SnapshotTOB latest_tob;                    // Latest TOB snapshot
    SnapshotFOD latest_fod;                    // Latest FOD snapshot
    
    // Consumer tracking
    std::atomic<uint32_t> active_consumers{0}; // Number of active consumers
    std::atomic<uint64_t> last_consumer_read{0}; // Last consumer read timestamp
};
```

## Building

```bash
./build.sh
```

This builds all components:
- `shm_writer` - Event writer
- `shm_reader` - Event reader with snapshot distribution
- `snapshot_consumer` - Full-featured consumer
- `simple_consumer` - Lightweight consumer
- `snapshot_monitor` - Monitoring tool

## Performance Characteristics

- **Latency**: Sub-microsecond snapshot distribution
- **Throughput**: Supports multiple consumers without degradation
- **Memory**: Fixed-size shared memory allocation
- **CPU**: Minimal polling overhead with configurable intervals

## Use Cases

1. **Market Data Distribution**: Distribute order book snapshots to multiple trading systems
2. **Risk Management**: Multiple risk engines consuming the same market data
3. **Analytics**: Real-time analytics consuming market data
4. **Monitoring**: Real-time monitoring and alerting systems
5. **Testing**: Multiple test consumers for validation

## Troubleshooting

### Common Issues

1. **Shared Memory Not Found**: Ensure the reader is running and has created the snapshot shared memory
2. **Permission Denied**: Check shared memory permissions and user access
3. **No Snapshots Received**: Verify the writer and reader are running and processing events
4. **High Latency**: Check system load and adjust polling intervals

### Debugging

- Use the monitor to check update rates and consumer activity
- Verify sequence numbers are incrementing
- Check shared memory exists: `ls -la /dev/shm/`
- Monitor system resources during operation 