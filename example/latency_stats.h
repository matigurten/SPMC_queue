#pragma once
#include <vector>
#include <cstdint>

// Generic latency statistics aggregator for monitoring
class LatencyStats {
public:
    // print_every: how often to print stats (default 10)
    LatencyStats(int print_every = 10);
    void add(uint64_t parsing, uint64_t ipc, uint64_t end2end);
    void print_stats();
private:
    int print_every;
    int count = 0;
    std::vector<uint64_t> parsing_latencies, ipc_latencies, end2end_latencies;
    void print_one(const char* label, std::vector<uint64_t>& v);
}; 