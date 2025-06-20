#pragma once
#include <ostream>
#include <vector>
#include <algorithm>

template<typename T>
class Statistic
{
public:
  void reserve(uint32_t size) { vec.reserve(size); }

  size_t size() { return vec.size(); }

  void add(T v) { vec.push_back(v); }

  void print(std::ostream& os, double cycles_to_ns = 0.0) {
    size_t n = vec.size();
    os << "cnt: " << n << std::endl;
    if (n == 0) return;
    T first = vec[0];
    std::sort(vec.begin(), vec.end());
    T sum = std::accumulate(vec.begin(), vec.end(), 0);
    T mean = sum / n;
    T var = 0;
    for (T v : vec) {
      var += (v - mean) * (v - mean);
    }
    var /= n;
    auto print_val = [&](const char* label, T val) {
      os << label << ": " << val;
      if (cycles_to_ns > 0.0) {
        os << " (" << static_cast<uint64_t>(val * cycles_to_ns) << " ns)";
      }
      os << std::endl;
    };
    print_val("min", vec.front());
    print_val("max", vec.back());
    print_val("first", first);
    print_val("mean", mean);
    os << "sd: " << sqrt(var);
    if (cycles_to_ns > 0.0) {
      os << " (" << static_cast<uint64_t>(sqrt(var) * cycles_to_ns) << " ns)";
    }
    os << std::endl;
    print_val("1%", vec[n * 1 / 100]);
    print_val("10%", vec[n * 10 / 100]);
    print_val("50%", vec[n * 50 / 100]);
    print_val("90%", vec[n * 90 / 100]);
    print_val("99%", vec[n * 99 / 100]);
  }

private:
  std::vector<T> vec;
};

