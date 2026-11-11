#include "Item.hpp"

namespace Pumpkin::AE {

    void Codec<Item>::Write(BinaryWriter& w, const Item& v) {
        w.u16(kSchema);
        w.u64(v.id);
        w.str(v.name);
        w.u32(static_cast<std::uint32_t>(v.tags.size()));
        for (auto& t : v.tags) w.str(t);
        w.f32(v.weight); // v2 field
    }

    bool Codec<Item>::Read(BinaryReader& r, Item& v) {
        uint16_t schema = r.u16();
        if (!r.ok() || schema < 1 || schema > kSchema) return false; // reject unknown versions
        Item tmp;
        tmp.id   = r.u64();
        tmp.name = r.str();
        std::uint32_t n = r.count(4); // each string is at least a u32 length
        tmp.tags.reserve(n);
        for (std::uint32_t i = 0; i < n; ++i) tmp.tags.push_back(r.str());
        tmp.weight = (schema >= 2) ? r.f32() : 1.0f; // v1 -> default value
        if (!r.ok()) return false;
        v = std::move(tmp); // commit only on success
        return true;
    }
} // namespace Pumpkin::AE
