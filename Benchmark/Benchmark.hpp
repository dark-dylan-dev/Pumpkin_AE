
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

using namespace Pumpkin::AE;

constexpr uint32_t kInventoryType = 1;
constexpr uint32_t kFirstInventoryID = 1337;
constexpr const char* kInventoryFile = "inventory.assets";

struct Material {
    std::string name;
    float roughness = 0.5f;

    auto Reflect(this auto&& self) {
        return std::tie(self.name, self.roughness);
    }
};

struct Item {
    uint64_t id = 0;
    std::string name;
    std::vector<std::string> tags;
    float weight = 1.0f;
    std::shared_ptr<Material> material;

    auto Reflect(this auto&& self) {
        return std::tie(
            self.id,
            self.name,
            self.tags,
            self.weight,
            self.material
        );
    }
};

struct Inventory {
    std::vector<std::unique_ptr<Item>> items;
    Item* equipped = nullptr;

    auto Reflect(this auto&& self) {
        return std::tie(self.items, self.equipped);
    }
};

struct BenchmarkResults {
    std::vector<double> creatingInventories;
    std::vector<double> writingInventories;
    std::vector<double> savingInventories;
    std::vector<double> openingInventories;
    std::vector<double> loadingInventories;
};

template<typename Func>
double MeasureTime(Func&& function) {
    using namespace std::chrono;

    const auto start = steady_clock::now();

    function();

    const auto end = steady_clock::now();

    return duration<double, std::milli>(end - start).count();
}

struct BenchmarkStats {
    double average = 0.0;
    double median = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
};

