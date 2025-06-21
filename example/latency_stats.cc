#include "latency_stats.h"
#include <algorithm>
#include <numeric>
#include <iostream>

LatencyStats::LatencyStats(int print_every) : print_every(print_every) {}

void LatencyStats::add(uint64_t parsing, uint64_t ipc, uint64_t end2end) {
    parsing_latencies.push_back(parsing);
    ipc_latencies.push_back(ipc);
    end2end_latencies.push_back(end2end);
    count++;
    if (count % print_every == 0) print_stats();
}

void LatencyStats::print_stats() {
    print_one("Parsing", parsing_latencies);
    print_one("IPC", ipc_latencies);
    print_one("End2End", end2end_latencies);
    std::cout << std::endl;
    // Clear for next window
    parsing_latencies.clear();
    ipc_latencies.clear();
    end2end_latencies.clear();
}

void LatencyStats::print_one(const char* label, std::vector<uint64_t>& v) {
    if (v.empty()) return;
    std::sort(v.begin(), v.end());
    uint64_t min = v.front();
    uint64_t max = v.back();
    double avg = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
    uint64_t p50 = v[v.size()/2];
    uint64_t p90 = v[v.size()*9/10];
    uint64_t p99 = v[v.size()*99/100];
    std::cout << label << " latency (ns): min=" << min << ", avg=" << avg << ", max=" << max
              << ", p50=" << p50 << ", p90=" << p90 << ", p99=" << p99 << std::endl;
} 