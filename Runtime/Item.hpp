#pragma once

#include <AE/Codec.hpp>

struct Item {
    uint64_t                 id = 0;
    std::string              name;
    std::vector<std::string> tags;
    float                    weight = 1.0f;

    // User implementation: the only thing needed to read and write an Item
    auto Reflect(this auto&& self) { return std::tie(self.id, self.name, self.tags, self.weight); }
};
