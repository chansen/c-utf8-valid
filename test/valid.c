#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#ifdef UTF8_DFA_64
#  include "utf8_dfa64.h"
#else
#  include "utf8_dfa32.h"
#endif

#include "utf8_valid.h"
#include "test_common.h"

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
  char src[4];

  // Unicode scalar value [U+0000, U+007F] 
  for (uint32_t ord = 0x0000; ord <= 0x007F; ord++) {
    encode_ord(ord, 1, src);
    TEST_VALID(src, 1);
  }

  // Unicode scalar value [U+0080, U+07FF] 
  for (uint32_t ord = 0x0080; ord <= 0x07FF; ord++) {
    encode_ord(ord, 2, src);
    TEST_VALID(src, 2);
  }

  // Unicode scalar value [U+0800, U+D7FF] and [U+E000, U+FFFF] 
  for (uint32_t ord = 0x0800; ord <= 0xFFFF; ord++) {
    if ((ord & 0xF800) == 0xD800)
      continue;    
    encode_ord(ord, 3, src);
    TEST_VALID(src, 3);
    if ((ord % (1 << 6)) == 0)
      TEST_INVALID(src, 2, 2, 0);
  }

  // Unicode scalar value [U+10000, U+10FFFF] 
  for (uint32_t ord = 0x10000; ord <= 0x10FFFF; ord++) {
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
  char src[4];

  // Non-shortest form 2-byte sequence [U+0000, U+007F] 
  for (uint32_t ord = 0x0000; ord <= 0x007F; ord++) {
    encode_ord(ord, 2, src);
    TEST_INVALID(src, 2, 1, 0);
  }

  // Non-shortest form 3-byte sequence [U+0000, U+07FF] 
  for (uint32_t ord = 0x0000; ord <= 0x07FF; ord++) {
    encode_ord(ord, 3, src);
    TEST_INVALID(src, 3, 1, 0);
    if ((ord % (1 << 6)) == 0)
      TEST_INVALID(src, 2, 1, 0);
  }

  // Non-shortest form 4-byte sequence [U+0000, U+FFFF] 
  for (uint32_t ord = 0x0000; ord <= 0xFFFF; ord++) {
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
  char src[4];

  // Code points outside Unicode codespace [U+110000, U+1FFFFF] 
  for (uint32_t ord = 0x110000; ord <= 0x1FFFFF; ord++) {
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
  char src[4];

  // Surrogates [U+D800, U+DFFF] 
  for (uint32_t ord = 0xD800; ord <= 0xDFFF; ord++) {
    encode_ord(ord, 3, src);
    TEST_INVALID(src, 3, 1, 0);
    if ((ord % (1 << 6)) == 0)
      TEST_INVALID(src, 2, 1, 0);
  }
}

void
test_continuations() {
  char src[1];

  // Misplaced continuation bytes [\x80, \xBF] 
  for (uint8_t b = 0x80; b <= 0xBF; b++) {
    src[0] = b;
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

void
test_ascii() {
  size_t cursor;

  // Valid inputs
  TestCount++;
  if (!utf8_valid_ascii("hello", 5)) {
    printf("utf8_valid_ascii: ASCII rejected\n");
    TestFailed++;
  }

  TestCount++;
  if (!utf8_valid_ascii("\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80", 9)) {
    printf("utf8_valid_ascii: multibyte rejected\n");
    TestFailed++;
  }

  // Invalid inputs
  TestCount++;
  if (utf8_valid_ascii("\x80", 1)) {
    printf("utf8_valid_ascii: bare continuation accepted\n");
    TestFailed++;
  }

  TestCount++;
  if (utf8_valid_ascii("\xED\xA0\x80", 3)) {
    printf("utf8_valid_ascii: surrogate accepted\n");
    TestFailed++;
  }

  // Cursor
  TestCount++;
  if (utf8_check_ascii("ab\x80", 3, &cursor) || cursor != 2) {
    printf("utf8_check_ascii: cursor == %zu, expected 2\n", cursor);
    TestFailed++;
  }

  TestCount++;
  if (utf8_check_ascii("\xC3\xA9\xF3\xC5", 4, &cursor) || cursor != 2) {
    printf("utf8_check_ascii: cursor == %zu, expected 2\n", cursor);
    TestFailed++;
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
  test_ascii();
  return report_results();
}
