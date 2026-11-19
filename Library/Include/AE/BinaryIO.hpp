#pragma once

#include <algorithm>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Platform/Export.hpp>

/// @file
/// @brief Byte-level encoding of C++ values, used to build asset payloads.
///
/// @par Encoding
/// - Integers: fixed width, little-endian.
/// - bool: 1 byte, 0 or 1. Any other value is rejected on read.
/// - enum: encoded as its underlying integer type.
/// - float / double: IEEE-754 bit pattern stored as u32 / u64.
/// - string: u32 byte length followed by the raw UTF-8 bytes, with no terminator.
/// - Sequences (vector, list, deque...): u32 element count followed by the elements.
/// - pair / tuple: the elements back to back, no prefix.
/// - Reflectable types: the fields returned by Reflect(), in order, back to back.
/// - Pointers (raw, unique_ptr, shared_ptr): u8 tag, 0 = null, 1 = new object (the object
///   follows inline), 2 = reference (the u32 number of an earlier object follows).
///
/// @par Reflect()
/// A type becomes serializable by providing a Reflect() member returning a
/// tuple of references to its fields. Declare it with deducing this so that the
/// same function serves both writing (const) and reading (non-const):
/// @code
/// auto Reflect(this auto&& self) { return std::tie(self.a, self.b); }
/// @endcode
/// The order of the fields in Reflect() is the on-disk schema.
///
/// @par Pointers
/// A pointer field stores the object it points to. Objects are tracked by
/// identity, so sharing, aliasing and cycles survive a round trip.
/// - Nesting deeper than kMaxPointerDepth fails, on write and on read, so a
///   chain of objects cannot overflow the stack.

namespace Pumpkin::AE {

    /// @brief Type exposing a Reflect() member returning a tuple of references to its fields.
    template<typename T>
    concept Reflectable = requires(const T & t) { t.Reflect(); };

    /// @brief Arithmetic types and enums, encoded as fixed-width integers / IEEE-754 floats.
    template<typename T>
    concept Primitive = std::is_arithmetic_v<T> || std::is_enum_v<T>;

    /// @brief Types convertible to std::string_view (std::string, string_view, const char*...).
    template<typename T>
    concept StringLike = std::convertible_to<const T&, std::string_view>;

    /// @brief std::pair, std::tuple and friends. Ranges are excluded.
    template<typename T>
    concept TupleLike = requires { std::tuple_size<T>::value; } && !std::ranges::range<T>;

    template<typename T> inline constexpr bool always_false = false;

    /// @brief True for std::unique_ptr<T, D> where T is not an array type.
    template<typename T> struct IsUniquePtr : std::false_type {};
    template<typename T, typename D> requires (!std::is_array_v<T>)
    struct IsUniquePtr<std::unique_ptr<T, D>> : std::true_type {};

    /// @brief True for std::shared_ptr<T> where T is not an array type.
    template<typename T> struct IsSharedPtr : std::false_type {};
    template<typename T> requires (!std::is_array_v<T>)
    struct IsSharedPtr<std::shared_ptr<T>> : std::true_type {};

    /// @brief Raw or smart pointer to exactly one object, encoded as a presence flag plus the pointee.
    ///
    /// void*, function pointers and arrays are excluded.
    template<typename T>
    concept Indirect =
        (std::is_pointer_v<T> && std::is_object_v<std::remove_pointer_t<T>> && !StringLike<T>)
        || IsUniquePtr<T>::value || IsSharedPtr<T>::value;

    /// @brief Deepest chain of nested pointers accepted by BinaryWriter and BinaryReader.
    inline constexpr std::uint32_t kMaxPointerDepth = 256;

    /// @brief Kind of pointer through which an object was first met (Root: the value being encoded).
    enum class PtrKind : std::uint8_t { Raw, Unique, Shared, Root };

    /// @brief Pointer tags, as encoded on disk.
    inline constexpr std::uint8_t kPtrNull = 0; ///< No object.
    inline constexpr std::uint8_t kPtrNew  = 1; ///< First time this object is met: it follows inline.
    inline constexpr std::uint8_t kPtrRef  = 2; ///< Object already written: its u32 number follows.

    /// @brief One address per type.
    template<typename T> inline char kTypeTag = 0;
    template<typename T> const void* TypeId() { return &kTypeTag<T>; }

