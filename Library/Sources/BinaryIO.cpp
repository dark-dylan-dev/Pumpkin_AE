#include <AE/BinaryIO.hpp>

#include <bit>

namespace Pumpkin::AE {

    void BinaryWriter::put(std::uint64_t v, int n) {
        if constexpr(std::endian::native == std::endian::little)
            for (int i = 0; i < n; ++i) m_buf.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
        else
            for (int i = n - 1; i > 0; --i) m_buf.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }
    void BinaryWriter::u8(std::uint8_t v)   { put(v, 1); }
    void BinaryWriter::u16(std::uint16_t v) { put(v, 2); }
    void BinaryWriter::u32(std::uint32_t v) { put(v, 4); }
    void BinaryWriter::u64(std::uint64_t v) { put(v, 8); }
    void BinaryWriter::boolean(bool v)      { u8(v ? 1 : 0); }
    void BinaryWriter::f32(float v)         { u32(std::bit_cast<std::uint32_t>(v)); }
    void BinaryWriter::f64(double v)        { u64(std::bit_cast<std::uint64_t>(v)); }
    void BinaryWriter::str(const std::string& s) {
        u32(static_cast<std::uint32_t>(s.size()));
        m_buf.insert(m_buf.end(), s.begin(), s.end());
    }
    std::vector<std::uint8_t> BinaryWriter::take() { return std::move(m_buf); }


    BinaryReader::BinaryReader(std::span<const std::uint8_t> b) : m_bytes(b) {}

    std::uint64_t BinaryReader::get(int n) {
        if (!m_ok || m_bytes.size() - m_pos < static_cast<std::size_t>(n)) { m_ok = false; return 0; }
        std::uint64_t v = 0;
        if constexpr(std::endian::native == std::endian::little)
            for (int i = 0; i < n; ++i) v |= static_cast<std::uint64_t>(m_bytes[m_pos++]) << (8 * i);
        else
            for (int i = n - 1; i > 0; --i) v |= static_cast<std::uint64_t>(m_bytes[m_pos++]) << (8 * i);
        return v;
    }

    bool BinaryReader::ok() const    { return m_ok; }
    bool BinaryReader::atEnd() const { return m_pos == m_bytes.size(); }

    std::uint8_t  BinaryReader::u8()  { return static_cast<std::uint8_t>(get(1)); }
    std::uint16_t BinaryReader::u16() { return static_cast<std::uint16_t>(get(2)); }
    std::uint32_t BinaryReader::u32() { return static_cast<std::uint32_t>(get(4)); }
    std::uint64_t BinaryReader::u64() { return get(8); }
    bool          BinaryReader::boolean() { std::uint8_t v = u8(); if (v > 1) m_ok = false; return v == 1; }
    float         BinaryReader::f32() { return std::bit_cast<float>(u32()); }
    double        BinaryReader::f64() { return std::bit_cast<double>(u64()); }

    std::string BinaryReader::str() {
        std::uint32_t n = u32();
        if (!m_ok || n > m_bytes.size() - m_pos) { m_ok = false; return {}; } // bound BEFORE allocating
        std::string s(reinterpret_cast<const char*>(m_bytes.data() + m_pos), n);
        m_pos += n;
        return s;
    }

    std::uint32_t BinaryReader::count(std::size_t minElemSize) {
        std::uint32_t n = u32();
        if (!m_ok || static_cast<std::uint64_t>(n) * minElemSize > m_bytes.size() - m_pos) { m_ok = false; return 0; }
        return n;
    }

} // namespace Pumpkin::AE
