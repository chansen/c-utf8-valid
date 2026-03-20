# c-utf8

A header-only UTF-8 library in C implementing validation, decoding, and 
transcoding conforming to the Unicode and ISO/IEC 10646 specifications.

## Usage

```c
#include "utf8_dfa32.h"   // or utf8_dfa64.h for the 64-bit variant
#include "utf8_valid.h"

// Check if a string is valid UTF-8
bool ok = utf8_valid(src, len);

// Check and find the error position
size_t cursor;
if (!utf8_check(src, len, &cursor)) {
  // cursor is the byte offset of the first ill-formed sequence
}

// Length of the maximal subpart of an ill-formed sequence
size_t n = utf8_maximal_subpart(src, len);
```

## Requirements

- C99 or later

## DFA backends

All routines in this library are driven by a Deterministic Finite Automaton
(DFA) that consumes one byte at a time and transitions between a small set of
states. Four backend headers are provided; you choose one or two based on
what the consumer headers you include actually need.

### Forward DFA

The forward DFA scans bytes left-to-right starting from the `ACCEPT` state.
Each byte indexes into a 256-entry lookup table and produces the next state.
Returning to `ACCEPT` marks the end of a complete valid sequence. Reaching
`REJECT` means an ill-formed byte was encountered; that state is a permanent
trap and no further input can leave it.

The 9 forward DFA states are:

| State    | Meaning |
| :---     | :--- |
| `ACCEPT` | Start / valid sequence boundary |
| `REJECT` | Ill-formed input (absorbing trap) |
| `TAIL1`  | Awaiting 1 more continuation byte |
| `TAIL2`  | Awaiting 2 more continuation bytes |
| `TAIL3`  | Awaiting 3 more continuation bytes |
| `E0`     | After `0xE0`; next must be `A0–BF` (reject non-shortest form) |
| `ED`     | After `0xED`; next must be `80–9F` (reject surrogates) |
| `F0`     | After `0xF0`; next must be `90–BF` (reject non-shortest form) |
| `F4`     | After `0xF4`; next must be `80–8F` (reject above U+10FFFF) |

The four special states (`E0`, `ED`, `F0`, `F4`) enforce Unicode constraints
that a naive continuation-byte count cannot express: non-shortest-form
encodings, surrogate halves (U+D800–U+DFFF), and codepoints above U+10FFFF
are all structurally rejected.

### Reverse DFA

The reverse DFA scans bytes right-to-left and is used for backward navigation
and backward decoding and moving a cursor back by N codepoints.

Going forward you see the lead byte first and know immediately how many
continuation bytes follow and what constraints apply to them. Going in
reverse you see continuation bytes before the lead, so the DFA tracks how
many have been seen and remembers the range of the outermost continuation
byte (the one adjacent to the lead), because that range determines which lead
bytes are valid.

The reverse DFA uses 7 states, 2 fewer than the forward DFA:

| State    | Meaning |
| :---     | :--- |
| `ACCEPT` | Start / valid sequence boundary |
| `REJECT` | Ill-formed input (absorbing trap) |
| `TL1`    | Seen 1 continuation byte |
| `TL2LO`  | Seen 2 continuations; outermost was `80–9F` |
| `TL2HI`  | Seen 2 continuations; outermost was `A0–BF` |
| `TL3LO`  | Seen 3 continuations; outermost was `80–8F` |
| `TL3MH`  | Seen 3 continuations; outermost was `90–BF` |

The LO/HI and LO/MH split on the outermost continuation range enforces the
same constraints as the forward DFA's `E0`, `ED`, `F0`, and `F4` states,
applied when the lead byte finally arrives rather than when it is first seen.

### Shift-based DFA encoding

