#include <AE/AssetArchive.hpp> ///< Library include

#include "Item.hpp" ///< User file example

#include <print>

int main() {
    // example basic usage
    {
        Pumpkin::AE::Writer w;
        const char* filename = "test.assets";
        std::vector<uint8_t> a{1, 2, 3, 4}, b{9, 9, 9};
        w.Add(1001, /*type*/ 1, a);
        w.Add(1002, /*type*/ 1, b);
        if (!w.Save(filename)) { std::println("error: couldn't save \"{}\"", filename); return 1; }

        Pumpkin::AE::Reader r;
        if (!r.Open(filename)) { std::println("error: {}", r.error); return 1; }
        std::println("--- \"{}\" ---", filename);
        std::println("File has {} entries", r.List().size());
        for (auto& e : r.List())
            std::println("id={} type={} offset={:#x} size={}", e.id, e.type, e.offset, e.size);

        if (auto data = r.Load(1002)) std::println("loaded {} bytes for id 1002: {}", data->size(), *data);
        else                          std::println("error: {}", r.error);
        std::println();
    }

    // example user usage
    {
        Pumpkin::AE::Writer w;
        const char* filename = "items.assets";

        static const std::vector<std::string> tags {
            "weapon", "melee", "fire", "ice", "broken",
            "giant", "excalibur", "decoration", "locked", "heavy"
        };
        for (uint64_t i = 0; i < 10; ++i) {
            Item sword{
                .id = i,
                .name = "Iron sword",
                .tags = {tags[i]},
                .weight = 100.0f / static_cast<float>(i + 1),
            };

            w.Add(sword.id, Pumpkin::AE::Codec<Item>::kType, Pumpkin::AE::Encode(sword));
        }
        if (!w.Save(filename)) { std::println("error: couldn't save \"{}\"", filename); return 1; }

        Pumpkin::AE::Reader r;
        r.Open(filename);
        std::println("--- \"{}\" ---", filename);
        std::println("File has {} entries", r.List().size());
        for (auto& e : r.List()) {
            std::println("id={} type={} offset={:#x} size={}", e.id, e.type, e.offset, e.size);
            if (auto bytes = r.Load(e.id)) // CRC checked here
                if (auto item = Pumpkin::AE::Decode<Item>(*bytes)) {
                    std::println("{} ({} tags, {} kg)", item->name, item->tags.size(), item->weight);
                    std::println("Tags: {}", item->tags);
                }
        }
    }
}