BenchmarkStats CalculateStats(std::vector<double> times)
{
    if (times.empty())
        return {};

    BenchmarkStats stats{};

    stats.average =
        std::accumulate(times.begin(), times.end(), 0.0) / times.size();

    std::sort(times.begin(), times.end());

    const size_t size = times.size();

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

std::vector<Inventory> CreateInventories(
    int nbInventories,
    int nbItemsPerInventory)
{
    std::vector<Inventory> inventories;

    if (nbInventories <= 0 || nbItemsPerInventory < 0)
        return inventories;

    inventories.reserve(nbInventories);

    // Shared materials
    const auto iron = std::make_shared<Material>(
        Material{ .name = "Iron", .roughness = 0.7f });

    const auto steel = std::make_shared<Material>(
        Material{ .name = "Steel", .roughness = 0.3f });

    for (int inv = 0; inv < nbInventories; ++inv)
    {
        Inventory inventory;
        inventory.items.reserve(nbItemsPerInventory);

        for (int item = 0; item < nbItemsPerInventory; ++item)
        {
            const uint64_t id =
                static_cast<uint64_t>(inv) * nbItemsPerInventory + item;

            inventory.items.push_back(std::make_unique<Item>(Item{
                .id = id,
                .name = "Sword",
                .tags = {"weapon"},
                .weight = 100.0f / (item + 1),
                .material = item % 2 ? steel : iron
            }));
        }

        if (inventory.items.size() > 3)
            inventory.equipped = inventory.items[3].get();

        inventories.push_back(std::move(inventory));
    }

    return inventories;
}

bool WriteInventories(
    Writer& writer,
    const std::vector<Inventory>& inventories)
{
    for (size_t i = 0; i < inventories.size(); ++i)
    {
        const uint32_t inventoryID =
            kFirstInventoryID + static_cast<uint32_t>(i);

        if (!writer.Add(
            inventoryID,
            kInventoryType,
            inventories[i]))
        {
            std::println("Encoding failed for inventory {}", i);
            return false;
        }
    }

    return true;
}

bool SaveInventories(Writer& writer)
{
    if (!writer.Save(kInventoryFile))
    {
        std::println("Failed to save inventories.");
        return false;
    }

    return true;
}

bool OpenInventories(Reader& reader)
{
    if (!reader.Open(kInventoryFile))
    {
        std::println("Reader error: {}", reader.error);
        return false;
    }

    return true;
}

bool LoadInventories(
    Reader& reader,
    int nbInventories,
    std::vector<Inventory>& loadedInventories)
{
    loadedInventories.clear();
    loadedInventories.reserve(nbInventories);

    for (int i = 0; i < nbInventories; ++i)
    {
        const uint32_t inventoryID = kFirstInventoryID + i;

        auto loaded = reader.Load<Inventory>(inventoryID);

        if (!loaded)
        {
            std::println(
                "Loading failed for inventory {}: {}",
                i,
                reader.error
            );
            return false;
        }

        loadedInventories.push_back(std::move(*loaded));
    }

    return true;
}



bool RunBenchmark(int nbInventories, int nbItemsPerInventory)
{
    constexpr int kWarmupRuns = 1;
    constexpr int kBenchmarkRuns = 30;

    BenchmarkResults results;

    auto RunSingleTest = [&](BenchmarkResults* output) -> bool
    {
        // 1. Creation
        std::vector<Inventory> inventories;

        double creationTime = MeasureTime([&]()
        {
            inventories = CreateInventories(
                nbInventories,
                nbItemsPerInventory
            );
        });

        if (inventories.empty())
            return false;

        // 2. Encoding
        Writer writer;
        bool writeSuccess = false;

        double writingTime = MeasureTime([&]()
        {
            writeSuccess = WriteInventories(writer, inventories);
        });

        if (!writeSuccess)
            return false;

        // 3. Saving
        bool saveSuccess = false;

        double savingTime = MeasureTime([&]()
        {
            saveSuccess = SaveInventories(writer);
        });

        if (!saveSuccess)
            return false;

        // 4. Opening
        Reader reader;
        bool openSuccess = false;

        double openingTime = MeasureTime([&]()
        {
            openSuccess = OpenInventories(reader);
        });

        if (!openSuccess)
            return false;

        // 5. Loading
        std::vector<Inventory> loadedInventories;
        bool loadSuccess = false;

        double loadingTime = MeasureTime([&]()
        {
            loadSuccess = LoadInventories(
                reader,
                nbInventories,
                loadedInventories
            );
        });

        if (!loadSuccess)
            return false;

        // Store measurements
        if (output)
        {
            output->creatingInventories.push_back(creationTime);
            output->writingInventories.push_back(writingTime);
            output->savingInventories.push_back(savingTime);
            output->openingInventories.push_back(openingTime);
            output->loadingInventories.push_back(loadingTime);
        }

        return true;
    };

    // Warmup
    std::println("Warming up...");

    for (int i = 0; i < kWarmupRuns; ++i)
    {
        if (!RunSingleTest(nullptr))
            return false;
    }

    // Benchmark
    std::println("Running {} benchmark iterations...", kBenchmarkRuns);

    for (int i = 0; i < kBenchmarkRuns; ++i)
    {
        if (!RunSingleTest(&results))
            return false;

        std::print("\rProgress: {}/{}", i + 1, kBenchmarkRuns);
    }

    std::println("\n");

    // Results
    std::println("========== BENCHMARK RESULTS ==========");
    std::println("Inventories        : {}", nbInventories);
    std::println("Items per inventory: {}", nbItemsPerInventory);
    std::println(
        "Total items        : {}",
        static_cast<uint64_t>(nbInventories) * nbItemsPerInventory
    );
    std::println("Iterations         : {}", kBenchmarkRuns);

    std::println("\n--- Execution statistics (ms) ---");

    std::println(
        "{:<15} | {:>9} | {:>9} | {:>9} | {:>9}",
        "Function", "Average", "Median", "Min", "Max"
    );

    PrintStats("Creation", results.creatingInventories);
    PrintStats("Encoding", results.writingInventories);
    PrintStats("Saving", results.savingInventories);
    PrintStats("Opening", results.openingInventories);
    PrintStats("Loading", results.loadingInventories);

    std::println("=======================================");

    return true;
}


