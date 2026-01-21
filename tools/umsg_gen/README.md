# umsg_gen

Small stdlib-only generator that turns `.umsg` message definitions into C++11 headers.

## Usage

```sh
python3 tools/umsg_gen/umsg_gen.py examples/Generator/state.umsg -o generated/
```

This writes `generated/<struct_name>.hpp`.

If the schema contains a `package` directive, the output is placed under that subdirectory:

- `package foo;` -> `generated/foo/<struct_name>.hpp`
- `package foo.bar;` -> `generated/foo/bar/<struct_name>.hpp`

## Input format (restricted)

A `.umsg` file contains exactly one struct:

```cpp
package demo;

struct state_t {
    uint64_t timestamp;
    double p[3];
    bool ok;
};
```

Supported field types:
- `uint8_t/int8_t`, `uint16_t/int16_t`, `uint32_t/int32_t`, `uint64_t/int64_t`
- `bool`, `float`, `double`
- fixed-size arrays like `double q[4];`

## Output

The generated struct includes:
- `static const uint32_t kMsgHash` (FNV-1a 32-bit of canonicalized schema text)
- `static const size_t kPayloadSize`
- `bool encode(umsg::bufferSpan& payload) const` (capacity-in / length-out)
- `bool decode(umsg::bufferSpan payload)` (permissive: requires at least `kPayloadSize`, ignores trailing bytes)

The generated encode/decode uses `umsg::Writer` and `umsg::Reader` from `marshalling.hpp`.
