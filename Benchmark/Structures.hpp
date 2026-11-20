#pragma once

struct Material {
    std::string name;
    float roughness = 0.5f;

    auto Reflect(this auto&& self) {
        return std::tie(self.name, self.roughness);
    }
};

struct Item {
    std::uint64_t id = 0;
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

struct BenchmarkStats {
    double average = 0.0;
    double median = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
};