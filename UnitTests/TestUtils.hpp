#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <deque>
#include <iterator>
#include <limits>
#include <list>
#include <memory>
#include <span>
#include <string>
#include <tuple>
#include <vector>

#include <Common.hpp>

using Bytes = std::vector<std::uint8_t>;

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------

// Path inside UnitTests/ (created on demand), removed when the object dies.
struct TempFile {
	std::string path;

	explicit TempFile(const std::string& name) : path("UnitTests/" + name) {
		std::filesystem::create_directories("UnitTests");
		std::filesystem::remove(path);
	}
	~TempFile() {
		std::error_code ec;
		std::filesystem::remove(path, ec);
	}
	TempFile(const TempFile&) = delete;
	TempFile& operator=(const TempFile&) = delete;
};

inline Bytes ReadFileBytes(const std::string& path) {
	std::ifstream f(path, std::ios::binary);
	return Bytes(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

inline void WriteFileBytes(const std::string& path, const Bytes& b) {
	std::ofstream f(path, std::ios::binary | std::ios::trunc);
	f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

// ---------------------------------------------------------------------------
// Little-endian helpers, written independently from the library
// ---------------------------------------------------------------------------

inline std::uint64_t GetLE(const Bytes& b, std::size_t pos, int n) {
	std::uint64_t v = 0;
	for (int i = 0; i < n; ++i) v |= static_cast<std::uint64_t>(b.at(pos + i)) << (8 * i);
	return v;
}

inline void PatchLE(Bytes& b, std::size_t pos, std::uint64_t v, int n) {
	for (int i = 0; i < n; ++i) b.at(pos + i) = static_cast<std::uint8_t>(v >> (8 * i));
}

// Bitwise CRC-32 (IEEE 802.3). Check value: Crc32("123456789") == 0xCBF43926.
inline std::uint32_t Crc32(std::span<const std::uint8_t> d) {
	std::uint32_t c = 0xFFFFFFFFu;
	for (const std::uint8_t byte : d) {
		c ^= byte;
		for (int i = 0; i < 8; ++i) c = (c & 1u) ? (c >> 1) ^ 0xEDB88320u : (c >> 1);
	}
	return ~c;
}

inline Bytes ToBytes(const std::string& s) { return Bytes(s.begin(), s.end()); }

// Archive layout constants (see AssetArchive.hpp)
inline constexpr std::size_t kHeaderSize = 20;
inline constexpr std::size_t kEntrySize  = 32;
// Header field offsets
inline constexpr std::size_t kOffMagic = 0, kOffVersion = 4, kOffReserved = 6, kOffCount = 8, kOffDirOffset = 12;
// Directory entry field offsets (relative to the start of the entry)
inline constexpr std::size_t kEntId = 0, kEntType = 8, kEntCrc = 12, kEntOffset = 16, kEntSize = 24;

// ---------------------------------------------------------------------------
// Extra fixture types (Common.hpp holds the shared ones)
// ---------------------------------------------------------------------------

enum class Color : std::uint16_t { Red = 1, Green = 2, Blue = 513 };
enum class Signed : int { Low = -5, High = 7 };

struct Primitives {
	bool           b = false;
	char           c = 0;
	std::int8_t    i8 = 0;
	std::uint8_t   u8 = 0;
	std::int16_t   i16 = 0;
	std::uint16_t  u16 = 0;
	std::int32_t   i32 = 0;
	std::uint32_t  u32 = 0;
	std::int64_t   i64 = 0;
	std::uint64_t  u64 = 0;
	float          f = 0.0f;
	double         d = 0.0;
	Color          color = Color::Red;
	Signed         sign = Signed::High;
	auto Reflect(this auto&& self) {
		return std::tie(self.b, self.c, self.i8, self.u8, self.i16, self.u16, self.i32, self.u32,
		                self.i64, self.u64, self.f, self.d, self.color, self.sign);
	}
	bool operator==(const Primitives&) const = default;
};

struct Containers {
	std::vector<int>                       numbers;
	std::vector<std::string>               words;
	std::vector<std::vector<std::uint8_t>> nested;
	std::pair<std::uint8_t, std::string>   pair;
	std::tuple<std::uint16_t, float>       tuple;
	auto Reflect(this auto&& self) { return std::tie(self.numbers, self.words, self.nested, self.pair, self.tuple); }
	bool operator==(const Containers&) const = default;
};

// Singly linked chain of owning pointers, to test the nesting limit.
struct Node {
	std::unique_ptr<Node> next;
	auto Reflect(this auto&& self) { return std::tie(self.next); }
};

// Raw pointer to itself: pointing back at the root is not encodable.
struct SelfRef {
	SelfRef* self = nullptr;
	int      value = 0;
	auto Reflect(this auto&& self) { return std::tie(self.self, self.value); }
};

// Reflectable type whose Codec is specialized: the specialization wins.
struct Legacy {
	int a = 0;
	auto Reflect(this auto&& self) { return std::tie(self.a); }
};

// Non reflectable type, serializable only through its Codec.
struct Opaque {
	int v = 0;
};

namespace Pumpkin::AE {
	// Stored as a single u16 instead of the 4 bytes Reflect() would give.
	template<>
	struct Codec<Legacy> {
		static void Write(BinaryWriter& w, const Legacy& l) { w.write(static_cast<std::uint16_t>(l.a)); }
		static bool Read(BinaryReader& r, Legacy& l) {
			const std::uint16_t a = r.u16();
			if (!r.ok()) return false;
			l.a = a;
			return true;
		}
	};

	// Stored as a single byte.
	template<>
	struct Codec<Opaque> {
		static void Write(BinaryWriter& w, const Opaque& o) { w.write(static_cast<std::uint8_t>(o.v)); }
		static bool Read(BinaryReader& r, Opaque& o) {
			const std::uint8_t b = r.u8();
			if (!r.ok()) return false;
			o.v = b;
			return true;
		}
	};
}
