#include "Item.hpp"

#include <print>

int main() {
    using namespace Pumpkin::AE;
    constexpr uint32_t kItemType = 1;

    // Write
    Writer w;
    for (uint64_t i = 0; i < 10; ++i) {
        Position pos{ .x = static_cast<int>(i), .y = static_cast<int>(i * 2) };
        Item sword{ .id = i, .name = "Iron sword", .tags = {"weapon"}, .weight = 100.0f / (i + 1), .position = std::make_unique<Position>(pos) };
        w.Add(sword.id, kItemType, sword);
    }
    if (!w.Save("items.assets")) return 1;

    // Read
    Reader r;
    if (!r.Open("items.assets")) { std::println("error: {}", r.error); return 1; }

    for (auto& e : r.List())
        if (auto item = r.Load<Item>(e.id))
            std::println("{}: {} ({} kg) {} {}", item->id, item->name, item->weight, item->tags, item->position ? "at (" + std::to_string(item->position->x) + ", " + std::to_string(item->position->y) + ")" : "");
}
