#include <AE/AssetArchive.hpp>

#include <algorithm>
#include <bit>

namespace Pumpkin::AE {

    /// @brief "ASET"
    constexpr std::uint32_t kMagic      = std::endian::native == std::endian::little ? 0x54455341 : 0x41534554;
    constexpr std::uint16_t kVersion    = PUMPKIN_AE_VERSION;
    constexpr std::size_t   kHeaderSize = 20;
    constexpr std::size_t   kEntrySize  = 32;

    // CRC-32 (IEEE), bitwise implementation.
    // See: https://en.wikipedia.org/wiki/Computation_of_cyclic_redundancy_checks#CRC-32_example
    static inline std::uint32_t CRC32(std::span<const std::uint8_t> d) {
        std::uint32_t c = 0xFFFFFFFF;
        for (std::uint8_t b : d) {
            c ^= b;
            for (int i = 0; i < 8; ++i)
                c = (c >> 1) ^ (0xEDB88320 & (0u - (c & 1))); // xor the polynomial if the low bit was 1
        }
        return ~c;
    }

    // Appends the n low bytes of v, least significant first (little-endian, host independent).
    static inline void Put(std::vector<std::uint8_t>& o, std::uint64_t v, int n) {
        if constexpr(std::endian::native == std::endian::little)
            for (int i = 0; i < n; ++i) o.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
        else
            for (int i = n - 1; i > 0; --i) o.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }

    // Reads n bytes at p as a little-endian integer and advances p. No bounds check: validate the size first.
    static inline std::uint64_t Get(std::span<const std::uint8_t> b, std::size_t& p, int n) {
        std::uint64_t v = 0;
        if constexpr(std::endian::native == std::endian::little)
            for (int i = 0; i < n; ++i) v |= static_cast<std::uint64_t>(b[p++]) << (8 * i);
        else
            for (int i = n - 1; i > 0; --i) v |= static_cast<std::uint64_t>(b[p++]) << (8 * i);
        return v;
    }


    bool Writer::Add(std::uint64_t id, std::uint32_t type, std::span<const std::uint8_t> bytes) {
        if (std::ranges::any_of(m_entries, [&](auto& e) { return e.id == id; }))
            return false; // duplicate ID

        // Offsets are absolute: the data section starts right after the header
        m_entries.push_back({id, type, CRC32(bytes), kHeaderSize + m_data.size(), bytes.size()});
        m_data.insert(m_data.end(), bytes.begin(), bytes.end());
        return true;
    }

    bool Writer::Save(const std::string& path) const {
        std::vector<std::uint8_t> out;
        // 20 bytes header
        Put(out, kMagic, 4); Put(out, kVersion, 2); Put(out, 0, 2); // 8 bytes - header [magic (4)|version (2)|reserved (2)]
        Put(out, m_entries.size(), 4);                              // 4 bytes - number of elements
        Put(out, kHeaderSize + m_data.size(), 8);                   // 8 bytes - directory offset (pointer to entries table, after raw data section)

        // Raw data
        out.insert(out.end(), m_data.begin(), m_data.end());

        // Entries table
        for (auto& e : m_entries) {
            Put(out, e.id, 8); Put(out, e.type, 4); Put(out, e.crc, 4); // 16 bytes - identity (id, type, checksum of the payload)
            Put(out, e.offset, 8); Put(out, e.size, 8);                 // 16 bytes - where the payload lives and how long it is
        }

        // File write
        std::ofstream f(path, std::ios::binary);
        f.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
        f.flush(); // surface buffered write errors (e.g. disk full) before checking the stream
        return static_cast<bool>(f);
    }


    bool Reader::Open(const std::string& path) {
        // Reset so the reader can be reused
        m_file.close(); m_file.clear(); m_entries.clear(); error.clear();

        // Open at the end (ate) so tellg() gives the file size
        m_file.open(path, std::ios::binary | std::ios::ate);
        if (!m_file) return Fail("cannot open file");
        const std::uint64_t fileSize = static_cast<std::uint64_t>(m_file.tellg());
        if (fileSize < kHeaderSize) return Fail("truncated header");

        // Header
        std::vector<std::uint8_t> h(kHeaderSize);
        m_file.seekg(0); m_file.read(reinterpret_cast<char*>(h.data()), static_cast<std::streamsize>(kHeaderSize));
        if (!m_file) return Fail("cannot read header");

        std::size_t p = 0;
        if (Get(h, p, 4) != kMagic) return Fail("bad magic");
        const std::uint64_t version = Get(h, p, 2);
        if (version == 0 || version > kVersion) return Fail("unsupported version");
        p += 2; // skip the reserved bytes
        const std::uint64_t count  = Get(h, p, 4);
        const std::uint64_t dirOff = Get(h, p, 8);

        // Bound everything BEFORE allocating
        if (dirOff < kHeaderSize || dirOff > fileSize) return Fail("bad directory offset");
        if (count > (fileSize - dirOff) / kEntrySize || // also keeps the multiplication below from overflowing
            dirOff + count * kEntrySize != fileSize)   return Fail("bad directory size"); // the directory must end exactly at EOF

        // Directory, read in one block
        std::vector<std::uint8_t> d(count * kEntrySize);
        m_file.seekg(static_cast<std::streamoff>(dirOff));
        m_file.read(reinterpret_cast<char*>(d.data()), static_cast<std::streamsize>(d.size()));
        if (!m_file) return Fail("truncated directory");

        // Parse into a temporary list
        std::vector<Entry> tmp; tmp.reserve(count);
        p = 0;
        for (std::uint64_t i = 0; i < count; ++i) {
            Entry e{Get(d, p, 8), static_cast<std::uint32_t>(Get(d, p, 4)), static_cast<std::uint32_t>(Get(d, p, 4)),
                    Get(d, p, 8), Get(d, p, 8)};

            // Written as "size > dirOff - offset" so that offset + size can't overflow
            if (e.offset < kHeaderSize || e.offset > dirOff || e.size > dirOff - e.offset)
                return Fail("entry out of bounds");
            if (std::ranges::any_of(tmp, [&](auto& o) { return o.id == e.id; }))
                return Fail("duplicate ID");
            tmp.push_back(e);
        }
        m_entries = std::move(tmp);
        return true;
    }

    const std::vector<Entry>& Reader::List() const {
        return m_entries;
    }

    std::optional<std::vector<std::uint8_t>> Reader::Load(std::uint64_t id) {
        error.clear();
        const auto it = std::ranges::find(m_entries, id, &Entry::id);
        if (it == m_entries.end()) { error = "unknown ID"; return std::nullopt; }

        // Size was validated against the file in Open(), so this allocation is bounded
        m_file.clear(); // drop any failbit/eofbit left by a previous read
        std::vector<std::uint8_t> buf(it->size);
        m_file.seekg(static_cast<std::streamoff>(it->offset));
        m_file.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));

        if (!m_file)               { error = "read failed";  return std::nullopt; }
        if (CRC32(buf) != it->crc) { error = "CRC mismatch"; return std::nullopt; }

        return buf;
    }

    bool Reader::Fail(const char* m) {
        error = m;
        m_entries.clear();
        return false;
    }

} // namespace Pumpkin::AE
