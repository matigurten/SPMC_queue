#include <bits/stdc++.h>
#include "structs.h"
#include "shm.h"
using namespace std;

// usage: ./shm_write [shm file]
// use taskset -c to bind core
int main(int argc, char** argv) {
  const char* shm_file = "SPMCQueue_test";
  if (argc >= 2) {
    shm_file = argv[1];
  }
  Q* q = shmmap(shm_file);
  if (!q) return 1;

  std::random_device rd;
  std::mt19937 gen(rd());
  std::exponential_distribution<double> exp_amount_dist(1.0 / 20.0); // mean ~20
  std::normal_distribution<double> normal_price_dist(150.0, 5.0); // mean 150, stddev 5
  std::uniform_int_distribution<int> side_dist(0, 1); // 0 or 1
  double price_step = 0.1;
  double price_min = 100.0;
  double price_max = 200.0;
  std::uniform_int_distribution<uint32_t> gTm_dist(100, 5000); // gateway to matching time: 0.1us to 5us
  std::uniform_int_distribution<uint32_t> mTs_dist(50, 2000);  // matching to sending time: 0.05us to 2us

  int64_t seq = 0;
  while (true) {
    q->write([seq, &gen, &exp_amount_dist, &normal_price_dist, &side_dist, price_step, price_min, price_max, &gTm_dist, &mTs_dist](Event& msg) {
      msg.instrument_id = 1234; // integer instrument_id
      msg.seq = seq;
      msg.event_type = 1;
      msg.side = static_cast<uint8_t>(side_dist(gen));
      msg.order_type = 1;
      // Exponential distribution for amount
      double raw_amount = exp_amount_dist(gen);
      uint32_t amount = static_cast<uint32_t>(std::round(raw_amount));
      if (amount < 1) amount = 1;
      if (amount > 100) amount = 100;
      msg.amount = amount;
      // Normal distribution for price, clamped and rounded
      double raw_price = normal_price_dist(gen);
      if (raw_price < price_min) raw_price = price_min;
      if (raw_price > price_max) raw_price = price_max;
      msg.price = std::round(raw_price / price_step) * price_step;
      msg.order_id = 1000000ULL + seq;
      msg.order_id2 = 2000000ULL + seq;
      msg.gTm = gTm_dist(gen);
      msg.mTs = mTs_dist(gen);

      uint64_t now = get_ns_since_epoch();
      msg.arrivalTime = now;
    });
    seq++;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }

  return 0;
}

