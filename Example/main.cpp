#include "Item.hpp"

#include <print>

int main() {
    using namespace Pumpkin::AE;
    constexpr uint32_t kInventoryType = 1;
    constexpr uint32_t kInventoryID = 1337;

    // Build the data: two materials shared by ten items, and one equipped item
    const auto iron  = std::make_shared<Material>(Material{ .name = "Iron",  .roughness = 0.7f });
    const auto steel = std::make_shared<Material>(Material{ .name = "Steel", .roughness = 0.3f });

    Inventory inventory;
    for (uint64_t i = 0; i < 10; ++i) {
        inventory.items.push_back(std::make_unique<Item>(Item{
            .id       = i,
            .name     = "Sword",
            .tags     = {"weapon"},
            .weight   = 100.0f / (i + 1),
            .position = std::make_unique<Position>(Position{ .x = static_cast<int>(i), .y = static_cast<int>(i * 2) }),
            .material = i % 2 ? steel : iron }));
    }
    inventory.equipped = inventory.items[3].get(); // raw pointer into the same inventory

    // Write: the whole graph is one asset
    Writer w;
    if (!w.Add(kInventoryID, kInventoryType, inventory)) { std::println("error: the inventory could not be encoded"); return 1; }
    if (!w.Save("inventory.assets")) return 1;

    // Read
    Reader r;
    if (!r.Open("inventory.assets")) { std::println("error: {}", r.error); return 1; }

    auto loaded = r.Load<Inventory>(kInventoryID);
    if (!loaded) { std::println("error: {}", r.error); return 1; }

    for (const auto& item : loaded->items)
        std::println("{}: {} ({} kg) {} at ({}, {}), {}", item->id, item->name, item->weight, item->tags,
                     item->position->x, item->position->y, item->material->name);

    // Identity survived the round trip
    std::println("items 0 and 2 share one material: {}", loaded->items[0]->material == loaded->items[2]->material);
    std::println("references to the iron material: {}", loaded->items[0]->material.use_count());
    std::println("equipped is item {}, the very object owned by the list: {}",
                 loaded->equipped->id, loaded->equipped == loaded->items[3].get());
}
