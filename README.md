# SPMC Queue - High-Performance Order Book Simulation

A high-performance C++ Single Producer Multiple Consumer (SPMC) queue system designed for order book simulation and market data event handling using shared memory.

## 🚀 Features

- **High-performance shared memory SPMC queue** with lock-free design
- **Real-time order book management** with crossing logic and trade execution
- **TOB (Top of Book) and FOD (Full Order Depth) snapshots** with configurable publishing
- **Multiple consumer support** for distributed processing
- **Latency monitoring and statistics** with nanosecond precision
- **Trading phase management** with human-readable timestamps
- **Parquet data export** for research and machine learning
- **Comprehensive testing and debugging tools**

## 📁 Project Structure

```
SPMC_Queue/
├── SPMCQueue.h              # Core SPMC queue implementation
├── example/
│   ├── build.sh             # Build script for all components
│   ├── shm_writer.cc        # Event producer/writer
│   ├── shm_reader.cc        # Event consumer with order book management
│   ├── order_book.cc        # Order book implementation
│   ├── order_book_manager.cc # Order book manager with crossing logic
│   ├── snapshot_consumer.cc # Snapshot consumer for TOB/FOD data
│   ├── simple_consumer.cc   # Simple event consumer
│   ├── snapshot_monitor.cc  # Snapshot monitoring tool
│   ├── fod_parquet_writer.cc # Parquet data export
│   ├── test_fod_reader.cc   # FOD debugging tool
│   ├── structs.h            # Market data event structures
│   ├── shm.h                # Shared memory definitions
│   ├── snapshot_shm.h       # Snapshot shared memory structures
│   ├── latency_stats.h      # Latency monitoring utilities
│   ├── logger.h             # Logging utilities
│   └── trading_phases.h     # Trading phase constants
```

## 🛠️ Building

### Prerequisites
- Linux/WSL environment (for shared memory support)
- GCC compiler with C++17 support
- POSIX shared memory libraries (`-lrt`)

### Build Commands
```bash
cd example
./build.sh
```

This builds all components:
- `shm_writer` - Event producer
- `shm_reader` - Event consumer with order book
- `snapshot_consumer` - Snapshot consumer
- `simple_consumer` - Simple event consumer
- `snapshot_monitor` - Monitoring tool
- `fod_parquet_writer` - Parquet export
- `test_fod_reader` - Debugging tool

## 🏃‍♂️ Running

### Basic Usage

1. **Start the writer** (produces market events):
   ```bash
   ./shm_writer myqueue
   ```

2. **Start the reader** (processes events, manages order book):
   ```bash
   ./shm_reader myqueue /snapshots
   ```

3. **Start snapshot consumers** (consume TOB/FOD snapshots):
   ```bash
   ./snapshot_consumer /snapshots consumer1
   ./snapshot_consumer /snapshots consumer2
   ```

### Advanced Usage

**Multiple consumers with different levels:**
```bash
./snapshot_consumer /snapshots consumer1 4  # Show 4 levels
./snapshot_consumer /snapshots consumer2 8  # Show 8 levels
```

**Monitor snapshots:**
```bash
./snapshot_monitor /snapshots
```

**Export to Parquet:**
```bash
./fod_parquet_writer /snapshots
```

## 📊 Key Components

### Event Types
- **Add Order**: Add new order to book
- **Replace Order**: Modify existing order
- **Remove Order**: Cancel order
- **Trade**: Executed trade

### Order Book Features
- **Price-time priority** order matching
- **Crossing logic** for immediate execution
- **Volume management** with partial fills
- **Level calculation** for order positioning
- **Snapshot publishing** with configurable triggers

### Performance Features
- **Lock-free shared memory** communication
- **Cache-aligned data structures** for optimal performance
- **Nanosecond precision** latency measurement
- **Busy-waiting consumers** for minimal latency
- **Configurable snapshot filtering** to reduce data volume

## 🔧 Configuration

### Snapshot Publishing Rules
- **TOB snapshots**: Published when order level ≤ 0 (affects visible book)
- **FOD snapshots**: Published when order modifies managed price layers
- **Latency tracking**: End-to-end latency from writer to consumer

### Shared Memory Configuration
- **Event queue**: `/myqueue` (configurable name)
- **Snapshot queue**: `/snapshots` (configurable name)
- **Queue size**: Configurable for different throughput requirements

## 📈 Performance Characteristics

- **Ultra-low latency**: Sub-microsecond shared memory communication
- **High throughput**: Millions of events per second
- **Scalable**: Multiple consumers without performance degradation
- **Real-time**: Immediate order book updates and trade execution

## 🧪 Testing and Debugging

### Test Programs
- `test_fod_reader`: Debug FOD snapshot consumption
- `snapshot_monitor`: Monitor snapshot publishing
- `simple_consumer`: Basic event consumption

### Debugging Tips
1. Start programs in order: writer → reader → consumers
2. Use `snapshot_monitor` to verify shared memory creation
3. Check latency statistics for performance issues
4. Verify order book state with TOB snapshots

## 🤝 Contributing

This project is designed for high-frequency trading and market data processing. Contributions are welcome for:
- Performance optimizations
- Additional order types
- Enhanced monitoring tools
- Platform compatibility improvements

## 📄 License

See [LICENSE](LICENSE) file for details.

## 🔗 Related Projects

- [CronichleQueue](https://github.com/matigurten/CronichleQueue) - Related queue implementation
- [algoDealer](https://github.com/matigurten/algoDealer) - Algorithmic trading framework

---

**Built for high-performance trading systems** 🚀