    /// @brief Identity of a pointed-to object: its address and its type.
    using ObjectKey = std::pair<const void*, const void*>;

    struct ObjectKeyHash {
        std::size_t operator()(const ObjectKey& k) const noexcept {
            return std::hash<const void*>{}(k.first) * 31 + std::hash<const void*>{}(k.second);
        }
    };

    template<typename T> consteval std::size_t MinEncodedSize();

    /// @brief Sum of the minimum encoded sizes of every element of a tuple type.
    /// @note This helper was generated by AI.
    template<typename Tup>
    consteval std::size_t TupleMinSize() {
        return[]<std::size_t... I>(std::index_sequence<I...>) {
            return (std::size_t{ 0 } + ... +
                MinEncodedSize<std::remove_cvref_t<std::tuple_element_t<I, Tup>>>());
        }(std::make_index_sequence<std::tuple_size_v<Tup>>{});
    }

    /// @brief Minimum number of bytes one encoded T takes. Used by BinaryReader::read()
    ///        to bound sequence counts before allocating.
    template<typename T>
    consteval std::size_t MinEncodedSize() {
        if constexpr (Primitive<T>)               return sizeof(T);
        else if constexpr (StringLike<T>)         return 4; // u32 length
        else if constexpr (Indirect<T>)           return 1; // pointer tag (null)
        else if constexpr (Reflectable<T>)        return TupleMinSize<decltype(std::declval<T&>().Reflect())>();
        else if constexpr (TupleLike<T>)          return TupleMinSize<T>();
        else if constexpr (std::ranges::range<T>) return 4; // u32 count
        else static_assert(always_false<T>, "MinEncodedSize(): type is not serializable");
    }

    /// @brief Appends encoded values to an in-memory buffer.
    class PUMPKIN_API BinaryWriter {
        std::vector<std::uint8_t> m_buf; ///< Encoded bytes.
        bool m_ok = true;                ///< false after the first value that cannot be encoded.
        bool m_first = true;             ///< true until the first write(), whose value is the root.
        std::uint32_t m_depth = 0;       ///< Pointer nesting of the write in progress.
        std::uint32_t m_nextId = 0;      ///< Number given to the next new pointee.
        /// Pointees met so far: their number and the kind of pointer that met them first.
        std::unordered_map<ObjectKey, std::pair<std::uint32_t, PtrKind>, ObjectKeyHash> m_seen;

        void bytes(const void* p, std::size_t n);

        /// @brief Writes a string length or sequence count as a u32.
        void len(const std::size_t n) {
            if (n > std::numeric_limits<std::uint32_t>::max()) { m_ok = false; return; }
            raw(static_cast<std::uint32_t>(n));
        }

