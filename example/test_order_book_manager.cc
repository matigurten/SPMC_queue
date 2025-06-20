#include <iostream>
#include <vector>
#include "order_book.h"

void print_snapshots(OrderBook& ob, int step) {
    SnapshotTOB tob = {};
    ob.snapshot_tob(tob);
    std::cout << "\n[Step " << step << "] TOB Snapshot:\n";
    std::cout << "Best Bid: " << tob.bid_price << " x " << tob.bid_amount << " (" << tob.bid_orders << ")\n";
    std::cout << "Best Ask: " << tob.ask_price << " x " << tob.ask_amount << " (" << tob.ask_orders << ")\n";

    SnapshotFOD fod = {};
    ob.snapshot_fod(fod);
    std::cout << "FOD Snapshot (Bids):\n";
    for (int i = 0; i < 10; ++i) {
        if (fod.bids[i].amount > 0)
            std::cout << i << ": " << fod.bids[i].price << " x " << fod.bids[i].amount << " (" << fod.bids[i].orders << ")\n";
    }
    std::cout << "FOD Snapshot (Asks):\n";
    for (int i = 0; i < 10; ++i) {
        if (fod.asks[i].amount > 0)
            std::cout << i << ": " << fod.asks[i].price << " x " << fod.asks[i].amount << " (" << fod.asks[i].orders << ")\n";
    }
}

int main() {
    OrderBook ob;
    std::vector<Event> events;
    int step = 1;

    // Add multiple bid orders at the same price level
    events.push_back({1, 1, EVENT_TYPE_ADD, 0, 1, 0, 10, 100.0, 1, 0, 0, 0, 0}); // bid 100.0, id 1
    events.push_back({1, 2, EVENT_TYPE_ADD, 0, 1, 0, 20, 100.0, 2, 0, 0, 0, 0}); // bid 100.0, id 2
    events.push_back({1, 3, EVENT_TYPE_ADD, 0, 1, 0, 30, 99.0, 3, 0, 0, 0, 0});  // bid 99.0, id 3
    // Add multiple ask orders at the same price level
    events.push_back({1, 4, EVENT_TYPE_ADD, 1, 1, 0, 15, 102.0, 4, 0, 0, 0, 0}); // ask 102.0, id 4
    events.push_back({1, 5, EVENT_TYPE_ADD, 1, 1, 0, 25, 102.0, 5, 0, 0, 0, 0}); // ask 102.0, id 5
    events.push_back({1, 6, EVENT_TYPE_ADD, 1, 1, 0, 25, 103.0, 6, 0, 0, 0, 0}); // ask 103.0, id 6

    std::cout << "== After adding multiple orders at same price levels ==\n";
    for (auto& ev : events) ob.process_event(&ev);
    print_snapshots(ob, step++);

    // Cancel one bid at 100.0 (id 1)
    Event cancel_bid1 = {1, 7, EVENT_TYPE_CANCEL, 0, 1, 0, 0, 100.0, 1, 0, 0, 0, 0};
    ob.process_event(&cancel_bid1);
    std::cout << "\n== After cancelling bid id 1 at 100.0 ==\n";
    print_snapshots(ob, step++);

    // Cancel the remaining bid at 100.0 (id 2)
    Event cancel_bid2 = {1, 8, EVENT_TYPE_CANCEL, 0, 1, 0, 0, 100.0, 2, 0, 0, 0, 0};
    ob.process_event(&cancel_bid2);
    std::cout << "\n== After cancelling bid id 2 at 100.0 (should remove price level 100.0) ==\n";
    print_snapshots(ob, step++);

    // Cancel one ask at 102.0 (id 4)
    Event cancel_ask4 = {1, 9, EVENT_TYPE_CANCEL, 1, 1, 0, 0, 102.0, 4, 0, 0, 0, 0};
    ob.process_event(&cancel_ask4);
    std::cout << "\n== After cancelling ask id 4 at 102.0 ==\n";
    print_snapshots(ob, step++);

    // Cancel the remaining ask at 102.0 (id 5)
    Event cancel_ask5 = {1, 10, EVENT_TYPE_CANCEL, 1, 1, 0, 0, 102.0, 5, 0, 0, 0, 0};
    ob.process_event(&cancel_ask5);
    std::cout << "\n== After cancelling ask id 5 at 102.0 (should remove price level 102.0) ==\n";
    print_snapshots(ob, step++);

    // Add more at same price to test re-population
    Event add_bid4 = {1, 11, EVENT_TYPE_ADD, 0, 1, 0, 40, 99.0, 7, 0, 0, 0, 0}; // bid 99.0, id 7
    Event add_bid5 = {1, 12, EVENT_TYPE_ADD, 0, 1, 0, 50, 99.0, 8, 0, 0, 0, 0}; // bid 99.0, id 8
    ob.process_event(&add_bid4);
    ob.process_event(&add_bid5);
    std::cout << "\n== After adding two more bids at 99.0 ==\n";
    print_snapshots(ob, step++);

    // Cancel all bids at 99.0 (id 3, 7, 8)
    Event cancel_bid3 = {1, 13, EVENT_TYPE_CANCEL, 0, 1, 0, 0, 99.0, 3, 0, 0, 0, 0};
    Event cancel_bid7 = {1, 14, EVENT_TYPE_CANCEL, 0, 1, 0, 0, 99.0, 7, 0, 0, 0, 0};
    Event cancel_bid8 = {1, 15, EVENT_TYPE_CANCEL, 0, 1, 0, 0, 99.0, 8, 0, 0, 0, 0};
    ob.process_event(&cancel_bid3);
    ob.process_event(&cancel_bid7);
    ob.process_event(&cancel_bid8);
    std::cout << "\n== After cancelling all bids at 99.0 (should remove all bids) ==\n";
    print_snapshots(ob, step++);

    return 0;
} 