All four backends implement the shift-based DFA approach originally described
by [Per Vognsen](https://gist.github.com/pervognsen/218ea17743e1442e59bb60d29b1aa725).
Each byte maps to a row in a 256-entry table. The row is a packed integer
where each state's transition target is stored at a fixed bit offset. The
current state value is used directly as the shift amount to extract the next
state, avoiding a second lookup. The inner loop is a table load, a shift, and 
a mask:

```c
state = (table[byte] >> state) & mask;
```

The 32-bit row packing technique was derived by
[Dougall Johnson](https://gist.github.com/dougallj/166e326de6ad4cf2c94be97a204c025f);
state offsets are chosen by an SMT solver (`tool/smt_solver.py`) to pack all
transitions without collision inside a `uint32_t`.

The 64-bit forward backend uses state offsets at fixed multiples of 6
(6, 12, 18, ..., 48), packing all transitions into 54 bits of a `uint64_t`.
Bits 56–62 store a per-byte payload mask that the decode step uses to
accumulate codepoint bits in the same pass:

```c
*codepoint = (*codepoint << 6) | (byte & (row >> 56));
state      = (row >> state) & 63;
```

ASCII rows carry mask `0x7F`, 2/3/4-byte lead rows carry `0x1F`/`0x0F`/`0x07`,
continuation rows carry `0x3F`, and error rows carry `0x00`.

The 64-bit reverse backend uses the same payload mask scheme. The sequence
depth is encoded in the state value itself, so the bit shift for codepoint
accumulation can be derived without a separate lookup:

```c
shift      = ((state >> 2) & 3) * 6;
*codepoint = *codepoint | (byte & (row >> 56)) << shift;
state      = (row >> state) & 63;
```

### Choosing a backend

#### Forward backends

| Header         | Row type   | Table size | Decoding |
| :---           | :---       | :---       | :---     |
| `utf8_dfa32.h` | `uint32_t` | 1 KB       | No       |
| `utf8_dfa64.h` | `uint64_t` | 2 KB       | Yes      |

Use `utf8_dfa32.h` for validation-only work: `utf8_valid.h`,
`utf8_valid_stream.h`, `utf8_distance.h`, `utf8_advance_forward.h`.

Use `utf8_dfa64.h` when codepoint decoding or transcoding is needed:
`utf8_decode_next.h`, `utf8_transcode.h`.

#### Reverse backends

| Header          | Row type   | Table size | Decoding |
| :---            | :---       | :---       | :---     |
| `utf8_rdfa32.h` | `uint32_t` | 1 KB       | No       |
| `utf8_rdfa64.h` | `uint64_t` | 2 KB       | Yes      |

Use `utf8_rdfa32.h` for backward navigation: `utf8_advance_backward.h`.

Use `utf8_rdfa64.h` for backward decoding: `utf8_decode_prev.h`.

#### Summary

The 64-bit variants are supersets of the 32-bit variants — include exactly
one forward backend and, if you need backward traversal, exactly one reverse
backend.

| You need                   | Forward              | Reverse               |
| :---                       | :---                 | :---                  |
| Validate                   | `utf8_dfa{32,64}.h`  | —                     |
| Count codepoints           | `utf8_dfa{32,64}.h`  | —                     |
| Streaming validation       | `utf8_dfa{32,64}.h`  | —                     |
| Skip forward N codepoints  | `utf8_dfa{32,64}.h`  | —                     |
| Decode forward             | `utf8_dfa64.h`       | —                     |
| Transcode to UTF-16/32     | `utf8_dfa64.h`       | —                     |
| Skip backward N codepoints | —                    | `utf8_rdfa{32,64}.h`  |
| Decode backward            | —                    | `utf8_rdfa64.h`       |

## API

The library is split into independent header files. Each requires exactly one
DFA backend to be included first.

---

### Validation — `utf8_valid.h`

Requires `utf8_dfa32.h` or `utf8_dfa64.h`.

```c
bool   utf8_valid(const char *src, size_t len);
bool   utf8_valid_ascii(const char *src, size_t len);
bool   utf8_check(const char *src, size_t len, size_t *cursor);
bool   utf8_check_ascii(const char *src, size_t len, size_t *cursor);
size_t utf8_maximal_subpart(const char *src, size_t len);
```

**`utf8_valid`** returns `true` if `src[0..len)` is valid UTF-8.

**`utf8_check`** returns `true` if `src[0..len)` is valid UTF-8. On failure,
if `cursor` is non-NULL, sets `*cursor` to the byte offset of the first
ill-formed sequence (the length of the maximal valid prefix).

**`utf8_valid_ascii`** and **`utf8_check_ascii`** are drop-in replacements
with a 16-byte ASCII fast path that skips the DFA for chunks containing only
ASCII bytes. Behaviour is identical to `utf8_valid` and `utf8_check`.
Throughput advantage depends on content mix and microarchitecture; see the
[Performance](#performance) section.

**`utf8_maximal_subpart`** returns the length of the maximal subpart of the
ill-formed sequence starting at `src[0..len)`, as defined by Unicode 
(see [Error handling and U+FFFD replacement](#error-handling-and-ufffd-replacement)). 
The return value is always >= 1. Call this after `utf8_check`reports failure, 
with `src` advanced to the cursor position.

---

### Streaming validation — `utf8_valid_stream.h`

Requires `utf8_dfa32.h` or `utf8_dfa64.h`.

```c
typedef enum {
  UTF8_VALID_STREAM_OK,         // src fully consumed, no errors
  UTF8_VALID_STREAM_PARTIAL,    // src fully consumed, ends mid-sequence
  UTF8_VALID_STREAM_ILLFORMED,  // stopped at an ill-formed subsequence
  UTF8_VALID_STREAM_TRUNCATED,  // eof is true and src ends mid-sequence
} utf8_valid_stream_status_t;

typedef struct {
  utf8_valid_stream_status_t status;
  size_t consumed;            // bytes read from src.
  size_t advance;             // bytes to skip on ILLFORMED or TRUNCATED, else 0
} utf8_valid_stream_result_t;

typedef struct {
  utf8_dfa_state_t state;
  size_t pending;
} utf8_valid_stream_t;

void   utf8_valid_stream_init(utf8_valid_stream_t *s);
utf8_valid_stream_result_t utf8_valid_stream_check(utf8_valid_stream_t *s,
                                                   const char *src, 
                                                   size_t len,
                                                   bool eof);
```

**`utf8_valid_stream_init`** initialises a stream validator. Call this before
the first `utf8_valid_stream_check`.

**`utf8_valid_stream_check`** validates `src[0..len)` as the next chunk of a
UTF-8 byte stream. `eof` should be `true` only for the final chunk. The DFA
state is carried in `utf8_valid_stream_t` across calls.

If the chunk is well-formed and ends on a sequence boundary, status is `OK`.
If the chunk ends in the middle of a sequence and `eof` is `false`, status is
`PARTIAL`. In both cases, `consumed` is the number of bytes read from `src`
and `advance` is 0.

If validation stops at an ill-formed sequence, status is `ILLFORMED`. If
`eof` is `true` and the chunk ends in the middle of a sequence, status is
`TRUNCATED`.

On `UTF8_VALID_STREAM_ILLFORMED` or `UTF8_VALID_STREAM_TRUNCATED`, the stream
state resets to `ACCEPT` automatically so the caller can continue without
reinitialising.

On `ILLFORMED` or `TRUNCATED`, `consumed` is the byte offset of the ill-formed 
or truncated sequence and `advance` is the number of bytes in the current 
chunk that belong to it. Resume validation at `src[consumed + advance]`.

```c
utf8_valid_stream_t s;
utf8_valid_stream_init(&s);

size_t stream_offset = 0;
bool valid = true;

while (valid && (len = read_chunk(buf, sizeof buf)) > 0) {
  bool eof = len < sizeof buf;
  utf8_valid_stream_result_t r = utf8_valid_stream_check(&s, buf, len, eof);

  switch (r.status) {
  case UTF8_VALID_STREAM_OK:
  case UTF8_VALID_STREAM_PARTIAL:
    stream_offset += len;
    break;
  case UTF8_VALID_STREAM_ILLFORMED:
  case UTF8_VALID_STREAM_TRUNCATED:
    stream_offset += r.consumed + r.advance;
    handle_error(stream_offset);
    valid = false;
    break;
  }
}
```

---

### Codepoint count — `utf8_distance.h`

Requires `utf8_dfa32.h` or `utf8_dfa64.h`.

```c
size_t utf8_distance(const char *src, size_t len);
```

**`utf8_distance`** returns the number of Unicode codepoints in
`src[0..len)`, or `(size_t)-1` if the input contains ill-formed UTF-8.

---

### Forward navigation — `utf8_advance_forward.h`

Requires `utf8_dfa32.h` or `utf8_dfa64.h`.

```c
size_t utf8_advance_forward(const char *src,
                            size_t len,
                            size_t distance,
                            size_t *advanced);
```

**`utf8_advance_forward`** returns the byte offset of the codepoint
`distance` positions ahead within `src[0..len)`. Returns `len` if `distance`
exceeds the number of codepoints in the buffer. Returns `(size_t)-1` if the
input contains ill-formed UTF-8.

If `advanced` is non-NULL, sets `*advanced` to the number of codepoints
actually skipped before stopping.

---

### Backward navigation — `utf8_advance_backward.h`

Requires `utf8_rdfa32.h` or `utf8_rdfa64.h`.

```c
size_t utf8_advance_backward(const char *src, 
                             size_t len,
                             size_t distance, 
                             size_t *advanced);
```

**`utf8_advance_backward`** returns the byte offset of the codepoint
`distance` positions before the end of `src[0..len)`. Returns `0` if
`distance` exceeds the number of codepoints in the buffer. Returns
`(size_t)-1` if the input contains ill-formed UTF-8.

If `advanced` is non-NULL, sets `*advanced` to the number of codepoints
actually skipped before stopping.

---

### Forward decoding — `utf8_decode_next.h`

Requires `utf8_dfa64.h`.

```c
int utf8_decode_next(const char *src, size_t len, uint32_t *codepoint);
int utf8_decode_next_replace(const char *src, size_t len, uint32_t *codepoint);
```

**`utf8_decode_next`** decodes the codepoint starting at `src[0]`.

- **Success:** returns bytes consumed (1–4) and writes the codepoint to
  `*codepoint`.
- **End of input** (`len == 0`): returns `0`; `*codepoint` is unchanged.
- **Ill-formed sequence:** returns the negated length of the maximal subpart
  (see [Error handling and U+FFFD replacement](#error-handling-and-ufffd-replacement)),
  in the range `-1..-3`. Advance by `-return_value`
  bytes and call again. `*codepoint` is unchanged.

**`utf8_decode_next_replace`** is identical but on error writes U+FFFD to
`*codepoint` and returns the maximal subpart length as a positive value.
Never returns a negative value; returns `0` only when `len` is 0.

```c
while (len > 0) {
  uint32_t cp;
  int n = utf8_decode_next_replace(src, len, &cp);
  if (n == 0)
    break;
  process(cp); // U+FFFD for any ill-formed sequence
  src += n; 
  len -= n;
}
```

---

### Backward decoding — `utf8_decode_prev.h`

Requires `utf8_rdfa64.h`.

```c
int utf8_decode_prev(const char *src, size_t len, uint32_t *codepoint);
int utf8_decode_prev_replace(const char *src, size_t len, uint32_t *codepoint);
```

**`utf8_decode_prev`** decodes the codepoint ending at `src[len-1]`.

- **Success:** returns bytes consumed (1–4) and writes the codepoint to
  `*codepoint`. Step back by the return value: next call uses
  `src[0..len-return_value)`.
- **End of input** (`len == 0`): returns `0`; `*codepoint` is unchanged.
- **Ill-formed sequence:** returns the negated number of bytes to step back,
  in the range `-1..-3`. Step back by `-return_value` bytes and call again.
  `*codepoint` is unchanged.

**`utf8_decode_prev_replace`** is identical but on error writes U+FFFD to
`*codepoint` and returns the step-back distance as a positive value.

The reverse DFA sees continuation bytes before the lead byte. For some
ill-formed inputs (e.g. two lone continuation bytes `\x80\x80`),
`utf8_decode_prev` may produce fewer U+FFFD substitutions than
`utf8_decode_next` over the same bytes, because all bytes consumed before
rejection are reported as one ill-formed unit rather than individually.

---

### Transcoding — `utf8_transcode.h`

Requires `utf8_dfa64.h`.

```c
typedef enum {
    UTF8_TRANSCODE_OK,         // src fully consumed, no errors
    UTF8_TRANSCODE_EXHAUSTED,  // dst full before src was consumed
    UTF8_TRANSCODE_ILLFORMED,  // stopped at an ill-formed sequence
    UTF8_TRANSCODE_TRUNCATED,  // src ends mid-sequence
} utf8_transcode_status_t;

typedef struct {
    utf8_transcode_status_t status;
    size_t consumed;   // bytes read from src
    size_t decoded;    // codepoints decoded from src
    size_t written;    // code units written to dst
    size_t advance;    // bytes to skip on ILLFORMED or TRUNCATED, else 0
} utf8_transcode_result_t;

utf8_transcode_result_t utf8_transcode_utf16(const char *src, size_t src_len,
                                             uint16_t *dst, size_t dst_len);
utf8_transcode_result_t utf8_transcode_utf16_replace(const char *src, size_t src_len,
                                                     uint16_t *dst, size_t dst_len);
utf8_transcode_result_t utf8_transcode_utf32(const char *src, size_t src_len,
                                             uint32_t *dst, size_t dst_len);
utf8_transcode_result_t utf8_transcode_utf32_replace(const char *src, size_t src_len,
                                                     uint32_t *dst, size_t dst_len);
```

**`utf8_transcode_utf16`** and **`utf8_transcode_utf32`** transcode
`src[0..src_len)` into `dst[0..dst_len)`, stopping at the first ill-formed
or truncated sequence. Codepoints above U+FFFF are encoded as surrogate
pairs in UTF-16 and consume two code units.

**`utf8_transcode_utf16_replace`** and **`utf8_transcode_utf32_replace`**
replace each ill-formed sequence with U+FFFD and continue. Status is always
`OK` or `EXHAUSTED`; `advance` is always 0.

On `ILLFORMED` or `TRUNCATED`, `consumed` is the byte offset of the
ill-formed sequence and `advance` is the length of its maximal subpart
(see [Error handling and U+FFFD replacement](#error-handling-and-ufffd-replacement)).
Resume transcoding at `src[consumed + advance]`.

```c
// Simple case: replace ill-formed sequences with U+FFFD
uint16_t dst[256];
utf8_transcode_result_t r;
do {
  r = utf8_transcode_utf16_replace(src, src_len, dst, sizeof dst);
  flush(dst, r.written);
  src     += r.consumed;
  src_len -= r.consumed;
} while (r.status == UTF8_TRANSCODE_EXHAUSTED);

// Strict case: stop on ill-formed input
while (src_len > 0) {
  utf8_transcode_result_t r = utf8_transcode_utf16(src, src_len, dst, sizeof dst);
  flush(dst, r.written);

  switch (r.status) {
  case UTF8_TRANSCODE_OK:
  case UTF8_TRANSCODE_EXHAUSTED:
    src     += r.consumed;
    src_len -= r.consumed;
    break;
  case UTF8_TRANSCODE_ILLFORMED:
  case UTF8_TRANSCODE_TRUNCATED:
    handle_error(src + r.consumed, r.advance);
    src     += r.consumed + r.advance;
    src_len -= r.consumed + r.advance;
    break;
  }

  if (r.status == UTF8_TRANSCODE_OK ||
    r.status == UTF8_TRANSCODE_TRUNCATED)
    break;
}
```

For UTF-32, `result.written` always equals `result.decoded` since each
codepoint maps to exactly one code unit. For UTF-16, `result.written` may
exceed `result.decoded` due to surrogate pairs for codepoints above U+FFFF.

## Error handling and U+FFFD replacement

When processing untrusted input, any ill-formed sequence should be replaced
with U+FFFD (REPLACEMENT CHARACTER). Skipping ill-formed sequences can have
security implications; see Unicode Technical Report #36,
[Unicode Security Considerations](https://www.unicode.org/reports/tr36/tr36-15.html).
Ill-formed sequences should be replaced with U+FFFD or the input rejected
outright.

### Maximal subpart

The number of U+FFFD characters to emit per ill-formed sequence is determined
by the *maximal subpart* rule, defined in [Unicode 17.0 Table 3-8](https://www.unicode.org/versions/Unicode17.0.0/core-spec/chapter-3/#G66453). 
A maximal subpart is the longest prefix of an ill-formed sequence that is 
either the start of an otherwise well-formed sequence, or a single byte. Each 
maximal subpart produces exactly one U+FFFD.

For example, the byte sequence `\xF0\x80\x80` is a truncated 4-byte sequence:
`\xF0` is a valid 4-byte lead followed by two valid continuation bytes, but a
third continuation byte is missing. All three bytes form one maximal subpart
and produce one U+FFFD. Two lone continuation bytes `\x80\x80`, on the other
hand, each form their own maximal subpart of length 1 and produce two U+FFFD.

### Standards requirements

[Unicode 17.0 §3.9 recommends](https://www.unicode.org/versions/Unicode17.0.0/core-spec/chapter-3/#G66453), 
and the [WHATWG Encoding Standard](https://encoding.spec.whatwg.org/#utf-8-decoder)
requires, that decoders replace each maximal subpart of an ill-formed
sequence with exactly one U+FFFD. This is the behaviour implemented by
`utf8_decode_next_replace`,
`utf8_transcode_utf16_replace`, and `utf8_transcode_utf32_replace`.

The non-replace variants (`utf8_decode_next`, `utf8_transcode_utf16`,
`utf8_transcode_utf32`) stop and report the error position instead. These are
intended for applications that need to handle ill-formed input explicitly,
such as validating input at a trust boundary, logging the error location, or
applying a custom substitution policy. If you have no such requirement, prefer
the `_replace` variants.

## Security

All routines in this library are safe to use on untrusted input.

**All decoded codepoints are within the Unicode scalar value range.** The DFA
structurally rejects non-shortest-form encodings, surrogate halves 
(U+D800–U+DFFF), and codepoints above U+10FFFF. These cannot appear in the 
output of any decoding or transcoding function.

**No dynamic allocation.** All functions operate on caller-supplied buffers
with no heap allocation, no global mutable state, and no use of `errno`.

**No data-dependent branches on byte value.** The DFA step is a table lookup
and bitwise shift with no conditional branches that depend on the input byte.
Execution time scales with the number of bytes processed, not their values.
Note that `utf8_valid_ascii` and `utf8_check_ascii` are exceptions; they
branch on whether a chunk contains only ASCII bytes, making their execution
time content-dependent.

## Performance

### Corpus

Documents are retrieved from Wikipedia and converted to plain text, available in `benchmark/corpus/`.

| File   |   Size | Code points | Distribution          | Best `utf8_valid` | Best `utf8_valid_ascii` |
| :---   |   ---: |        ---: | :---                  |      ---: |       ---: |
| ar.txt |  25 KB |         14K | 19% ASCII, 81% 2-byte | 4102 MB/s |  5103 MB/s |
| el.txt | 102 KB |         59K | 23% ASCII, 77% 2-byte | 4104 MB/s |  5006 MB/s |
| en.txt |  80 KB |         82K | 99.9% ASCII           | 4108 MB/s | 33103 MB/s |
| ja.txt | 176 KB |         65K | 11% ASCII, 89% 3-byte | 4109 MB/s |  5223 MB/s |
| lv.txt | 135 KB |        127K | 92% ASCII, 7% 2-byte  | 4111 MB/s |  6151 MB/s |
| ru.txt | 148 KB |         85K | 23% ASCII, 77% 2-byte | 4109 MB/s |  4010 MB/s |
| sv.txt |  94 KB |         93K | 96% ASCII, 4% 2-byte  | 4104 MB/s |  8775 MB/s |

Best numbers from Clang 20 `-O3 -march=x86-64-v3` (Raptor Lake).

---

### Raptor Lake (Clang 20, x86-64)

| Flags                  | `utf8_valid` | `utf8_valid_ascii` | Notes                                     |
| :---                   |         ---: |               ---: | :---                                      |
| `-O2`                  |    2710 MB/s |          2849 MB/s | ascii fast path competitive on multibyte  |
| `-O2 -march=x86-64-v3` |    4097 MB/s |          3982 MB/s | BMI2 `SHRX` eliminates `CL` constraint    |
| `-O3 -march=x86-64-v3` |    4102 MB/s |          5103 MB/s | fast path profitable on all content types |

Numbers shown for ar.txt (81% 2-byte). On near-pure ASCII (en.txt) `utf8_valid_ascii`
reaches 25–33 GB/s at `-O3`.

### Raptor Lake (GCC 14, x86-64)

| Flags                  | `utf8_valid` | `utf8_valid_ascii` | Notes                                    |
| :---                   |         ---: |               ---: | :---                                     |
| `-O2`                  |    2731 MB/s |          2642 MB/s | ascii fast path competitive on multibyte |
| `-O2 -march=x86-64-v3` |    4103 MB/s |          3996 MB/s | BMI2 `SHRX` eliminates `CL` constraint   |
| `-O3 -march=x86-64-v3` |    4106 MB/s |          3979 MB/s | essentially equal to `-O2` with SHRX     |

Numbers shown for ar.txt (81% 2-byte). On near-pure ASCII (en.txt) `utf8_valid_ascii`
reaches 36–39 GB/s at `-O3`.

### Haswell (Clang 22, x86-64)

| Flags                  | `utf8_valid` | `utf8_valid_ascii` | Notes                                  |
| :---                   |         ---: |               ---: | :---                                   |
| `-O2`                  |    2096 MB/s |          1669 MB/s | ascii fast path hurts on multibyte     |
| `-O2 -march=x86-64-v3` |    3449 MB/s |          2564 MB/s | BMI2 `SHRX` eliminates `CL` constraint |
| `-O3 -march=x86-64-v3` |    3450 MB/s |          3162 MB/s | gap narrows at `-O3`                   |

Numbers shown for ar.txt (81% 2-byte). On near-pure ASCII (en.txt) `utf8_valid_ascii`
reaches 14–18 GB/s at all optimization levels.

### Apple M1 (Clang 21, AArch64)

| Flags                 | `utf8_valid` | `utf8_valid_ascii` | Notes                              |
| :---                  |         ---: |               ---: | :---                               |
| `-O2`                 |    2774 MB/s |          2646 MB/s | essentially equal on multibyte     |
| `-O2 -mtune=apple-m1` |    2714 MB/s |          2701 MB/s | essentially equal on multibyte     |
| `-O3`                 |    2798 MB/s |          5033 MB/s | O3 unlocks NEON fast path          |
| `-O3 -mtune=apple-m1` |    2771 MB/s |          4977 MB/s | mtune negligible at `-O3`          |

Numbers shown for ar.txt (81% 2-byte). On near-pure ASCII (en.txt) `utf8_valid_ascii`
reaches ~20 GB/s at `-O3`.

### Apple M1 (GCC 15, AArch64)

| Flags                 | `utf8_valid` | `utf8_valid_ascii` | Notes                           |
| :---                  |         ---: |               ---: | :---                            |
| `-O2`                 |    2797 MB/s |          2669 MB/s | comparable to Clang             |
| `-O2 -mtune=apple-m1` |    2754 MB/s |          2633 MB/s | mtune no effect                 |
| `-O3`                 |    2729 MB/s |          2588 MB/s | essentially equal to `-O2`      |
| `-O3 -mtune=apple-m1` |    2778 MB/s |          2659 MB/s | essentially equal on multibyte  |

Numbers shown for ar.txt (81% 2-byte). On near-pure ASCII (en.txt) `utf8_valid_ascii`
reaches ~35 GB/s at `-O3`.

### Observations

- `utf8_valid` is the most consistent performer across content mixes and
  compilers in these measurements. Across all tested platforms, `utf8_valid`
  processes approximately one byte per clock cycle at peak throughput,
  consistent with the unrolled DFA loop executing one table lookup and one shift
  per byte.
- On x86, `-march=x86-64-v3` or `-march=native` enables BMI2 `SHRX`, which
  removes the variable-shift dependency on `CL` and roughly doubles throughput
  for `utf8_valid`.
- `utf8_valid_ascii` profitability on multibyte content is microarchitecture
  dependent. On Haswell it is slower than `utf8_valid` on multibyte-heavy input
  at all tested flags. On Raptor Lake and Apple M1 with Clang `-O3` it is
  faster on all content types.
- On Apple M1 with Clang, `utf8_valid_ascii` is essentially equal to
  `utf8_valid` on multibyte content at `-O2`, and significantly faster at `-O3`.
- On Apple M1 with GCC 15, both implementations perform comparably to Clang
  at `-O2`. `utf8_valid_ascii` on near-pure ASCII reaches ~35 GB/s at `-O3`.

## Benchmark

The benchmark is in `benchmark/bench.c`. Compile and run with:

```sh
make bench
make bench BENCH_OPTFLAGS="-O3 -march=x86-64-v3"
./bench                        # benchmarks benchmark/corpus/
./bench -d <directory>         # benchmarks all .txt files in directory
./bench -f <file>              # benchmarks single file
./bench -s <MB>                # resize input to <MB> before benchmarking
```

**Benchmark mode** (mutually exclusive):

| Flag        | Description                                                |
| :---        | :---                                                       |
| `-t <secs>` | run each implementation for `<secs>` seconds (default: 20) |
| `-n <reps>` | run each implementation for `<reps>` repetitions           |
| `-b <MB>`   | run each implementation until `<MB>` total data processed  |

**Warmup** (`-w <n>`): before timing, each implementation is run for `n`
iterations to warm up caches and branch predictors. By default the warmup
count is derived from input size, targeting approximately 256 MB of warmup
data, capped between 1 and 100 iterations. Use `-w 0` to disable warmup
entirely.

**Output format:** For each file, the header line shows the filename,
byte size, code point count, and average bytes per code point
(`units/point`). The code point distribution breaks down the input by
Unicode range. Results are sorted slowest to fastest; the percentage
shows improvement over the slowest implementation.

```
sv.txt: 94 KB; 93K code points; 1.04 units/point
  U+0000..U+007F          90K  96.4%
  U+0080..U+07FF           3K   3.5%
  U+0800..U+FFFF          171   0.2%
  hoehrmann                    422 MB/s
  utf8_valid_old              1746 MB/s  (+314%)
  utf8_valid                  2778 MB/s  (+559%)
  utf8_valid_ascii           10012 MB/s  (+2274%)
```

`units/point` is a rough content-mix indicator: `1.00` is near-pure ASCII,
~`1.7–1.9` is common for 2-byte-heavy text, and ~`2.7–3.0` for CJK-heavy text.

The benchmark includes two reference implementations: `hoehrmann` a widely used
table-driven DFA implementation and `utf8_valid_old` (previous scalar decoder)
to track regression and quantify gains from the current DFA approach.

## License

MIT License. Copyright (c) 2017–2026 Christian Hansen.

## See Also

- [Flexible and Economical UTF-8 Decoder](https://bjoern.hoehrmann.de/utf-8/decoder/dfa/) by Björn Höhrmann
- [Branchless UTF-8 Decoder](https://nullprogram.com/blog/2017/10/06/) by Chris Wellons
- [simdutf](https://github.com/simdutf/simdutf) Unicode validation and transcoding using SIMD
