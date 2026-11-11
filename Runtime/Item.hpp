#pragma once

#include <AE/Codec.hpp>

struct Item {
    uint64_t                 id = 0;
    std::string              name;
    std::vector<std::string> tags;
    float                    weight = 1.0f; // added in v2
};

template <>
struct Pumpkin::AE::Codec<Item> {
    static constexpr uint32_t kType   = 1;
    static constexpr uint16_t kSchema = 2;

    static void Write(BinaryWriter& w, const Item& v);

    static bool Read(BinaryReader& r, Item& v);
};
