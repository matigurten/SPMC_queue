#pragma once
#include <cstdint>

// Event type constants (based on typical market data event types)
constexpr uint8_t EVENT_TYPE_ADD    = 1; // New order
constexpr uint8_t EVENT_TYPE_CANCEL = 2; // Cancel order
constexpr uint8_t EVENT_TYPE_MODIFY = 3; // Modify/replace order
constexpr uint8_t EVENT_TYPE_TRADE  = 4; // Trade (execution)
// Add more as needed for your event stream

// Event struct definition (formerly Message)
#pragma pack(push, 1)
struct Event {
    uint32_t instrument_id;   // 4 bytes
    uint32_t seq;             // 4 bytes
    uint8_t event_type;       // 1 byte
    uint8_t side;             // 1 byte
    uint8_t order_type;       // 1 byte
    uint8_t _pad1;            // 1 byte padding
    uint32_t amount;          // 4 bytes
    double price;             // 8 bytes
    uint64_t order_id;        // 8 bytes
    uint64_t other_id;        // 8 bytes (was order_id2)
    uint64_t write_time;        // 8 bytes (was gTs)
    uint64_t parsing_latency;   // 8 bytes (was ltcy)
    uint64_t arrival_time;    // 8 bytes (was arrivalTime)
};
#pragma pack(pop)
static_assert(sizeof(Event) == 64, "Event should be 64 bytes");

#pragma pack(push, 1)
struct PriceLevel {
    double price;     // 8 bytes
    uint32_t amount;  // 4 bytes
    uint16_t orders;  // 2 bytes
    uint16_t _pad;    // 2 bytes for alignment
};
#pragma pack(pop)
static_assert(sizeof(PriceLevel) == 16, "PriceLevel must be 16 bytes");

#pragma pack(push, 1)
struct alignas(64) SnapshotTOB {
    uint32_t instrument_id;   // 4 bytes
    uint32_t seq;             // 4 bytes
    uint64_t timestamp;       // 8 bytes
    double bid_price;         // 8 bytes
    double ask_price;         // 8 bytes
    uint32_t bid_amount;      // 4 bytes
    uint32_t ask_amount;      // 4 bytes
    uint16_t bid_orders;      // 2 bytes
    uint16_t ask_orders;      // 2 bytes

    uint8_t trading_phase;    // 1 byte
    int8_t side_changed;      // 1 byte
    int8_t level_changed;     // 1 byte

    uint8_t last_aggression;  // 1 byte
    double last_price;        // 8 bytes
    uint64_t volume;          // 8 bytes
};
#pragma pack(pop)
static_assert(sizeof(SnapshotTOB) == 64, "SnapshotTOB should be 64 bytes");

#pragma pack(push, 1)
struct alignas(64) SnapshotFOD {
    uint32_t instrument_id;   // 4 bytes
    uint32_t seq;             // 4 bytes
    uint64_t timestamp;       // 8 bytes
    uint16_t trading_phase;   // 2 bytes
    uint8_t last_aggression;  // 1 byte
    int8_t side_changed;      // 1 byte
    int8_t level_changed;     // 1 byte
    uint8_t _pad[1];          // 1 byte padding for alignment
    double last_price;        // 8 bytes
    uint64_t volume;          // 8 bytes
    PriceLevel bids[10];      // 160 bytes
    PriceLevel asks[10];      // 160 bytes
    uint8_t _pad2[22];        // 22 bytes (reduced from 24 to accommodate new fields)
};
#pragma pack(pop)
static_assert(sizeof(SnapshotFOD) == 384, "SnapshotFOD should be 384 bytes");

// Add other structures here as needed 