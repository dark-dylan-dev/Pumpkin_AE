#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <Platform/Export.hpp>

/// @file
/// @brief Byte-level encoding of primitive types, used to build asset payloads.
///
/// @par Encoding
/// - Integers: fixed width, little-endian.
/// - bool: 1 byte, 0 or 1. Any other value is rejected on read.
/// - float / double: IEEE-754 bit pattern stored as u32 / u64.
/// - string: u32 byte length followed by the raw UTF-8 bytes, with no terminator.
/// - Sequences: encoded by the caller as a u32 element count followed by the elements.

namespace Pumpkin::AE {

    /// @brief Appends encoded values to an in-memory buffer.
    class PUMPKIN_API BinaryWriter {
        std::vector<std::uint8_t> m_buf; ///< Encoded bytes.

        /// @brief Appends the n low bytes of v, least significant first.
        void put(std::uint64_t v, int n);
    public:
        void u8(std::uint8_t v);   ///< @brief Writes an unsigned 8-bit integer.
        void u16(std::uint16_t v); ///< @brief Writes an unsigned 16-bit integer.
        void u32(std::uint32_t v); ///< @brief Writes an unsigned 32-bit integer.
        void u64(std::uint64_t v); ///< @brief Writes an unsigned 64-bit integer.
        void boolean(bool v);      ///< @brief Writes a boolean as one byte (0 or 1).
        void f32(float v);         ///< @brief Writes a 32-bit float as its IEEE-754 bit pattern.
        void f64(double v);        ///< @brief Writes a 64-bit float as its IEEE-754 bit pattern.

        /// @brief Writes a string as a u32 byte length followed by its bytes.
        /// @param s String, assumed to be valid UTF-8. Not validated.
        void str(const std::string& s);

        /// @brief Moves the encoded bytes out of the writer.
        /// @note The writer is left in a moved-from state and should not be reused.
        std::vector<std::uint8_t> take();
    };

    /// @brief Decodes values from a byte span, with bounds checking on every read.
    ///
    /// Failure is sticky: once a read goes out of bounds or finds an invalid
    /// value, ok() returns false for good and every later read returns a
    /// zero/empty value. Check ok() once after reading a whole structure.
    /// The reader does not own the bytes, so they must outlive it.
    class PUMPKIN_API BinaryReader {
        std::span<const std::uint8_t> m_bytes; ///< Bytes being read.
        std::size_t m_pos = 0;             ///< Current read position.
        bool m_ok = true;                  ///< false after the first failed read.

        /// @brief Reads n bytes as a little-endian integer, or fails if fewer remain.
        std::uint64_t get(int n);
    public:
        /// @param b Bytes to read from.
        explicit BinaryReader(std::span<const std::uint8_t> b);

        /// @brief Returns true while every read so far has succeeded.
        [[nodiscard]] bool ok() const;

        /// @brief Returns true if every byte has been consumed. Used to reject trailing data.
        [[nodiscard]] bool atEnd() const;

        std::uint8_t  u8();  ///< @brief Reads an unsigned 8-bit integer.
        std::uint16_t u16(); ///< @brief Reads an unsigned 16-bit integer.
        std::uint32_t u32(); ///< @brief Reads an unsigned 32-bit integer.
        std::uint64_t u64(); ///< @brief Reads an unsigned 64-bit integer.
        bool          boolean(); ///< @brief Reads a boolean. Fails if the byte is not 0 or 1.
        float         f32(); ///< @brief Reads a 32-bit float.
        double        f64(); ///< @brief Reads a 64-bit float.

        /// @brief Reads a string. The length is checked against the remaining bytes before allocating.
        /// @return The string, or an empty string on failure.
        std::string str();

        /// @brief Reads a sequence element count and checks it against the remaining bytes.
        ///
        /// Call this before reserving or looping, so a corrupted count can't
        /// trigger a huge allocation.
        /// @param minElemSize Minimum encoded size of one element in bytes. Must be at least 1.
        /// @return The count, or 0 on failure.
        std::uint32_t count(std::size_t minElemSize);
    };

} // namespace Pumpkin::AE
