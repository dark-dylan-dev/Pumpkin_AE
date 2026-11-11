#pragma once

namespace Pumpkin::AE {

    template <class T>
    std::vector<std::uint8_t> Encode(const T& v) {
        BinaryWriter w;
        Codec<T>::Write(w, v);
        return w.take();
    }

    template <class T>
    std::optional<T> Decode(std::span<const std::uint8_t> bytes) {
        BinaryReader r(bytes);
        T v{};
        if (!Codec<T>::Read(r, v) || !r.ok() || !r.atEnd()) return std::nullopt; // reject trailing junk too
        return v;
    }

} // namespace Pumpkin::AE
