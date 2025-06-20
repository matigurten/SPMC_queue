#include <iostream>
#include <random>
#include <chrono>
#include <thread>
#include <cstring>
#include "../SPMCQueue.h"
#include "structs.h"
#include "shm.h"

using Q = SPMCQueue<Event, 1024>;

constexpr double PRICE_STEP = 0.1; // Price step for rounding

// usage: ./shm_write [shm file]
// use taskset -c to bind core
int main(int argc, char** argv) {
  if (argc != 2) {
    printf("usage: %s qname\n", argv[0]);
    return 1;
  }
  const char* qname = argv[1];
  auto q = shmmap(qname);
  if (!q) {
    perror("failed to create queue");
    return 1;
  }
  
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> side_dist(0, 1);
  std::bernoulli_distribution add_dist(0.8); // 80% adds, 20% cancels
  std::uniform_int_distribution<> amount_dist(1, 100);
  std::uniform_real_distribution<> price_offset_dist(0.1, 3.0); // Reduced range for more overlap
  std::uniform_real_distribution<> crossing_dist(0.0, 1.0); // For occasional crossing orders
  
  uint64_t seq = 1;
  while (true) {
    q->write([&](Event& msg) {
      // Calculate parsing latency and set timestamps
      msg.arrival_time = get_ns_since_epoch();

      // ... simulate some work ...
      std::this_thread::sleep_for(std::chrono::nanoseconds(10)); // Placeholder for actual work

      msg.write_time = get_ns_since_epoch();
      msg.parsing_latency = msg.write_time - msg.arrival_time;

      // Set other fields
      msg.instrument_id = 1;
      msg.seq = seq;
      msg.event_type = add_dist(gen) ? EVENT_TYPE_ADD : EVENT_TYPE_CANCEL;
      msg.side = side_dist(gen);
      msg.order_type = 1;
      msg.amount = amount_dist(gen);
      
      // Generate prices with some overlap for potential trades
      double base_price = 100.0;
      double offset = price_offset_dist(gen);
      
      if (msg.side == 0) { // Bid
          // Bids: 97.0 to 100.5 (allowing some to cross above 100.0)
          double raw_price = base_price - offset + (crossing_dist(gen) * 0.5);
          msg.price = std::round(raw_price / PRICE_STEP) * PRICE_STEP;
      } else { // Ask
          // Asks: 99.5 to 103.0 (allowing some to cross below 100.0)
          double raw_price = base_price + offset - (crossing_dist(gen) * 0.5);
          msg.price = std::round(raw_price / PRICE_STEP) * PRICE_STEP;
      }
      msg.order_id = seq; // Use seq as order_id for simplicity
      msg.other_id = 0;
    });
    seq++;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }

  return 0;
}

