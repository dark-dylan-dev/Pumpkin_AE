 <div align="center">

# Pumpkin AE


[![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C?style=for-the-badge&logo=cplusplus)](https://en.cppreference.com/w/cpp/23.html)
[![CMake](https://img.shields.io/badge/CMake-3.30+-064F8C?style=for-the-badge&logo=cmake)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/Platform-Cross--Platform-lightgrey?style=for-the-badge)](#)
[![Benchmark](https://img.shields.io/badge/Benchmark-Performance-orange?style=for-the-badge)](#benchmark)
[![Unit Tests](https://img.shields.io/badge/Unit_Tests-Google_Test-34A853?style=for-the-badge&logo=googletest)](#unit-tests)
[![Assets](https://img.shields.io/badge/Assets-Serialization-blueviolet?style=for-the-badge)](#asset-archive)

**Pumpkin AE** is a lightweight C++23 asset serialization library designed to efficiently serialize, deserialize, and store complex data structures in binary asset archives.

It provides a reflection-based serialization system, binary readers and writers, asset management, and support for preserving pointer relationships across serialization.
</div>



## Features

- **Binary Serialization** - Convert C++ objects into compact binary data.
- **Binary Deserialization** - Reconstruct C++ objects from binary data with bounds checking.
- **Asset Archives** - Store multiple assets in a single archive file.
- **Reflection-Based Serialization** - Automatically serialize custom structures using a `Reflect()` member function.
- **Custom Codecs** - Extend serialization with specialized `Codec<T>` implementations.
- **Pointer Management** - Support for raw pointers, `std::unique_ptr`, and `std::shared_ptr`.
- **Reference Preservation** - Preserve shared references, object identity, and cyclic pointer relationships.
- **Data Integrity** - CRC-32 checksums to detect corrupted asset payloads.
- **Unit Testing** - Automated tests covering binary I/O, codecs, and archive operations.
- **Benchmarking** - Measure creation, encoding, saving, opening, and loading performance.

## Project Structure

```text
Pumpkin_AE/
├── Benchmark/              # Performance benchmarks
├── Example/                # Usage example
├── Library/
│   ├── Include/
│   │   ├── AE/
│   │   │   ├── AssetArchive.hpp
│   │   │   ├── BinaryIO.hpp
│   │   │   ├── Codec.hpp
│   │   │   └── Codec.inl
│   │   └── Platform/
│   │       └── Export.hpp
│   ├── Sources/
│   │   ├── AssetArchive.cpp
│   │   └── BinaryIO.cpp
│   └── CMakeLists.txt
├── UnitTests/               # Google Test unit tests
├── CMakeLists.txt
└── Doxyfile                 # Doxygen configuration
```

## Core Components

### 1. Binary I/O

The `BinaryWriter` and `BinaryReader` classes handle low-level binary serialization and deserialization.

Supported data types include:

| Type | Support |
|---|---|
| Integers | Fixed-width, little-endian |
| Floating-point | IEEE 754 |
| Booleans | 1-byte representation |
| Enums | Underlying integer type |
| Strings | Length-prefixed UTF-8 |
| Containers | `std::vector`, lists, deques, and other ranges |
| Tuples | `std::pair` and `std::tuple` |
| Custom Structures | Reflection-based serialization |
| Pointers | Raw pointers, `unique_ptr`, and `shared_ptr` |

The binary format uses little-endian encoding, independently of the host machine's native endianness.

### 2. Reflection System

Custom structures can be made serializable by implementing a `Reflect()` member function.

This function returns a tuple of references to the fields that should be serialized.

```cpp
struct Position
{
    int x = 0;
    int y = 0;

    auto Reflect(this auto&& self)
    {
        return std::tie(self.x, self.y);
    }
};
```

No additional serialization implementation is required for reflectable structures.

The order of the fields returned by `Reflect()` defines their binary serialization order.

### 3. Asset Archive

The `Writer` and `Reader` classes provide a higher-level interface for managing serialized assets.

**Writer**
- Add assets using unique identifiers.
- Associate each asset with a custom type identifier.
- Automatically calculate CRC-32 checksums.
- Save all assets into a single archive file.

**Reader**
- Open and validate existing archives.
- Retrieve archive directory entries.
- Load individual assets by their identifiers.
- Verify payload integrity using CRC-32.
- Decode assets directly into C++ structures.

### 4. Pointer and Reference Management

Pumpkin AE supports pointer-based object graphs while preserving object identity.

Supported pointer types:
- Raw pointers
- `std::unique_ptr`
- `std::shared_ptr`

The serialization system tracks objects and their references, allowing shared ownership and pointer relationships to survive a serialization round trip.

A maximum pointer nesting depth of 256 is enforced to prevent excessive recursion.

## Usage Example

The following example demonstrates how to serialize and deserialize a custom structure.

```cpp
#include <cstdint>
#include <string>
#include <vector>

#include <AE/Codec.hpp>

struct Item
{
    std::uint64_t id = 0;
    std::string name;
    float weight = 0.0f;

    auto Reflect(this auto&& self)
    {
        return std::tie(
            self.id,
            self.name,
            self.weight
        );
    }
};

int main()
{
    using namespace Pumpkin::AE;

    constexpr std::uint64_t ItemID = 1337;
    constexpr std::uint32_t ItemType = 1;

    Item item{
        .id = ItemID,
        .name = "Sword",
        .weight = 12.5f
    };

    // Serialize and save the asset
    Writer writer;

    if (!writer.Add(ItemID, ItemType, item))
        return 1;

    if (!writer.Save("assets.pae"))
        return 1;

    // Open the archive
    Reader reader;

    if (!reader.Open("assets.pae"))
        return 1;

    // Load and deserialize the asset
    auto loadedItem = reader.Load<Item>(ItemID);

    if (!loadedItem)
        return 1;

    return 0;
}
```

## Building

### Requirements

- C++23 compatible compiler
- CMake 3.30 or newer
- Git
- Google Test (automatically fetched by CMake for the unit tests)

### Clone the Repository

```bash
git clone https://github.com/dark-dylan-dev/Pumpkin_AE.git
cd Pumpkin_AE
```

### Configure the Project

```bash
cmake -S . -B build
```

### Build

```bash
cmake --build build --config Release
```

The project generates the following targets:

| Target | Description |
|---|---|
| `Pumpkin_AE` | Main shared library |
| `Runtime` | Library runtime target, if provided by the build |
| `Example` | Example executable |
| `Benchmark` | Benchmark executable |
| `UnitTests` | Unit test executable |

## Running the Example

After building, run the generated example executable from the build output directory.

The example demonstrates:
- Creating an inventory containing multiple items.
- Storing shared materials.
- Preserving pointer relationships.
- Saving the inventory into an asset archive.
- Loading the archive and reconstructing its data.

## Benchmark

Pumpkin AE includes a dedicated benchmark application to measure the performance of its serialization pipeline.

Three configurations are available:

| Configuration | Inventories | Items per Inventory | Total Items |
|---|---:|---:|---:|
| Small | 10 | 10 | 100 |
| Medium | 100 | 100 | 10,000 |
| Large | 1,000 | 1,000 | 1,000,000 |

Each benchmark performs:
- 1 warm-up iteration.
- 30 measured iterations.

The following operations are measured independently:

| Operation | Description |
|---|---|
| Creation | Generate inventories and their items |
| Encoding | Serialize inventories into binary data |
| Saving | Write the asset archive to disk |
| Opening | Open and validate the archive |
| Loading | Load and deserialize all inventories |

Performance statistics include:
- Average execution time
- Median execution time
- Minimum execution time
- Maximum execution time

All measurements are expressed in milliseconds.

## Unit Tests

The project uses [Google Test](https://github.com/google/googletest) for automated testing.

The test suite covers:

- Binary I/O
- Binary readers and writers
- Codec serialization and deserialization
- Asset archive operations

To run the tests:

```bash
ctest --test-dir build --output-on-failure
```

## Documentation

Pumpkin AE includes a Doxygen configuration file.

To generate the documentation:

```bash
doxygen Doxyfile
```

Generated documentation is placed in the `Docs/` directory.

## Technical Details

| Property | Value |
|---|---|
| Project | Pumpkin AE |
| Language | C++23 |
| Build System | CMake |
| Minimum CMake Version | 3.30 |
| Library Type | Shared Library |
| Binary Endianness | Little-endian |
| Archive Format Version | 1 |
| Checksum | CRC-32 |
| Maximum Pointer Depth | 256 |
| Unit Testing | Google Test |
| Documentation | Doxygen |

## License

No license has currently been specified in this repository.

## Author

Developed by [dark-dylan-dev](https://github.com/dark-dylan-dev) and [0ri0546](https://github.com/0ri0546).

---

**Pumpkin AE** - A C++23 binary serialization and asset archiving library.
