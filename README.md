# Position-Independent Data in C++

***This is very much work in progress. It works fine for an internal use case of mine, but it might have a few rough edges. Use at your own risk!***

This project aims to provide to data
what [position-independent code](https://en.wikipedia.org/wiki/Position-independent_code) is for code.

The goal is to enable complex data structures, which are stored in a blob of memory (this could be a memory-mapped file)
and enable very efficient read-only access.

This is achieved by using relative offsets for all references between objects.

Similar techniques are used by projects like

* [FlatBuffers](https://flatbuffers.dev/)
* [Cap'n Proto](https://capnproto.org/)

In contrast to these, the present project

* only works with C++ and does not require a schema language
* does not have any limitations concerning the blob size (FlatBuffers supports only 2 GiB) and the size of arrays (
  except for the limitations which are caused by the target architecture and available memory and disk space)

## Requirements

- C++20 compiler (tested with GCC and Clang)
- CMake 3.26 or later

## Getting Started

This is a header-only library. Copy the `pid/` directory into your project, or add this repository as a subdirectory in your CMake build, and add the parent directory to your include path.

### Building the Tests

```bash
mkdir build && cd build
cmake ..
cmake --build .
ctest
```

## Core Concepts

The library has two phases:

1. **Build phase** — you construct position-independent data structures in a contiguous byte buffer using `pid::builder`.
2. **Read phase** — you cast the buffer (or a memory-mapped file) to your root type and read it with zero deserialization overhead.

All references between objects are stored as **relative offsets** rather than absolute pointers. This means the entire buffer can be relocated in memory (or loaded from a file) and all references remain valid.

### Offset and Size Types

The library is parameterized over two independent type parameters:

- **OffsetType** (signed) — the integer type used for relative pointers. Determines the maximum distance between a pointer and its target. Use `int8_t` for tiny structs, `int32_t` for blobs up to ~2 GiB, `int64_t` for blobs of (almost) arbitrary size.
- **SizeType** (unsigned) — the integer type used for container lengths. Determines the maximum number of elements in a string, vector, or map.

Pre-configured namespaces provide convenient aliases:

| Namespace | Offset width | Max pointer distance |
|-----------|-------------|----------------------|
| `pid8`    | 8-bit       | ±127 bytes           |
| `pid16`   | 16-bit      | ±32 KiB              |
| `pid32`   | 32-bit      | ±2 GiB               |
| `pid64`   | 64-bit      | ±8 EiB               |

Within each namespace, types like `string32`, `vector64`, `map16` indicate the **size type** (e.g., `string32` has 32-bit length, supporting strings up to ~4 GiB).

The `pid` namespace provides default aliases using 32-bit offsets and 32-bit sizes:

```cpp
pid::ptr<T>              // = pid32::ptr<T>
pid::string              // = pid32::string32
pid::vector<T>           // = pid32::vector32<T>
pid::map<Key, Value>     // = pid32::map32<Key, Value>
```

### How It Works

Traditional C++ data structures use absolute pointers. When you serialize them to disk or share them between processes, these pointers become invalid. This library replaces every pointer with a **signed relative offset** — the distance in bytes from the pointer's own address to its target:

```
Traditional pointer:       [0x7fff4020]  →  object at address 0x7fff4020
Relative offset (pid):     [+24]         →  object 24 bytes after this field
```

Because offsets are relative, the entire memory blob can be loaded at any address and all references still work. This enables:

- **Zero-copy deserialization** — `reinterpret_cast` the buffer and start reading immediately
- **Memory-mapped files** — map a file into memory and use the data directly, no parsing needed
- **Shared memory** — multiple processes can map the same file and read the data concurrently

### Header Files

| Header | Purpose                                                     |
|--------|-------------------------------------------------------------|
| `pid/pid.h` | Read-only types: `ptr`, `string`, `vector`, `map`           |
| `pid/builder.h` | Construction API: `builder`, `builder_ptr`, `add_*` methods |
| `pid/pid-build-datastructures.h` | Automatic conversion from STL types with deduplication      |

For most use cases, you only need to include `pid/builder.h` (which includes `pid/pid.h`) at build time, and `pid/pid.h` alone for read-only access.

## Usage Examples

### Writing Plain Old Data

```cpp
#include <pid/builder.h>

pid::builder b;

pid::builder_ptr<std::int32_t> p = b.add<std::int32_t>();
*p = 42;

// b.data now contains the serialized int32
```

### Writing a Struct with Pointers

```cpp
struct point {
    std::int32_t x;
    std::int32_t y;
};

struct line {
    pid32::ptr<point> a;
    pid32::ptr<point> b;
};

pid::builder b;

auto offset_line = b.add<line>();
auto offset_p1   = b.add<point>();
auto offset_p2   = b.add<point>();

offset_p1->x = 3;
offset_p1->y = 5;
offset_p2->x = 8;
offset_p2->y = 13;

offset_line->a = offset_p1;
offset_line->b = offset_p2;
```

### Reading the Data Back

After building, copy or memory-map the buffer and cast:

```cpp
// b.data() returns a reference to a std::vector<char> containing the serialized blob
const auto& data = b.data();

const line& l = *reinterpret_cast<const line*>(data.data());
std::cout << l.a->x << ", " << l.a->y << std::endl;  // 3, 5
std::cout << l.b->x << ", " << l.b->y << std::endl;  // 8, 13
```

### Strings

```cpp
pid::builder b;

auto s = b.add<pid32::string32>();
auto data = b.add_string("Hello world!");
*s = data;

// Reading:
const pid32::string32& str = *reinterpret_cast<const pid32::string32*>(b.data.data());
std::string_view sv = str;  // implicit conversion to std::string_view
std::cout << str << std::endl;  // "Hello world!"
```

### Vectors

```cpp
pid::builder b;

struct my_data {
    pid32::vector32<std::int32_t> numbers;
};

auto root = b.add<my_data>();
root->numbers = b.add_vector<std::int32_t, std::uint32_t>(3);
root->numbers[0] = 10;
root->numbers[1] = 20;
root->numbers[2] = 30;
```

### Maps (Sorted Associative Containers)

Maps are stored as sorted arrays of key-value pairs with binary search for lookup:

```cpp
pid::builder b;

auto map = b.add<pid32::map32<std::int32_t, pid32::string32>>();
auto map_builder = b.add_map<std::int32_t, pid32::string32, std::uint32_t>(3);
*map = map_builder.items;

// Keys MUST be inserted in sorted order
*map_builder.add_key(1) = b.add_string("one");
*map_builder.add_key(2) = b.add_string("two");
*map_builder.add_key(3) = b.add_string("three");

// Reading:
const auto& m = *reinterpret_cast<const pid32::map32<std::int32_t, pid32::string32>*>(b.data.data());
std::cout << m.at(2) << std::endl;  // "two"
auto it = m.find(1);                // binary search, O(log n)
```

### Automatic Conversion from STL Types

The `pid-build-datastructures.h` header provides `pid::datastructure_builder`, which can automatically convert standard C++ types (`std::string`, `std::vector`, `std::map`, `std::optional`) to their position-independent equivalents with automatic deduplication:

```cpp
#include <pid/pid-build-datastructures.h>

std::map<std::string, std::vector<std::int32_t>> input{
    {"primes", {2, 3, 5, 7}},
    {"fibonacci", {1, 1, 2, 3, 5, 8}}
};

pid::builder b;
pid::datastructure_builder db{b};

auto items = db(input);
auto root = b.add<pid32::map32<pid32::string32, pid32::vector32<std::int32_t>>>();
*root = items;
```

### Parallel Construction with Sub-Builders

For large data sets, you can build independent parts in parallel using separate builders and merge them:

```cpp
pid::builder main_builder;
auto root = main_builder.add<my_root_type>();

// Build a sub-graph on another thread
pid::builder sub_builder;
auto sub_data = sub_builder.add<my_sub_type>();
// ... populate sub_data ...

// Merge into the main builder (must be synchronized)
std::lock_guard lock{mutex};
auto translator = main_builder.add_sub_builder(sub_builder);
root->child = translator(sub_data);  // translates the offset into the main builder
```

## Available Types

### Pointer: `pidN::ptr<T>`

A relative pointer. Stores a signed N-bit offset from itself to the target object. Supports `->`, `*`, and explicit `operator bool()` (null when offset is 0).

### String: `pidN::stringM`

A string with N-bit pointer offset and M-bit length. Implicitly converts to `std::string_view`. Supports `==`, `<=>`, `<<`, `begin()`, `end()`, `size()`, `empty()`.

### Vector: `pidN::vectorM<T>`

A fixed-size array with N-bit pointer offset and M-bit length. Supports `[]`, `at()`, `begin()`, `end()`, `size()`, `empty()`.

### Map: `pidN::mapM<Key, Value>`

A sorted associative container (stored as a sorted vector of pairs). Supports `find()` (binary search), `at()`, `begin()`, `end()`, `size()`.

## License

See [LICENSE](LICENSE).
