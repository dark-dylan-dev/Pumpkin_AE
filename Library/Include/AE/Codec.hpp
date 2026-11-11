#pragma once

#include <optional>

#include <AE/BinaryIO.hpp>

namespace Pumpkin::AE {

    /// @brief Adapter between a domain type and its binary encoding.
    ///
    /// Specialize this for each type you want to store. The archive core never
    /// sees it, so adding a type doesn't require changing the core.
    /// A specialization must provide:
    /// @code
    /// static constexpr std::uint32_t kType;   // asset type stored in the directory
    /// static constexpr std::uint16_t kSchema; // current schema version of the payload
    /// static void Write(BinaryWriter&, const T&);
    /// static bool Read(BinaryReader&, T&);    // must leave T untouched on failure
    /// @endcode
    /// @tparam T Domain type.
    template <class T>
    struct Codec;

    /// @brief Encodes a value into a payload, ready for Writer::Add().
    /// @tparam T Domain type, which needs a Codec<T> specialization.
    /// @param v Value to encode.
    /// @return The encoded bytes.
    template <class T>
    std::vector<std::uint8_t> Encode(const T& v);

    /// @brief Decodes a payload into a value.
    /// @tparam T Domain type, which needs a Codec<T> specialization.
    /// @param bytes Encoded payload, e.g. from Reader::Load().
    /// @return The value, or std::nullopt if the payload is invalid, truncated or has trailing bytes.
    template <class T>
    std::optional<T> Decode(std::span<const std::uint8_t> bytes);

} // namespace Pumpkin::AE

#include <AE/Codec.inl>
