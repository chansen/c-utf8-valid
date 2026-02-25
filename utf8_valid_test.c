#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#ifdef UTF8_VALID_64
#  include "utf8_valid64.h"
#else
#  include "utf8_valid.h"
#endif


/*
 *  UTF-8
 *
 *     U+0000..U+007F         00..7F
 *                         n  C0..C1  80..BF
 *     U+0080..U+07FF         C2..DF  80..BF
 *                         n  E0      80..9F  80..BF
 *     U+0800..U+D7FF         E0..ED  A0..9F  80..BF
 *     U+D800..U+DFFF      s  ED      A0..BF  80..BF
 *     U+E000..U+FFFF         EE..EF  80..BF  80..BF
 *                         n  F0      80..8F  80..BF  80..BF
 *     U+0800..U+FFFF         F0      80..8F  A0..BF  80..BF
 *    U+10000..U+10FFFF       F0..F4  90..8F  80..BF  80..BF
 *
 *   U-110000..U-1FFFFF    x  F4..F7  90..BF  80..BF  80..BF
 *                         xn F8      80..87  80..BF  80..BF  80..BF
 *   U-200000..U-3FFFFFF   x  F8..FB  88..BF  80..BF  80..BF  80..BF
 *                         xn FC      80..83  80..BF  80..BF  80..BF  80..BF
 *  U-4000000..U-7FFFFFFF  x  FC..FD  84..BF  80..BF  80..BF  80..BF  80..BF
 * 
 *  Legend:
 *    n = Non-shortest form
 *    s = Surrogates
 *    x = Codepoints outside Unicode codespace
 */

/*
 *  Encodes the given ordinal [0, 7FFFFFFF] using the UTF-8 encoding scheme
 *  to the given sequence length [1, 6]. This routine can be used to
 *  produce well-formed and ill-formed UTF-8.
 */
