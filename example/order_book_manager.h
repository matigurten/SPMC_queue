#pragma once
#include <map>
#include <unordered_map>
#include <list>
#include "structs.h"

class order_book_manager {
public:
    // Side: 0 = bid, 1 = ask
    std::map<double, std::list<Event*>, std::greater<double>> bids;
    std::map<double, std::list<Event*>, std::less<double>> asks;
    std::unordered_map<uint64_t, Event*> order_map;

    void process_event(Event* ev);
    void add_order(Event* ev);
    void cancel_order(Event* ev);
    void modify_order(Event* ev);

    // Snapshots
    void snapshot_tob(SnapshotTOB& snap, uint32_t instrument_id, uint32_t seq, uint64_t timestamp, uint16_t trading_phase, double last_price, uint64_t volume);
    void snapshot_fod(SnapshotFOD& snap, uint32_t instrument_id, uint32_t seq, uint64_t timestamp, uint8_t trading_phase, double last_price, uint64_t volume);
}; 