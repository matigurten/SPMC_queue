#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>

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

inline std::string ns_to_utc_timestr(uint64_t ns_since_epoch) {
    uint64_t seconds = ns_since_epoch / 1000000000ULL;
    uint64_t nanoseconds = ns_since_epoch % 1000000000ULL;
    time_t time_seconds = static_cast<time_t>(seconds);
    struct tm timeinfo;
    gmtime_r(&time_seconds, &timeinfo);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    std::ostringstream oss;
    oss << buf << "." << std::setfill('0') << std::setw(9) << nanoseconds;
    return oss.str();
} 