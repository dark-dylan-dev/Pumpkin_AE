#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <AE/Codec.hpp>

struct Position {
    int x = 0, y = 0;
    auto Reflect(this auto&& self) { return std::tie(self.x, self.y); }
};

struct Material {
    std::string name;
    float       roughness = 0.5f;
    auto Reflect(this auto&& self) { return std::tie(self.name, self.roughness); }
};

struct Item {
    std::uint64_t             id = 0;
    std::string               name;
    std::vector<std::string>  tags;
    float                     weight = 1.0f;
    std::unique_ptr<Position> position;
    std::shared_ptr<Material> material;
    auto Reflect(this auto&& self) { return std::tie(self.id, self.name, self.tags, self.weight, self.position, self.material); }
};

struct Inventory {
    std::vector<std::unique_ptr<Item>> items;
    Item* equipped = nullptr;
    auto Reflect(this auto&& self) { return std::tie(self.items, self.equipped); }
};

// Leaky types to test circular references
struct Leaky2;

struct Leaky1 {
    std::shared_ptr<Leaky2> leaky;
	auto Reflect(this auto&& self) { return std::tie(self.leaky); }
};

struct Leaky2 {
	std::shared_ptr<Leaky1> leaky;
	auto Reflect(this auto&& self) { return std::tie(self.leaky); }
};