        template<typename T>
        void raw(T v) {
            if constexpr (std::is_enum_v<T>) return raw(static_cast<std::underlying_type_t<T>>(v));
            else if constexpr (std::is_floating_point_v<T>) { // IEEE-754 bits as an integer, so the byte swap below also covers floats
                static_assert(sizeof(T) == 4 || sizeof(T) == 8, "unsupported floating-point type");
                if constexpr (sizeof(T) == 4) raw(std::bit_cast<std::uint32_t>(v));
                else                          raw(std::bit_cast<std::uint64_t>(v));
            }
            else {
                if constexpr (std::endian::native == std::endian::big && std::is_integral_v<T>)
                    v = std::byteswap(v); // file format = little endian
                bytes(&v, sizeof v);
            }
        }
    public:
        /// @brief Encodes any supported value. Nested types recurse through their own Reflect().
        template<typename T>
        void write(const T& v) {
            const bool first = std::exchange(m_first, false);
            if (!m_ok) return;
            if constexpr (std::is_class_v<T> && !Indirect<T>) {
                if (first) m_seen.emplace(ObjectKey{ std::addressof(v), TypeId<T>() }, std::pair{ 0u, PtrKind::Root });
            }

            if constexpr (Primitive<T>)         raw(v);
            else if constexpr (StringLike<T>) { const std::string_view s = v; len(s.size()); bytes(s.data(), s.size()); }
            else if constexpr (Indirect<T>) {
                using P = std::remove_cv_t<std::remove_reference_t<decltype(*v)>>;
                constexpr PtrKind kind = IsUniquePtr<T>::value ? PtrKind::Unique
                                       : IsSharedPtr<T>::value ? PtrKind::Shared : PtrKind::Raw;

                if (!v) { raw(kPtrNull); return; }
                const ObjectKey key{ std::addressof(*v), TypeId<P>() };

                if (const auto it = m_seen.find(key); it != m_seen.end()) { // already written: back-reference
                    const PtrKind owner = it->second.second;
                    const bool allowed = kind == PtrKind::Raw    ? owner != PtrKind::Root
                                       : kind == PtrKind::Shared ? owner == PtrKind::Shared
                                       : false;                  // a unique_ptr is never the second pointer to an object
                    if (!allowed) { m_ok = false; return; }
                    raw(kPtrRef);
                    raw(it->second.first);
                    return;
                }

                if (m_depth >= kMaxPointerDepth) { m_ok = false; return; }
                m_seen.emplace(key, std::pair{ m_nextId++, kind }); // registered BEFORE the pointee, so a cycle finds it
                raw(kPtrNew);
                ++m_depth;
                write(*v);
                --m_depth;
            }
            else if constexpr (Reflectable<T>)  std::apply([&](const auto&... f) { (write(f), ...); }, v.Reflect());
            else if constexpr (TupleLike<T>)    std::apply([&](const auto&... f) { (write(f), ...); }, v);
            else if constexpr (std::ranges::range<T>) {
                len(std::ranges::distance(v));
                for (const auto& e : v) write(e);
            }
            else static_assert(always_false<T>, "BinaryWriter::write(): type is not serializable (add Reflect())");
        }

        /// @brief Returns false if a written value could not be encoded
        [[nodiscard]] bool ok() const { return m_ok; }

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
        std::uint32_t m_depth = 0;         ///< Pointer nesting of the read in progress, bounded by kMaxPointerDepth.

        /// @brief Reads n bytes as a little-endian integer, or fails if fewer remain.
        std::uint64_t get(int n);

        /// @brief An object created by read() for a pointer tagged "new".
        struct Obj {
            const void*           type; ///< TypeId<P>() of the pointee.
            void*                 ptr;  ///< The object.
            std::shared_ptr<void> keep; ///< Shared ownership, set only for objects created through a shared_ptr.
            PtrKind               kind; ///< Kind of the pointer that created it.
        };
        std::vector<Obj> m_objs;        ///< Objects created so far, indexed by their number.

        /// @brief Frees a pointee that read() allocated before a failure, then nulls the pointer.
        template<typename P>
        static void drop(P& p) {
            if constexpr (std::is_pointer_v<P>) { delete p; p = nullptr; }
            else p.reset();
        }
    public:
        /// @param b Bytes to read from.
        explicit BinaryReader(std::span<const std::uint8_t> b);

        /// @brief Returns true while every read so far has succeeded.
        [[nodiscard]] bool ok() const;

        /// @brief Returns true if every byte has been consumed. Used to reject trailing data.
        [[nodiscard]] bool atEnd() const;

        std::uint8_t  u8();      ///< @brief Reads an unsigned 8-bit integer.
        std::uint16_t u16();     ///< @brief Reads an unsigned 16-bit integer.
        std::uint32_t u32();     ///< @brief Reads an unsigned 32-bit integer.
        std::uint64_t u64();     ///< @brief Reads an unsigned 64-bit integer.
        bool          boolean(); ///< @brief Reads a boolean. Fails if the byte is not 0 or 1.
        float         f32();     ///< @brief Reads a 32-bit float.
        double        f64();     ///< @brief Reads a 64-bit float.

        /// @brief Reads a string. The length is checked against the remaining bytes before allocating.
        /// @return The string, or an empty string on failure.
        std::string str();

        /// @brief Reads a sequence element count and checks it against the remaining bytes.
        ///
        /// Call this before reserving or looping, so a corrupted count can't
        /// trigger a huge allocation. read() does it for you.
        /// @param minElemSize Minimum encoded size of one element in bytes. Must be at least 1.
        /// @return The count, or 0 on failure.
        std::uint32_t count(std::size_t minElemSize);

