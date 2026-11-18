#pragma once

namespace Pumpkin::AE {

    template <class T>
    std::optional<std::vector<std::uint8_t>> Encode(const T& v) {
        BinaryWriter w;
        Codec<T>::Write(w, v);
        if (!w.ok()) return std::nullopt; // never hand out a half-written payload
        return w.take();
    }

    template <class T>
    std::optional<T> Decode(std::span<const std::uint8_t> bytes) {
        BinaryReader r(bytes);
        T v{};
        if (!Codec<T>::Read(r, v) || !r.ok() || !r.atEnd()) return std::nullopt; // reject trailing junk too
        return v;
    }

    template <class T>
        requires (!std::convertible_to<const T&, std::span<const std::uint8_t>>)
    bool Writer::Add(const std::uint64_t id, const std::uint32_t type, const T& v) {
        const auto bytes = Encode(v);
        return bytes && Add(id, type, std::span<const std::uint8_t>(*bytes));
    }

    template <class T>
    std::optional<T> Reader::Load(std::uint64_t id) {
        const auto bytes = Load(id);
        if (!bytes) return std::nullopt;

        auto value = Decode<T>(*bytes);
        if (!value) error = "payload does not match the layout of the requested type";
        return value;
    }

} // namespace Pumpkin::AE
