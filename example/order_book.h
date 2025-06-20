#pragma once
#include <map>
#include <unordered_map>
#include <list>
#include "structs.h"
#include "trading_phases.h"

// OrderBook holds the state and logic for a single instrument's order book
class OrderBook {
public:
    // Side: 0 = bid, 1 = ask
    std::map<double, std::list<Event*>, std::greater<double>> bids;
    std::map<double, std::list<Event*>, std::less<double>> asks;
    std::unordered_map<uint64_t, Event*> order_map;

    uint32_t latest_seq = 0;
    uint64_t latest_timestamp = 0; // Track latest event timestamp
    double last_price = 0.0;
    uint64_t last_amount = 0; // Updated on every trade
    uint64_t volume = 0;
    int last_aggression = -1; // -1 = none, 0 = bid, 1 = ask
    uint16_t trading_phase = TradingPhase::CONTINUOUS_TRADING; // Default: continuous trading
    int last_order_level = 0; // Track the level of the last processed order
    int last_order_side = -1; // Track the side of the last processed order

    void set_trading_phase(uint16_t phase) { trading_phase = phase; }
    void process_event(Event* ev);
    void add_order(Event* ev);
    void cancel_order(Event* ev);
    void modify_order(Event* ev);

    // Returns true if the current trading phase allows book crossing
    bool is_cross_allowed() const;

    // Get the level of the last processed order
    int get_last_order_level() const { return last_order_level; }
    int get_last_order_side() const { return last_order_side; }
    
    // Check if the last order modified the visible book (first 10 levels)
    bool did_modify_visible_book() const;

    // Snapshots
    void snapshot_tob(SnapshotTOB& snap) const;
    void snapshot_fod(SnapshotFOD& snap) const;

private:
    // Helper function to calculate and display order level for any order event
    void calculate_and_display_level(int side, double price);
    
    // Returns true if the current trading phase allows book crossing
    bool trading_phase_allows_crossing() const;
}; 