        /// @brief Decodes any supported value. Mirror of BinaryWriter::write().
        ///
        /// Sequences are cleared, then filled with push_back(). Check ok() once
        /// after reading a whole structure.
        template<typename T>
        void read(T& v) {
            static_assert(!std::is_const_v<T>,
                "BinaryReader::read(): Reflect() returned const references. "
                "Declare it as `auto Reflect(this auto&& self)` so it is non-const on a non-const object.");

            if constexpr (std::is_same_v<T, bool>)  v = boolean();
            else if constexpr (std::is_enum_v<T>) { std::underlying_type_t<T> u{}; read(u); v = static_cast<T>(u); }
            else if constexpr (std::is_floating_point_v<T>) {
                static_assert(sizeof(T) == 4 || sizeof(T) == 8, "unsupported floating-point type");
                if constexpr (sizeof(T) == 4) v = f32(); else v = f64();
            }
            else if constexpr (std::is_integral_v<T>)          v = static_cast<T>(get(static_cast<int>(sizeof(T))));
            else if constexpr (std::is_same_v<T, std::string>) v = str();
            else if constexpr (Indirect<T>) {
                using P = std::remove_reference_t<decltype(*v)>;
                static_assert(!std::is_const_v<P>, "BinaryReader::read(): cannot read through a pointer to const");
                static_assert(std::is_default_constructible_v<P>, "BinaryReader::read(): the pointee must be default constructible");
                constexpr PtrKind kind = IsUniquePtr<T>::value ? PtrKind::Unique
                                       : IsSharedPtr<T>::value ? PtrKind::Shared : PtrKind::Raw;

                const std::uint8_t tag = u8();
                if (!m_ok || tag == kPtrNull) {
                    // null pointer, or the read already failed
                    if constexpr (std::is_pointer_v<T>) v = nullptr; else v.reset();
                    return;
                }

                // object created earlier
                if (tag == kPtrRef) {
                    const std::uint32_t id = u32();
                    const Obj* o = (m_ok && id < m_objs.size()) ? &m_objs[id] : nullptr;
                    // A unique_ptr can't refer to an existing object, and a shared_ptr can only
                    // share one that was created through a shared_ptr.
                    if (!o || o->type != TypeId<P>() || kind == PtrKind::Unique
                           || (kind == PtrKind::Shared && o->kind != PtrKind::Shared)) {
                        m_ok = false;
                        if constexpr (std::is_pointer_v<T>) v = nullptr; else v.reset();
                        return;
                    }
                    if constexpr (kind == PtrKind::Raw)         v = static_cast<P*>(o->ptr);
                    else if constexpr (kind == PtrKind::Shared) v = std::static_pointer_cast<P>(o->keep);
                    return;
                }

                if (tag != kPtrNew || m_depth >= kMaxPointerDepth) {
                    m_ok = false;
                    if constexpr (std::is_pointer_v<T>) v = nullptr; else v.reset();
                    return;
                }

                if constexpr (std::is_pointer_v<T>)       v = new P{};
                else if constexpr (IsSharedPtr<T>::value) v = std::make_shared<P>();
                else                                      v.reset(new P{});

                Obj obj{ TypeId<P>(), std::addressof(*v), nullptr, kind };
                if constexpr (kind == PtrKind::Shared) obj.keep = v;
                m_objs.push_back(std::move(obj));

                ++m_depth;
                read(*v);
                --m_depth;
                if (!m_ok) drop(v);
            }
            else if constexpr (Reflectable<T>)                 std::apply([&](auto&... f) { (read(f), ...); }, v.Reflect());
            else if constexpr (TupleLike<T>)                   std::apply([&](auto&... f) { (read(f), ...); }, v);
            else if constexpr (std::ranges::range<T>) {
                using E = T::value_type;
                const std::uint32_t n = count(std::max<std::size_t>(1, MinEncodedSize<E>()));
                v.clear();
                if constexpr (requires { v.reserve(n); }) v.reserve(n);
                for (std::uint32_t i = 0; i < n && m_ok; ++i) {
                    E e{};
                    read(e);
                    v.push_back(std::move(e));
                }
            }
            else static_assert(always_false<T>, "BinaryReader::read(): type is not deserializable (add Reflect())");
        }
    };

} // namespace Pumpkin::AE
