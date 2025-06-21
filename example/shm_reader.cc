#include <bits/stdc++.h>
#include <cstring>
#include <iomanip>
#include <ctime>
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
              << ", phase=" << TradingPhase::get_phase_name(tob.trading_phase) << ", ts=" << tob.arrival_time << "; ";
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

void write_snapshots_to_shm(const OrderBook& ob, TOBSHM* tob_shm, FODSHM* fod_shm) {
    // Only publish snapshots if the order actually modified the visible book
    if (ob.did_modify_visible_book()) {
        int last_level = ob.get_last_order_level();
        
        // TOB snapshots: only when order affects top of book (level <= 0)
        if (last_level <= 0 && tob_shm) {
            ob.snapshot_tob(tob_shm->latest_snapshot);
            tob_shm->last_seq.fetch_add(1, std::memory_order_release);
        }
        
        // FOD snapshots: when order modifies the visible book (level <= 10)
        if (fod_shm) {
            ob.snapshot_fod(fod_shm->latest_snapshot);
            fod_shm->last_seq.fetch_add(1, std::memory_order_release);
        }
    }
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
              << ", ts=" << ns_to_utc_timestr(ev->arrival_time)
              << ", latency_ipc=" << ipc_latency << "ns"
              << std::endl;
}

// Recovery function: replay all messages from the beginning to sync with flow
void recover_from_beginning(Q* queue, OrderBook& ob, TOBSHM* tob_shm, FODSHM* fod_shm) {
    std::cout << "=== RECOVERY MODE: Replaying all messages from beginning ===" << std::endl;
    
    // Reset order book to clean state
    ob = OrderBook();
    ob.set_trading_phase(TradingPhase::CONTINUOUS_TRADING);
    
    // Get a fresh reader that starts from the beginning
    auto reader = queue->getReader();
    
    uint64_t recovered_count = 0;
    uint64_t last_seq = 0;
    
    // Read all available messages to rebuild order book state
    while (true) {
        const Event* ev = reader.read();
        if (!ev) {
            // No more messages to replay
            break;
        }
        
        // Process event to rebuild order book state
        ob.process_event(const_cast<Event*>(ev));
        last_seq = ev->seq;
        recovered_count++;
        
        // Print progress every 100 messages
        if (recovered_count % 100 == 0) {
            std::cout << "Recovery progress: " << recovered_count << " messages replayed, last_seq=" << last_seq << std::endl;
        }
    }
    
    std::cout << "=== RECOVERY COMPLETE: Replayed " << recovered_count << " messages, synced to seq=" << last_seq << " ===" << std::endl;
    
    // Publish current state to shared memory
    if (tob_shm) {
        ob.snapshot_tob(tob_shm->latest_snapshot);
        tob_shm->last_seq.fetch_add(1, std::memory_order_release);
    }
    
    if (fod_shm) {
        ob.snapshot_fod(fod_shm->latest_snapshot);
        fod_shm->last_seq.fetch_add(1, std::memory_order_release);
    }
    
    std::cout << "Current order book state published to shared memory" << std::endl;
}

// usage: ./shm_reader [queue_name] [tob_shm_name] [fod_shm_name] [--recover] [--check-gaps]
// use taskset -c to bind core
int main(int argc, char** argv) {
  if (argc < 4) {
    printf("usage: %s queue_name tob_shm_name fod_shm_name [--recover] [--check-gaps]\n", argv[0]);
    printf("  --recover: Force recovery mode (replay all messages from beginning)\n");
    printf("  --check-gaps: Enable automatic recovery on sequence gaps\n");
    return 1;
  }
  const char* qname = argv[1];
  const char* tob_name = argv[2];
  const char* fod_name = argv[3];
  
  bool force_recovery = false;
  bool check_gaps = false;
  
  // Parse command line options
  for (int i = 4; i < argc; i++) {
    if (strcmp(argv[i], "--recover") == 0) {
      force_recovery = true;
    } else if (strcmp(argv[i], "--check-gaps") == 0) {
      check_gaps = true;
    }
  }
  
  auto q = shmmap(qname);
  if (!q) {
    perror("failed to get queue");
    return 1;
  }
  auto reader = q->getReader();
  cout << "reader obtained" << endl;
  
  // Create separate TOB and FOD shared memory
  TOBSHM* tob_shm = create_tob_shm(tob_name);
  if (!tob_shm) {
    perror("failed to create TOB shared memory");
    return 1;
  }
  cout << "TOB shared memory created: " << tob_name << endl;
  
  FODSHM* fod_shm = create_fod_shm(fod_name);
  if (!fod_shm) {
    perror("failed to create FOD shared memory");
    return 1;
  }
  cout << "FOD shared memory created: " << fod_name << endl;
  
  OrderBook ob;
  ob.set_trading_phase(TradingPhase::CONTINUOUS_TRADING);
  std::unordered_map<uint32_t, uint64_t> last_seq;
  LatencyStats stats(10); // Print every 10 events
  
  // Force recovery if requested
  if (force_recovery) {
    recover_from_beginning(q, ob, tob_shm, fod_shm);
  }
  
  uint64_t expected_seq = 1; // Track expected sequence number
  
  while (true) {
    uint64_t read_time = get_ns_since_epoch();
    const Event* ev = reader.read();
    if (!ev) continue;
    
    // Check for sequence gaps if enabled
    if (check_gaps && ev->seq != expected_seq) {
      std::cout << "=== SEQUENCE GAP DETECTED: expected=" << expected_seq << ", got=" << ev->seq << " ===" << std::endl;
      std::cout << "Triggering automatic recovery..." << std::endl;
      recover_from_beginning(q, ob, tob_shm, fod_shm);
      expected_seq = ev->seq + 1; // Update expected sequence
      continue;
    }
    
    expected_seq = ev->seq + 1; // Update expected sequence for next iteration
    
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
    
    // Write snapshots to separate shared memory files for consumers
    write_snapshots_to_shm(ob, tob_shm, fod_shm);
    
    uint64_t t_end = get_ns_since_epoch();
    // Only print order book management time if snapshots were published
    if (tob_shm || fod_shm) {
        std::cout << "OrderBook management time: " << (t_end - start_book_ts) << " ns after snapshot publish" << std::endl;
    }
    // Future: write snapshot to another shared memory file for logic applications
  }

  return 0;
}