char *
encode_ord(uint32_t ord, size_t len, char *dst) {
  static const uint32_t kMask[6] = { 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
  static const uint32_t kMax[6]  = { 1 <<  7, 1 << 11, 1 << 16,
                                     1 << 21, 1 << 26, 1 << 31 };
  size_t i;

  assert(len >= 1);
  assert(len <= 6);
  assert(ord < kMax[len - 1]);

  for (i = len - 1; i > 0; i--) {
    dst[i] = (ord & 0x3F) | 0x80;
    ord >>= 6;
  }
  dst[0] = ord | kMask[len - 1];
  return dst;
}

char *
escape_str(const char *src, size_t len, char *dst) {
  static const char * const kHex = "0123456789ABCDEF";
  size_t i;
  char *d;

  for (d = dst, i = 0; i < len; i++) {
    const unsigned char c = src[i];
    if (c >= ' ' && c <= '~') {
      if (c == '\\' || c == '"')
        *d++ = '\\';
      *d++ = c;
    }
    else {
      *d++ = '\\';
      *d++ = 'x';
      *d++ = kHex[c >> 4];
      *d++ = kHex[c & 0x0F];
    }
  }
  *d = 0;
  return dst;
}

static size_t TestCount  = 0;
static size_t TestFailed = 0;

void test_utf8(const char* src,
               size_t len,
               size_t exp_spl,
               bool exp_ret,
               size_t exp_cursor,
               unsigned line) {
  char escaped[255 * 4 + 1];
  size_t offset, got_spl;
  bool got_ret;

  assert(len <= 255);

  got_ret = utf8_check(src, len, &offset);

  TestCount++;

  if (got_ret != exp_ret) {
    escape_str(src, len, escaped);
    printf("utf8_check(\"%s\", %u) returned %s, expected %s at line %u\n",
           escaped, (unsigned)len, got_ret ? "true" : "false",
           exp_ret ? "true" : "false", line);
    TestFailed++;
  }

  if (!exp_ret) {
    TestCount++;
    if (offset != exp_cursor) {
      escape_str(src, len, escaped);
      printf("utf8_check(\"%s\", %u) cursor == %u, expected %u at line %u\n",
             escaped, (unsigned)len, (unsigned)offset, (unsigned)exp_cursor,
             line);
      TestFailed++;
    }
  }

  src += offset;
  len -= offset;

  TestCount++;

  got_spl = utf8_maximal_subpart(src, len);

  if (got_spl != exp_spl) {
    escape_str(src, len, escaped);
    printf("utf8_maximal_subpart(\"%s\", %u) == %u, expected %u at line %u\n",
           escaped, (unsigned)len, (unsigned)got_spl, (unsigned)exp_spl, line);
    TestFailed++;
  }
}

#define TEST_UTF8(src, len, subpart, exp, cursor) \
  test_utf8(src, len, subpart, exp, cursor, __LINE__)

#define TEST_VALID(src, len) \
  test_utf8(src, len, 0, true, 0, __LINE__)

#define TEST_INVALID(src, len, subpart, cursor) \
  test_utf8(src, len, subpart, false, cursor, __LINE__)


void
test_empty() {
  TEST_VALID("", 0);
}

void
test_unicode_scalar_value() {
  uint32_t ord;
  char src[4];

  // Unicode scalar value [U+0000, U+007F] 
  for (ord = 0x0000; ord <= 0x007F; ord++) {
    encode_ord(ord, 1, src);
    TEST_VALID(src, 1);
  }

  // Unicode scalar value [U+0080, U+07FF] 
  for (ord = 0x0080; ord <= 0x07FF; ord++) {
    encode_ord(ord, 2, src);
    TEST_VALID(src, 2);
  }

  // Unicode scalar value [U+0800, U+D7FF] and [U+E000, U+FFFF] 
  for (ord = 0x0800; ord <= 0xFFFF && (ord & 0xF800) != 0xD800; ord++) {
    encode_ord(ord, 3, src);
    TEST_VALID(src, 3);
    if ((ord % (1 << 6)) == 0)
      TEST_INVALID(src, 2, 2, 0);
  }

  // Unicode scalar value [U+10000, U+10FFFF] 
  for (ord = 0x10000; ord <= 0x10FFFF; ord++) {
    encode_ord(ord, 4, src);
    TEST_VALID(src, 4);
    if ((ord % (1 << 6)) == 0)
      TEST_INVALID(src, 3, 3, 0);
    if ((ord % (1 << 12)) == 0)
      TEST_INVALID(src, 2, 2, 0);
  }
}

void
test_non_shortest_form() {
  uint32_t ord;
  char src[4];

  // Non-shortest form 2-byte sequence [U+0000, U+007F] 
  for (ord = 0x0000; ord <= 0x007F; ord++) {
    encode_ord(ord, 2, src);
    TEST_INVALID(src, 2, 1, 0);
  }

  // Non-shortest form 3-byte sequence [U+0000, U+07FF] 
  for (ord = 0x0000; ord <= 0x07FF; ord++) {
    encode_ord(ord, 3, src);
    TEST_INVALID(src, 3, 1, 0);
    if ((ord % (1 << 6)) == 0)
      TEST_INVALID(src, 2, 1, 0);
  }

  // Non-shortest form 4-byte sequence [U+0000, U+FFFF] 
  for (ord = 0x0000; ord <= 0xFFFF; ord++) {
    encode_ord(ord, 4, src);
    TEST_INVALID(src, 4, 1, 0);
    if ((ord % (1 << 6)) == 0)
      TEST_INVALID(src, 3, 1, 0);
    if ((ord % (1 << 12)) == 0)
      TEST_INVALID(src, 2, 1, 0);
  }
}

void
test_non_unicode() {
  uint32_t ord;
  char src[4];

  // Code points outside Unicode codespace [U+110000, U+1FFFFF] 
  for (ord = 0x110000; ord <= 0x1FFFFF; ord++) {
    encode_ord(ord, 4, src);
    TEST_INVALID(src, 4, 1, 0);
    if ((ord % (1 << 6)) == 0)
      TEST_INVALID(src, 3, 1, 0);
    if ((ord % (1 << 12)) == 0)
      TEST_INVALID(src, 2, 1, 0);
  }
}

void
test_surrogates() {
  uint32_t ord;
  char src[4];

  // Surrogates [U+D800, U+DFFF] 
  for (ord = 0xD800; ord <= 0xDFFF; ord++) {
    encode_ord(ord, 3, src);
    TEST_INVALID(src, 3, 1, 0);
    if ((ord % (1 << 6)) == 0)
      TEST_INVALID(src, 2, 1, 0);
  }
}

void
test_continuations() {
  uint8_t ord;
  char src[1];

  // Misplaced continuation bytes [\x80, \xBF] 
  for (ord = 0x80; ord <= 0xBF; ord++) {
    src[0] = ord;
    TEST_INVALID(src, 1, 1, 0);
  }
}

void
test_maximal_subpart_edge_cases() {
  // len=0 
  TestCount++;
  if (utf8_maximal_subpart("", 0) != 0) {
    printf("utf8_maximal_subpart(\"\", 0) != 0\n");
    TestFailed++;
  }

  // len=1 with valid ASCII 
  TestCount++;
  if (utf8_maximal_subpart("A", 1) != 1) {
    printf("utf8_maximal_subpart(\"A\", 1) != 1\n");
    TestFailed++;
  }

  // len=1 with lead byte (truncated) 
  TestCount++;
  if (utf8_maximal_subpart("\xC3", 1) != 1) {
    printf("utf8_maximal_subpart(\"\\xC3\", 1) != 1\n");
    TestFailed++;
  }
}

void
test_mixed_sequences() {
  // ASCII followed by multibyte 
  TEST_VALID("hello\xC3\xA9world", 12);

  // ASCII fast path (16 bytes) followed by multibyte 
  TEST_VALID("abcdefghijklmnop\xC3\xA9", 18);

  // Valid followed by invalid surrogate 
  TEST_INVALID("\xC3\xA9\xED\xA0\x80", 5, 1, 2);

  // Valid 3-byte followed by invalid 
  TEST_INVALID("\xE2\x82\xAC\xED\xA0\x80", 6, 1, 3);

  // Multiple valid sequences 
  TEST_VALID("\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80", 9);

  // Valid sequence at exactly 16-byte boundary 
  TEST_VALID("abcdefghijklmno\xC3\xA9", 17);

  // Invalid byte after 16 ASCII bytes 
  TEST_INVALID("abcdefghijklmnop\x80", 17, 1, 16);
}

void
test_5_and_6_byte_sequences() {
  /* 5-byte lead byte F8 - maximal subpart is 1 */
  TEST_INVALID("\xF8\x88\x80\x80\x80", 5, 1, 0);

  /* 6-byte lead byte FC - maximal subpart is 1 */
  TEST_INVALID("\xFC\x84\x80\x80\x80\x80", 6, 1, 0);

  /* FE and FF are never valid */
  TEST_INVALID("\xFE", 1, 1, 0);
  TEST_INVALID("\xFF", 1, 1, 0);
}

static void stream_check(const char* desc,
                         size_t exp_n,
                         size_t exp_cursor,
                         size_t got_n,
                         size_t got_cursor,
                         unsigned line) {
  TestCount++;
  if (got_n != exp_n) {
    printf("utf8_stream_check [%s] returned %zu, expected %zu at line %u\n",
           desc, got_n, exp_n, line);
    TestFailed++;
  }
  if (got_n == (size_t)-1) {
    TestCount++;
    if (got_cursor != exp_cursor) {
      printf("utf8_stream_check [%s] cursor == %zu, expected %zu at line %u\n",
             desc, got_cursor, exp_cursor, line);
      TestFailed++;
    }
  }
}

#define STREAM_OK(desc, st, src, len, eof, exp_n) do { \
  size_t _cursor = 0; \
  size_t _n = utf8_stream_check(st, src, len, eof, &_cursor); \
  stream_check(desc, exp_n, 0, _n, _cursor, __LINE__); \
} while (0)

