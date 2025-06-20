#include "order_book_manager.h"
#include "order_book.h"

/*
Order Book Management Methods: Time and Space Complexities

Current method: std::map<double, std::list<Event*>> for price levels (bids/asks), std::unordered_map<order_id, Event*> for order lookup.

Operation Complexities (Map/List + Hash):
  - Add order:      O(log P) + O(1)   (log P for price level insert/find, O(1) for list append/hash insert)
  - Cancel order:   O(1) + O(log P)   (O(1) hash lookup, O(1) list remove if pointer known, log P for erase)
  - Modify order:   O(1) + O(log P)   (Same as cancel + add)
  - Best bid/ask:   O(1)              (map::begin() for best price level)
  - Snapshot (top N): O(N)            (Iterate over N price levels)
  - Space:          O(O+P)            (O = #orders, P = #price levels)

Other common methods:
- Array/Vector Indexed by Price (Dense Book):
    Add/Cancel/Modify: O(1), Best bid/ask: O(P) unless extra tracking, Snapshot: O(P), Space: O(M) (M = max price * multiplier)
- Flat Array (very small price ranges):
    Add/Cancel/Modify: O(1), Best bid/ask: O(1) with heap/tracking, Snapshot: O(N) or O(P), Space: O(M)
- Skip List/Custom Tree:
    Add/Cancel/Modify: O(log P), Best bid/ask: O(1), Snapshot: O(N), Space: O(O+P)
- Heap (for best price only):
    Add/Cancel: O(log P), Best bid/ask: O(1), Snapshot: O(N log P), Space: O(O+P)

In practice, the map/list + hash method is widely used for high-performance, sparse order books.
*/

// ---- OrderBook Implementation ----

void OrderBook::process_event(Event* ev) {
    latest_seq = ev->seq;
    switch (ev->event_type) {
        case EVENT_TYPE_ADD:
            add_order(ev);
            break;
        case EVENT_TYPE_CANCEL:
            cancel_order(ev);
            break;
        case EVENT_TYPE_MODIFY:
            modify_order(ev);
            break;
        case EVENT_TYPE_TRADE:
            // For a trade event, update last_price, volume, last_aggression
            last_price = ev->price;
            volume += ev->amount;
            last_aggression = ev->side; // 0=bid, 1=ask
            break;
    }
    // If you want to update last_price/volume on add/cancel/modify, add logic here
}

void OrderBook::add_order(Event* ev) {
    if (ev->side == 0) {
        bids[ev->price].push_back(ev);
    } else {
        asks[ev->price].push_back(ev);
    }
    order_map[ev->order_id] = ev;
}

void OrderBook::cancel_order(Event* ev) {
    auto it = order_map.find(ev->order_id);
    if (it == order_map.end()) return;
    Event* orig = it->second;
    if (orig->side == 0) {
        auto pit = bids.find(orig->price);
        if (pit != bids.end()) {
            pit->second.remove(orig);
            if (pit->second.empty()) bids.erase(pit);
        }
    } else {
        auto pit = asks.find(orig->price);
        if (pit != asks.end()) {
            pit->second.remove(orig);
            if (pit->second.empty()) asks.erase(pit);
        }
    }
    order_map.erase(it);
    // Do not delete orig, as memory is managed elsewhere
}

void OrderBook::modify_order(Event* ev) {
    // Remove old, add new (if price/side changed)
    cancel_order(ev);
    add_order(ev);
}

void OrderBook::snapshot_tob(SnapshotTOB& snap) const {
    snap.instrument_id = 1; // TODO: track instrument_id if needed
    snap.seq = latest_seq;
    snap.timestamp = 0; // TODO: set to latest event timestamp if needed
    snap.trading_phase = 0; // TODO: track trading phase if needed
    snap.last_price = last_price;
    snap.volume = volume;
    // Best bid
    if (!bids.empty() && !bids.begin()->second.empty()) {
        snap.bid_price = bids.begin()->first;
        snap.bid_amount = 0;
        snap.bid_orders = 0;
        for (auto* ev : bids.begin()->second) {
            snap.bid_amount += ev->amount;
            snap.bid_orders++;
        }
    } else {
        snap.bid_price = 0;
        snap.bid_amount = 0;
        snap.bid_orders = 0;
    }
    // Best ask
    if (!asks.empty() && !asks.begin()->second.empty()) {
        snap.ask_price = asks.begin()->first;
        snap.ask_amount = 0;
        snap.ask_orders = 0;
        for (auto* ev : asks.begin()->second) {
            snap.ask_amount += ev->amount;
            snap.ask_orders++;
        }
    } else {
        snap.ask_price = 0;
        snap.ask_amount = 0;
        snap.ask_orders = 0;
    }
}

void OrderBook::snapshot_fod(SnapshotFOD& snap) const {
    snap.instrument_id = 1; // TODO: track instrument_id if needed
    snap.seq = latest_seq;
    snap.timestamp = 0; // TODO: set to latest event timestamp if needed
    snap.trading_phase = 0; // TODO: track trading phase if needed
    snap.last_price = last_price;
    snap.volume = volume;
    // Top 10 bids
    int i = 0;
    for (auto it = bids.begin(); it != bids.end() && i < 10; ++it, ++i) {
        snap.bids[i].price = it->first;
        snap.bids[i].amount = 0;
        snap.bids[i].orders = 0;
        for (auto* ev : it->second) {
            snap.bids[i].amount += ev->amount;
            snap.bids[i].orders++;
        }
    }
    for (; i < 10; ++i) {
        snap.bids[i].price = 0;
        snap.bids[i].amount = 0;
        snap.bids[i].orders = 0;
    }
    // Top 10 asks
    i = 0;
    for (auto it = asks.begin(); it != asks.end() && i < 10; ++it, ++i) {
        snap.asks[i].price = it->first;
        snap.asks[i].amount = 0;
        snap.asks[i].orders = 0;
        for (auto* ev : it->second) {
            snap.asks[i].amount += ev->amount;
            snap.asks[i].orders++;
        }
    }
    for (; i < 10; ++i) {
        snap.asks[i].price = 0;
        snap.asks[i].amount = 0;
        snap.asks[i].orders = 0;
    }
}

// (No OrderBook logic here. Add OrderBookManager static utilities as needed.) 