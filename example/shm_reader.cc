#include <bits/stdc++.h>
#include <cstring>
#include <iomanip>
#include "../SPMCQueue.h"
#include "structs.h"
#include "order_book.h"
#include "trading_phases.h"
#include "snapshot_shm.h"
#include <unordered_map>
#include <vector>
#include <algorithm>
#include "latency_stats.h"
#include "shm.h"

using Q = SPMCQueue<Event, 1024>;

using namespace std;

void print_snapshots(const OrderBook& ob) {
    SnapshotTOB tob = {};
    ob.snapshot_tob(tob);
    std::cout << "[Seq " << tob.seq << "] ";
    std::cout << "Last: price=" << tob.last_price
              << ", amount=" << ob.last_amount
              << ", aggression=" << ob.last_aggression
              << ", volume=" << tob.volume
              << ", phase=" << TradingPhase::get_phase_name(tob.trading_phase) << ", ts=" << tob.timestamp << "; ";
    std::cout << "Bids: " << tob.bid_price << " x " << tob.bid_amount << " (" << tob.bid_orders << ") | ";
    std::cout << "Asks: " << tob.ask_price << " x " << tob.ask_amount << " (" << tob.ask_orders << ")\n";

    SnapshotFOD fod = {};
    ob.snapshot_fod(fod);
    
    // Print FOD with proper formatting
    std::cout << "\n=== FULL ORDER DEPTH ===\n";
    std::cout << "Level |    Bids     |    Asks     |\n";
    std::cout << "      | Price x Qty | Price x Qty |\n";
    std::cout << "------|-------------|-------------|\n";
    
    for (int i = 0; i < 5; ++i) {  // Show top 5 levels
        std::cout << std::setw(5) << i << " | ";
        
        // Print bid level
        if (fod.bids[i].amount > 0) {
            std::cout << std::fixed << std::setprecision(1) 
                      << std::setw(6) << fod.bids[i].price << " x" 
                      << std::setw(4) << fod.bids[i].amount;
        } else {
            std::cout << "         ";
        }
        
        std::cout << " | ";
        
        // Print ask level
        if (fod.asks[i].amount > 0) {
            std::cout << std::fixed << std::setprecision(1) 
                      << std::setw(6) << fod.asks[i].price << " x" 
                      << std::setw(4) << fod.asks[i].amount;
        } else {
            std::cout << "         ";
        }
        
        std::cout << " |\n";
    }
    std::cout << "========================\n\n";
}

void write_snapshots_to_shm(const OrderBook& ob, SnapshotSHM* snapshot_shm) {
    if (!snapshot_shm) return;
    
    // Update TOB snapshot
    ob.snapshot_tob(snapshot_shm->latest_tob);
    snapshot_shm->last_tob_seq.fetch_add(1, std::memory_order_release);
    
    // Update FOD snapshot
    ob.snapshot_fod(snapshot_shm->latest_fod);
    snapshot_shm->last_fod_seq.fetch_add(1, std::memory_order_release);
    
    // Update write sequence and timestamp
    snapshot_shm->write_seq.fetch_add(1, std::memory_order_release);
    snapshot_shm->write_timestamp.store(get_ns_since_epoch(), std::memory_order_relaxed);
}

void print_event(const Event* ev, uint64_t read_time) {
    uint64_t ipc_latency = read_time - ev->write_time;
    std::cout << "INCOMING: inst=" << ev->instrument_id
              << ", seq=" << ev->seq
              << ", type=" << (int)ev->event_type
              << ", side=" << (int)ev->side
              << ", ord_type=" << (int)ev->order_type
              << ", amount=" << ev->amount
              << ", price=" << ev->price
              << ", order_id=" << ev->order_id
              << ", other_id=" << ev->other_id
              << ", latency_ipc=" << ipc_latency << "ns"
              << std::endl;
}

// usage: ./shm_reader [queue_name] [snapshot_shm_name]
// use taskset -c to bind core
int main(int argc, char** argv) {
  if (argc < 3) {
    printf("usage: %s queue_name snapshot_shm_name\n", argv[0]);
    return 1;
  }
  const char* qname = argv[1];
  const char* snapshot_name = argv[2];
  
  auto q = shmmap(qname);
  if (!q) {
    perror("failed to get queue");
    return 1;
  }
  auto reader = q->getReader();
  cout << "reader obtained" << endl;
  
  // Create snapshot shared memory
  SnapshotSHM* snapshot_shm = create_snapshot_shm(snapshot_name);
  if (!snapshot_shm) {
    perror("failed to create snapshot shared memory");
    return 1;
  }
  cout << "snapshot shared memory created: " << snapshot_name << endl;
  
  OrderBook ob;
  ob.set_trading_phase(TradingPhase::CONTINUOUS_TRADING);
  std::unordered_map<uint32_t, uint64_t> last_seq;
  LatencyStats stats(10); // Print every 10 events
  while (true) {
    uint64_t read_time = get_ns_since_epoch();
    const Event* ev = reader.read();
    if (!ev) continue;
    
    print_event(ev, read_time);

    // Latency calculations (do not modify Event struct)
    uint64_t ipc_latency = read_time - ev->write_time;
    uint64_t end2end_latency = read_time - ev->arrival_time;

    // Pass the correct latencies
    stats.add(ev->parsing_latency, ipc_latency, end2end_latency);

    // Measure order book processing time
    uint64_t start_book_ts = get_ns_since_epoch();
    ob.process_event(const_cast<Event*>(ev));
    print_snapshots(ob);
    
    // Write snapshots to shared memory for consumers
    write_snapshots_to_shm(ob, snapshot_shm);
    
    uint64_t t_end = get_ns_since_epoch();
    // Latency calculations (do not modify Event struct)
    uint64_t parsing_latency = ev->parsing_latency;
    std::cout << "OrderBook management time: " << (t_end - start_book_ts) << " ns" << std::endl;
    // Future: write snapshot to another shared memory file for logic applications
  }

  return 0;
}


