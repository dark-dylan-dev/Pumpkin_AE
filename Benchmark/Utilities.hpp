#pragma once

#include <print>
#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <tuple>
#include <cstdint>
#include <utility>
#include <chrono>
#include <numeric>

#include <AE/Codec.hpp>
#include "Structures.hpp"

using namespace Pumpkin::AE;

constexpr std::uint32_t kInventoryType = 1;
constexpr std::uint32_t kFirstInventoryID = 1337;
constexpr const char* kInventoryFile = "inventory.assets";

template<typename Func>
double MeasureTime(Func&& function) {
    using namespace std::chrono;

    const auto start = steady_clock::now();

    function();

    const auto end = steady_clock::now();

    return duration<double, std::milli>(end - start).count();
}

BenchmarkStats CalculateStats(std::vector<double> times)
{
    if (times.empty())
        return {};

    BenchmarkStats stats{};

    stats.average =
        std::accumulate(times.begin(), times.end(), 0.0) / times.size();

    std::sort(times.begin(), times.end());

    const std::size_t size = times.size();

    if (size % 2 == 0)
        stats.median = (times[size / 2 - 1] + times[size / 2]) / 2.0;
    else
        stats.median = times[size / 2];

    stats.minimum = times.front();
    stats.maximum = times.back();

    return stats;
}

void PrintStats(const char* name, const std::vector<double>& times)
{
    const auto stats = CalculateStats(times);

    std::println(
        "{:<15} | {:>9.3f} | {:>9.3f} | {:>9.3f} | {:>9.3f} ms",
        name,
        stats.average,
        stats.median,
        stats.minimum,
        stats.maximum
    );
}