#define STREAM_ERR(desc, st, src, len, eof, exp_cursor) do { \
  size_t _cursor = 0; \
  size_t _n = utf8_stream_check(st, src, len, eof, &_cursor); \
  stream_check(desc, (size_t)-1, exp_cursor, _n, _cursor, __LINE__); \
} while (0)

void
test_streaming() {
  utf8_stream_t st;

  /* 1. Clean chunk -- no split, no error */
  utf8_stream_init(&st);
  STREAM_OK("clean ASCII chunk", &st, "hello", 5, false, 5);
  STREAM_OK("clean ASCII chunk eof", &st, "world", 5, true, 5);

  /* 2. Clean multibyte chunk */
  utf8_stream_init(&st);
  STREAM_OK("clean 2-byte chunk", &st, "\xC3\xA9\xC3\xA9", 4, true, 4);

  /* 3. 2-byte sequence split across two chunks */
  utf8_stream_init(&st);
  STREAM_OK("2-byte split chunk 1", &st, "\xC3", 1, false, 0);
  STREAM_OK("2-byte split chunk 2", &st, "\xA9", 1, false, 1);

  /* 4. 3-byte sequence split after first byte */
  utf8_stream_init(&st);
  STREAM_OK("3-byte split 1/3", &st, "\xE2", 1, false, 0);
  STREAM_OK("3-byte split 2/3", &st, "\x82", 1, false, 0);
  STREAM_OK("3-byte split 3/3", &st, "\xAC", 1, true, 1);

  /* 5. 4-byte sequence split at every boundary */
  utf8_stream_init(&st);
  STREAM_OK("4-byte split 1/4", &st, "\xF0", 1, false, 0);
  STREAM_OK("4-byte split 2/4", &st, "\x9F", 1, false, 0);
  STREAM_OK("4-byte split 3/4", &st, "\x98", 1, false, 0);
  STREAM_OK("4-byte split 4/4", &st, "\x80", 1, true, 1);

  /* 6. Multiple sequences in one chunk */
  utf8_stream_init(&st);
  STREAM_OK("multi-seq chunk", &st,
            "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80", 9, true, 9);

  /* 7. Invalid byte mid-chunk */
  utf8_stream_init(&st);
  STREAM_ERR("invalid mid-chunk", &st, "ab\x80""cd", 5, false, 2);

  /* 8. Invalid byte at start of chunk */
  utf8_stream_init(&st);
  STREAM_ERR("invalid at start", &st, "\x80""hello", 6, false, 0);

  /* 9. Invalid byte at end of chunk */
  utf8_stream_init(&st);
  STREAM_ERR("invalid at end", &st, "hello\x80", 6, false, 5);

  /* 10. Truncated sequence at EOF */
  utf8_stream_init(&st);
  STREAM_ERR("truncated 2-byte at eof", &st, "\xC3", 1, true, 0);

  utf8_stream_init(&st);
  STREAM_ERR("truncated 3-byte at eof", &st, "\xE2\x82", 2, true, 0);

  utf8_stream_init(&st);
  STREAM_ERR("truncated 4-byte at eof", &st, "\xF0\x9F\x98", 3, true, 0);

  /* 11. Resume after invalid byte */
  utf8_stream_init(&st);
  STREAM_ERR("invalid before resume", &st, "ab\x80", 3, false, 2);
  STREAM_OK("resume after invalid", &st, "cd", 2, true, 2);

  /* 12. Resume after truncated EOF */
  utf8_stream_init(&st);
  STREAM_ERR("truncated before resume", &st, "\xC3", 1, true, 0);
  STREAM_OK("resume after truncated eof", &st, "ok", 2, true, 2);

  /* 13. Empty chunk */
  utf8_stream_init(&st);
  STREAM_OK("empty chunk", &st, "", 0, false, 0);
  STREAM_OK("empty chunk eof", &st, "", 0, true, 0);

  /* 14. Single-byte chunks */
  utf8_stream_init(&st);
  STREAM_OK("single-byte 1", &st, "h", 1, false, 1);
  STREAM_OK("single-byte 2", &st, "i", 1, false, 1);
  STREAM_OK("single-byte 3", &st, "!", 1, true,  1);

  /* 15. Split followed by more valid input */
  utf8_stream_init(&st);
  STREAM_OK("split then more 1", &st, "abc\xC3",   4, false, 3);
  STREAM_OK("split then more 2", &st, "\xA9""def", 4, true,  4);

  /* 16. cursor NULL on error -- should not crash */
  utf8_stream_init(&st);
  {
    size_t n = utf8_stream_check(&st, "\x80", 1, false, NULL);
    TestCount++;
    if (n != (size_t)-1) {
      printf("utf8_stream_check [null cursor] returned %zu, expected -1\n", n);
      TestFailed++;
    }
  }
}

int
main(int argc, char **argv) {
  test_empty();
  test_unicode_scalar_value();
  test_surrogates();
  test_non_shortest_form();
  test_non_unicode();
  test_continuations();
  test_maximal_subpart_edge_cases();
  test_mixed_sequences();
  test_5_and_6_byte_sequences();
  test_streaming();

  if (TestFailed)
    printf("Failed %zu tests of %zu.\n", TestFailed, TestCount);
  else
    printf("Passed %zu tests.\n", TestCount);

  return TestFailed ? 1 : 0;
}
