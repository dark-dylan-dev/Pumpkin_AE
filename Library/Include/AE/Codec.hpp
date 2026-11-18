#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <AE/AssetArchive.hpp>
#include <AE/BinaryIO.hpp>

namespace Pumpkin::AE {

    /// @brief Adapter between a domain type and its binary encoding.
    ///
    /// Types with a Reflect() member get a generated Codec automatically, so
    /// they need nothing else. Specialize Codec<T> yourself only when you need
    /// custom behaviour (schema migration, a non-reflectable type...). A
    /// specialization must provide:
    /// @code
    /// static void Write(BinaryWriter&, const T&);
    /// static bool Read(BinaryReader&, T&);    // must leave T untouched on failure
    /// @endcode
    /// The asset type stored in the archive directory is chosen by the caller
    /// when it calls Writer::Add().
    /// @tparam T Domain type.
    template <class T>
    struct Codec;

    /// @brief Generated codec for any type exposing Reflect().
    ///
    /// The fields are encoded in the order returned by Reflect(), which is the
    /// payload schema. A full specialization of Codec<T> takes precedence.
    template <Reflectable T>
    struct Codec<T> {
        static void Write(BinaryWriter& w, const T& v) { w.write(v); }

        static bool Read(BinaryReader& r, T& v) {
            T tmp{};
            r.read(tmp);
            if (!r.ok()) return false;
            v = std::move(tmp); // commit only on success
            return true;
        }
    };

    /// @brief Encodes a value into a payload, ready for Writer::Add().
    /// @tparam T Domain type, which needs a Codec<T> (generated if it has Reflect()).
    /// @param v Value to encode.
    /// @return The encoded bytes, or std::nullopt if the value could not be encoded (see BinaryWriter::ok()).
    template <class T>
    std::optional<std::vector<std::uint8_t>> Encode(const T& v);

    /// @brief Decodes a payload into a value.
    /// @tparam T Domain type, which needs a Codec<T> (generated if it has Reflect()).
    /// @param bytes Encoded payload, e.g. from Reader::Load().
    /// @return The value, or std::nullopt if the payload is invalid, truncated or has trailing bytes.
    template <class T>
    std::optional<T> Decode(std::span<const std::uint8_t> bytes);

} // namespace Pumpkin::AE

#include <AE/Codec.inl>
