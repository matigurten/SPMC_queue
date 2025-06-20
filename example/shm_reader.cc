#include <bits/stdc++.h>
#include "structs.h"
#include "shm.h"
#include <unordered_map>
using namespace std;

// usage: ./shm_reader [shm file]
// use taskset -c to bind core
int main(int argc, char** argv) {
  const char* shm_file = "SPMCQueue_test";
  if (argc >= 2) {
    shm_file = argv[1];
  }
  Q* q = shmmap(shm_file);
  if (!q) return 1;
  auto reader = q->getReader();
  cout << "reader size: " << sizeof(reader) << endl;

  std::unordered_map<uint32_t, uint64_t> last_seq;
  while (true) {
    Event* msg = reader.readLast();
    if (!msg) continue;
    uint64_t now = get_ns_since_epoch();
    int64_t latency_ns = static_cast<int64_t>(now) - static_cast<int64_t>(msg->arrivalTime);
    // Sequence validation
    auto it = last_seq.find(msg->instrument_id);
    if (it != last_seq.end()) {
      if (msg->seq != it->second + 1) {
        std::cerr << "[SEQ WARNING] instrument_id: " << msg->instrument_id << ", expected: " << (it->second + 1) << ", got: " << msg->seq << std::endl;
      }
    }
    last_seq[msg->instrument_id] = msg->seq;
    cout << "instrument_id: " << msg->instrument_id
         << ", sequence: " << msg->seq
         << ", event_type: " << static_cast<int>(msg->event_type)
         << ", side: " << static_cast<int>(msg->side)
         << ", order_type: " << static_cast<int>(msg->order_type)
         << ", amount: " << msg->amount
         << ", price: " << msg->price
         << ", order_id: " << msg->order_id
         << ", other_id: " << msg->order_id2
         << ", gTm: " << msg->gTm << " ns"
         << ", mTs: " << msg->mTs << " ns"
         << ", arrivalTime: " << msg->arrivalTime
         << ", latency: " << latency_ns << " ns"
         << endl << endl;
  }

  return 0;
}


