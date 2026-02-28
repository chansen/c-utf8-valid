# utf8_valid.h

Fast UTF-8 validation in a single C header, conforming to the Unicode and ISO/IEC 10646:2011 specification.

## Usage

```c
#include "utf8_valid.h"

// Check if a string is valid UTF-8
bool ok = utf8_valid(src, len);

// Check and find the error position
size_t cursor;
if (!utf8_check(src, len, &cursor)) {
  // cursor is the length of the maximal valid prefix
}

// Length of the maximal subpart of an ill-formed sequence
size_t n = utf8_maximal_subpart(src, len);
```

## API

```c
bool   utf8_valid(const char *src, size_t len);
bool   utf8_valid_constant(const char *src, size_t len);
bool   utf8_check(const char *src, size_t len, size_t *cursor);
bool   utf8_check_constant(const char *src, size_t len, size_t *cursor);
size_t utf8_maximal_subpart(const char *src, size_t len);
```

**`utf8_valid`** returns `true` if `src[0..len)` is valid UTF-8.

**`utf8_check`** returns `true` if valid. On failure, if `cursor` is non-NULL, sets `*cursor` to the length of the maximal valid prefix — i.e. the byte offset of the first invalid sequence.

**`utf8_maximal_subpart`** returns the length of the maximal subpart of the ill-formed sequence starting at `src`, as defined by Unicode 6.3 Table 3-8. The return value is always at least 1. This is intended to be called after `utf8_check` reports failure, pointing `src` at the cursor position.

**`utf8_valid_constant`** and **`utf8_check_constant`** are timing-safe
variants that process every byte through the DFA without the ASCII fast
path. Execution time is independent of input content, making them suitable
for security-sensitive contexts where timing side channels are a concern.
Otherwise identical in behaviour to `utf8_valid` and `utf8_check`.

## Streaming API
```c
typedef struct { uint32_t state; } utf8_stream_t;

void   utf8_stream_init(utf8_stream_t *s);
size_t utf8_stream_check(utf8_stream_t* s,
                         const char* src,
                         size_t len,
                         bool eof,
                         size_t* cursor);
```

For validating input that arrives in chunks.

**`utf8_stream_init`** initializes the stream state. Must be called before the first `utf8_stream_check`.

**`utf8_stream_check`** validates the next chunk and returns the number of bytes forming complete, valid sequences. Any bytes beyond the returned count represent an incomplete sequence crossing the chunk boundary and must be prepended to the next chunk by the caller.

If `eof` is `true` and the stream does not end on a sequence boundary, the trailing bytes are treated as ill-formed.

On error, returns `(size_t)-1` and sets `*cursor` (if non-NULL) to the byte offset of the invalid or truncated sequence within `src`. The stream is automatically reset to the initial state so the caller can resume from the next byte without reinitializing.

## Requirements

- C99 or later

## How it works

`utf8_valid.h` implements a shift-based DFA originally described by
[Per Vognsen](https://gist.github.com/pervognsen/218ea17743e1442e59bb60d29b1aa725),
with 32-bit row packing derived by
[Dougall Johnson](https://gist.github.com/dougallj/166e326de6ad4cf2c94be97a204c025f)
using an SMT solver (see `tool/smt_solver.py`).

The DFA has 9 states. Each state is assigned a bit offset within a 32-bit integer. Each byte in the input maps to a row in a 256-entry lookup table; the row encodes all 9 state transitions packed into a single `uint32_t`. The inner loop is a single table lookup and shift:

```c
state = (dfa[byte] >> state) & 31;
```

The error state is fixed at bit offset 0. Since error transitions shift a zero value into the row, they contribute nothing to the row value. The SMT solver finds offsets for the remaining 8 states that fit without collision inside 32 bits.

The fast path scans 16 bytes at a time, skipping the DFA entirely for pure ASCII chunks.

### 64-bit variant

`utf8_valid64.h` provides the same API using a 64-bit table. The error state is fixed at bit offset 0 and contributes nothing to row values. The remaining 8 state offsets are fixed multiples of 6 (6, 12, 18, ..., 48), using 54 bits of the 64-bit row with 10 bits of headroom for future state additions. No solver is needed. This variant is provided for reference and further development.

## License

BSD 2-Clause License. Copyright (c) 2017–2026 Christian Hansen.

## See Also

- [Flexible and Economical UTF-8 Decoder](https://bjoern.hoehrmann.de/utf-8/decoder/dfa/) by Bjoern Hoehrmann
- [Branchless UTF-8 Decoder](https://nullprogram.com/blog/2017/10/06/) by Chris Wellons
- [simdutf](https://github.com/simdutf/simdutf) — Unicode validation and transcoding at SIMD speed
