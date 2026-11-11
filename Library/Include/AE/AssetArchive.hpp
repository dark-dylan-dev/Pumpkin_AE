#pragma once

#include <cstdint>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <Platform/Export.hpp>

/// @file
/// @brief Binary asset archive: a writer, a reader and the on-disk format.
///
/// @par File layout
/// All integers are little-endian. The directory sits at the end of the file
/// because its position is only known once every asset has been written.
/// @code
/// [ header : 20 B ][ raw asset data ... ][ directory : count * 32 B ]<EOF>
/// @endcode
///
/// @par Header (20 bytes)
/// | Field     | Size | Description                                  |
/// |-----------|------|----------------------------------------------|
/// | magic     | 4    | 0x54455341, "ASET" in little-endian          |
/// | version   | 2    | Format version, currently 1                  |
/// | reserved  | 2    | Always 0                                     |
/// | count     | 4    | Number of directory entries                  |
/// | dirOffset | 8    | Absolute file offset of the directory        |
///
/// @par Directory entry (32 bytes)
/// | Field  | Size | Description                                       |
/// |--------|------|---------------------------------------------------|
/// | id     | 8    | Unique asset ID                                   |
/// | type   | 4    | Asset type, meaning defined by the caller         |
/// | crc    | 4    | CRC-32 of the asset payload only                  |
/// | offset | 8    | Absolute file offset of the payload               |
/// | size   | 8    | Payload size in bytes                             |
///
/// @par Checksum
/// CRC-32 (IEEE 802.3, reflected, polynomial 0xEDB88320, initial value and
/// final XOR 0xFFFFFFFF). It covers the payload bytes only, not the header or
/// the directory. It detects accidental corruption and does not guarantee
/// authenticity.

namespace Pumpkin::AE {

    /// @brief One directory entry describing a stored asset.
    struct PUMPKIN_API Entry {
        std::uint64_t id;     ///< Unique asset ID.
        std::uint32_t type;   ///< Asset type, meaning defined by the caller.
        std::uint32_t crc;    ///< CRC-32 of the payload.
        std::uint64_t offset; ///< Absolute file offset of the payload.
        std::uint64_t size;   ///< Payload size in bytes.
    };

    /// @brief Collects assets in memory and writes them to an archive file.
    class PUMPKIN_API Writer {
        std::vector<std::uint8_t> m_data;    ///< Raw data section: every payload, back to back.
        std::vector<Entry>        m_entries; ///< Directory, in insertion order.
    public:
        /// @brief Queues an asset. Nothing is written to disk until Save().
        /// @param id    Unique asset ID.
        /// @param type  Asset type, stored as is.
        /// @param bytes Payload. Copied into the writer, and its CRC-32 is computed here.
        /// @return false if an asset with the same ID was already added.
        bool Add(std::uint64_t id, std::uint32_t type, std::span<const std::uint8_t> bytes);

        /// @brief Builds the whole archive in memory and writes it to disk in one go.
        /// @param path Destination file. Truncated if it already exists.
        /// @return false if the file could not be opened or written.
        [[nodiscard]] bool Save(const std::string& path) const;
    };

    /// @brief Opens an archive, lists its entries and loads assets by ID.
    class PUMPKIN_API Reader {
        std::ifstream      m_file;    ///< Open archive file.
        std::vector<Entry> m_entries; ///< Validated directory. Empty if no archive is open.
    public:
        /// @brief Message describing the last failure. Cleared by Open() and Load().
        std::string error;

        /// @brief Opens an archive and validates its header and directory.
        ///
        /// Checks the magic number, the version, the directory offset and size,
        /// every entry's bounds and ID uniqueness. Sizes are bounded before any
        /// allocation. On failure the reader is left empty and error is set.
        /// Can be called again to reuse the reader.
        /// @param path Archive file.
        /// @return true if the archive is valid.
        bool Open(const std::string& path);

        /// @brief Returns the directory. No asset data is read.
        [[nodiscard]] const std::vector<Entry>& List() const;

        /// @brief Loads one asset by ID without reading the others, and verifies its CRC-32.
        /// @param id Asset ID.
        /// @return The payload, or std::nullopt with error set (unknown ID, read failure or CRC mismatch).
        std::optional<std::vector<std::uint8_t>> Load(std::uint64_t id);
    private:
        /// @brief Records an error and empties the directory so no partial state remains.
        /// @param m Error message.
        /// @return Always false, so callers can write `return Fail(...)`.
        bool Fail(const char* m);
    };

} // namespace Pumpkin::AE
