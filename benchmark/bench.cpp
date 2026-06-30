// Synthetic throughput + latency benchmark for the matching engine.
// Generates a random order flow around a moving mid-price and measures
// per-operation latency percentiles and overall throughput.

#include "lob/order_book.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

using namespace lob;
using clk = std::chrono::steady_clock;

int main(int argc, char** argv) {
    const std::size_t n = (argc > 1) ? std::stoull(argv[1]) : 1'000'000;

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<int>  side_d(0, 1);
    std::uniform_int_distribution<int>  off_d(-20, 20);
    std::uniform_int_distribution<Quantity> qty_d(1, 100);
    std::uniform_int_distribution<int>  type_d(0, 9);   // ~10% market orders

    OrderBook book;  // no callback: measure pure engine cost
    std::vector<std::uint64_t> latencies;
    latencies.reserve(n);

    Price mid = 10'000;
    OrderId next_id = 1;

    const auto wall_start = clk::now();
    for (std::size_t i = 0; i < n; ++i) {
        if (i % 5000 == 0) mid += (off_d(rng) > 0 ? 1 : -1);  // slow drift

        Order o;
        o.id   = next_id++;
        o.ts   = i;
        o.side = side_d(rng) ? Side::Buy : Side::Sell;
        o.type = (type_d(rng) == 0) ? OrderType::Market : OrderType::Limit;
        o.price    = mid + off_d(rng);
        o.quantity = qty_d(rng);

        const auto t0 = clk::now();
        book.submit(o);
        const auto t1 = clk::now();
        latencies.push_back(
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    }
    const auto wall_end = clk::now();

    std::sort(latencies.begin(), latencies.end());
    auto pct = [&](double p) { return latencies[static_cast<std::size_t>(p * (n - 1))]; };

    const double secs =
        std::chrono::duration<double>(wall_end - wall_start).count();

    std::cout << "operations      : " << n << '\n';
    std::cout << "elapsed         : " << secs << " s\n";
    std::cout << "throughput      : " << static_cast<std::uint64_t>(n / secs)
              << " ops/s\n";
    std::cout << "latency p50     : " << pct(0.50) << " ns\n";
    std::cout << "latency p99     : " << pct(0.99) << " ns\n";
    std::cout << "latency p99.9   : " << pct(0.999) << " ns\n";
    std::cout << "resting orders  : " << book.resting_order_count() << '\n';
    return 0;
}
