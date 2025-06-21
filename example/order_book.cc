#include "order_book.h"
#include "shm.h"
#include <iostream>
#include <cmath>

constexpr double PRICE_STEP = 0.1; // Adjust as needed

// Returns true if the current trading phase allows book crossing
bool OrderBook::is_cross_allowed() const {
    return TradingPhase::is_crossing_allowed(trading_phase);
}

// Helper: Calculate the level of an incoming order
int calc_order_level(int side, double price, const std::map<double, std::list<Event*>, std::greater<double>>& bids,
                    const std::map<double, std::list<Event*>, std::less<double>>& asks) {
    if (side == 0) { // Bid
        if (bids.empty()) return 0; // No TOB, treat as level 0
        double best_bid = bids.begin()->first;
        return static_cast<int>(std::round((best_bid - price) / PRICE_STEP));
    } else { // Ask
        if (asks.empty()) return 0; // No TOB, treat as level 0
        double best_ask = asks.begin()->first;
        return static_cast<int>(std::round((price - best_ask) / PRICE_STEP));
    }
}

void OrderBook::process_event(Event* ev) {
    latest_seq = ev->seq;
    latest_timestamp = ev->arrival_time; // Store latest event timestamp
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
            // For a trade event, update last_price, volume, last_aggression, last_amount
            last_price = ev->price;
            last_amount = ev->amount;
            volume += ev->amount;
            last_aggression = ev->side; // 0=bid, 1=ask
            break;
    }
    // If you want to update last_price/volume on add/cancel/modify, add logic here
}

// Helper function to calculate and display order level for any order event
void OrderBook::calculate_and_display_level(int side, double price) {
    int level = 0;
    if ((side == 0 && bids.empty()) || (side == 1 && asks.empty())) {
        std::cout << "Order level: NA (empty book)\n";
        level = 0;
    } else {
        level = calc_order_level(side, price, bids, asks);
        std::cout << "Order level: " << level << std::endl;
    }
    last_order_level = level;
    last_order_side = side;
}

// Add order with crossing logic and trade printing
void OrderBook::add_order(Event* ev) {
    // Calculate and display order level
    calculate_and_display_level(ev->side, ev->price);
    
    // If any amount remains, add to book
    if (ev->amount > 0) {
        // Handle crossing orders by executing trades
        if (ev->side == 0) { // Bid
            // Check if this bid crosses with existing asks
            while (!asks.empty() && ev->price >= asks.begin()->first && ev->amount > 0) {
                auto& ask_list = asks.begin()->second;
                while (!ask_list.empty() && ev->amount > 0) {
                    Event* resting = ask_list.front();
                    uint64_t trade_amt = std::min(ev->amount, resting->amount);
                    
                    // Execute the trade
                    std::cout << "TRADE: price=" << resting->price << ", amount=" << trade_amt
                              << ", aggressor=BUY, resting_order_id=" << resting->order_id
                              << ", incoming_order_id=" << ev->order_id << std::endl;
                    
                    // Update book state
                    last_price = resting->price;
                    last_amount = trade_amt;
                    last_aggression = 0; // BUY is aggressive
                    volume += trade_amt;
                    
                    // Update orders
                    ev->amount -= trade_amt;
                    resting->amount -= trade_amt;
                    
                    if (resting->amount == 0) {
                        order_map.erase(resting->order_id);
                        ask_list.pop_front();
                    }
                }
                if (ask_list.empty()) asks.erase(asks.begin());
            }
        } else { // Ask
            // Check if this ask crosses with existing bids
            while (!bids.empty() && ev->price <= bids.begin()->first && ev->amount > 0) {
                auto& bid_list = bids.begin()->second;
                while (!bid_list.empty() && ev->amount > 0) {
                    Event* resting = bid_list.front();
                    uint64_t trade_amt = std::min(ev->amount, resting->amount);
                    
                    // Execute the trade
                    std::cout << "TRADE: price=" << resting->price << ", amount=" << trade_amt
                              << ", aggressor=SELL, resting_order_id=" << resting->order_id
                              << ", incoming_order_id=" << ev->order_id << std::endl;
                    
                    // Update book state
                    last_price = resting->price;
                    last_amount = trade_amt;
                    last_aggression = 1; // SELL is aggressive
                    volume += trade_amt;
                    
                    // Update orders
                    ev->amount -= trade_amt;
                    resting->amount -= trade_amt;
                    
                    if (resting->amount == 0) {
                        order_map.erase(resting->order_id);
                        bid_list.pop_front();
                    }
                }
                if (bid_list.empty()) bids.erase(bids.begin());
            }
        }
        
        // Add remaining amount to book if any
        if (ev->amount > 0) {
            if (ev->side == 0) {
                bids[ev->price].push_back(ev);
            } else {
                asks[ev->price].push_back(ev);
            }
            order_map[ev->order_id] = ev;
        }
    }
}

void OrderBook::cancel_order(Event* ev) {
    // Calculate and display order level for the cancel event
    calculate_and_display_level(ev->side, ev->price);
    
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
    // Calculate and display order level for the modify event
    calculate_and_display_level(ev->side, ev->price);
    
    // Remove old, add new (if price/side changed)
    cancel_order(ev);
    add_order(ev);
}

void OrderBook::snapshot_tob(SnapshotTOB& snap) const {
    snap.instrument_id = 1; // TODO: track instrument_id if needed
    snap.seq = latest_seq;
    snap.arrival_time = latest_timestamp;
    snap.trading_phase = trading_phase;
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
    // Set new fields
    snap.level_changed = last_order_level;
    snap.side_changed = last_order_side;
}

void OrderBook::snapshot_fod(SnapshotFOD& snap) const {
    snap.instrument_id = 1; // TODO: track instrument_id if needed
    snap.seq = latest_seq;
    snap.arrival_time = latest_timestamp;
    snap.trading_phase = trading_phase;
    snap.last_price = last_price;
    snap.volume = volume;
    snap.level_changed = last_order_level;
    snap.side_changed = last_order_side;
    snap.publish_time = get_ns_since_epoch(); // Set publish time when snapshot is created
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

// Check if the last order modified the visible book (first 10 levels)
bool OrderBook::did_modify_visible_book() const {
    // If the order level is <= 10, it modified the visible book
    return last_order_level <= 10;
} 