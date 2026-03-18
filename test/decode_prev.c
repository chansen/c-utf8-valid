#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "utf8_rdfa64.h"
#include "utf8_decode_prev.h"

static size_t TestCount  = 0;
static size_t TestFailed = 0;

#define CHECK(cond, msg) do { \
  TestCount++; \
  if (!(cond)) { \
      printf("FAIL: %s (line %d)\n", msg, __LINE__); \
      TestFailed++; \
  } \
} while (0)

static void test_prev_end(void) {
  uint32_t cp = 0x1234;
  CHECK(utf8_decode_prev("", 0, &cp) == 0, "prev end: returns 0");
  CHECK(cp == 0x1234,                      "prev end: cp unchanged");
}

static void test_prev_ascii(void) {
  uint32_t cp;
  CHECK(utf8_decode_prev("A",    1, &cp) == 1 && cp == 'A',   "prev ascii: A");
  CHECK(utf8_decode_prev("\x00", 1, &cp) == 1 && cp == 0,     "prev ascii: U+0000");
  CHECK(utf8_decode_prev("\x7F", 1, &cp) == 1 && cp == 0x7F,  "prev ascii: U+007F");
}

static void test_prev_2byte(void) {
  uint32_t cp;
  CHECK(utf8_decode_prev("\xC2\x80", 2, &cp) == 2 && cp == 0x0080, "prev 2byte: U+0080");
  CHECK(utf8_decode_prev("\xC3\xA9", 2, &cp) == 2 && cp == 0x00E9, "prev 2byte: U+00E9");
  CHECK(utf8_decode_prev("\xDF\xBF", 2, &cp) == 2 && cp == 0x07FF, "prev 2byte: U+07FF");
}

static void test_prev_3byte(void) {
  uint32_t cp;
  CHECK(utf8_decode_prev("\xE0\xA0\x80", 3, &cp) == 3 && cp == 0x0800, "prev 3byte: U+0800");
  CHECK(utf8_decode_prev("\xE2\x82\xAC", 3, &cp) == 3 && cp == 0x20AC, "prev 3byte: U+20AC");
  CHECK(utf8_decode_prev("\xED\x9F\xBF", 3, &cp) == 3 && cp == 0xD7FF, "prev 3byte: U+D7FF");
  CHECK(utf8_decode_prev("\xEE\x80\x80", 3, &cp) == 3 && cp == 0xE000, "prev 3byte: U+E000");
  CHECK(utf8_decode_prev("\xEF\xBF\xBF", 3, &cp) == 3 && cp == 0xFFFF, "prev 3byte: U+FFFF");
}

static void test_prev_4byte(void) {
  uint32_t cp;
  CHECK(utf8_decode_prev("\xF0\x90\x80\x80", 4, &cp) == 4 && cp == 0x10000,  "prev 4byte: U+10000");
  CHECK(utf8_decode_prev("\xF0\x9F\x98\x80", 4, &cp) == 4 && cp == 0x1F600,  "prev 4byte: U+1F600");
  CHECK(utf8_decode_prev("\xF4\x8F\xBF\xBF", 4, &cp) == 4 && cp == 0x10FFFF, "prev 4byte: U+10FFFF");
}

static void test_prev_invalid(void) {
  uint32_t cp = 0x1234;
  int n;

  // Bare lead byte at end: maximal subpart = 1 
  n = utf8_decode_prev("\xC3", 1, &cp);
  CHECK(n == -1,      "prev invalid: bare lead returns -1");
  CHECK(cp == 0x1234, "prev invalid: cp unchanged on error");

  // Overlong C0 80: C0 is never valid, maximal subpart = 1 
  n = utf8_decode_prev("\xC0\x80", 2, &cp);
  CHECK(n == -1, "prev invalid: overlong C0 80");

  // Overlong E0 80 80: 80 valid cont, 80 valid cont, E0 rejects; subpart = 2 
  n = utf8_decode_prev("\xE0\x80\x80", 3, &cp);
  CHECK(n == -2, "prev invalid: overlong E0 80 80");

  // Surrogate ED A0 80: conts 80+A0 consumed, ED rejects; subpart = 2 
  n = utf8_decode_prev("\xED\xA0\x80", 3, &cp);
  CHECK(n == -2, "prev invalid: surrogate ED A0 80");

  // Above U+10FFFF: F4 90 80 80 — conts 80+80+90 consumed, F4 rejects; subpart = 3 
  n = utf8_decode_prev("\xF4\x90\x80\x80", 4, &cp);
  CHECK(n == -3, "prev invalid: above U+10FFFF");

  // Invalid lead F5: maximal subpart = 1 
  n = utf8_decode_prev("\xF5", 1, &cp);
  CHECK(n == -1, "prev invalid: F5");
}

static void test_prev_truncated(void) {
  uint32_t cp = 0x1234;
  int n;

  // Truncated 2-byte: only continuation at end, no lead; subpart = 1 
  n = utf8_decode_prev("\x80", 1, &cp);
  CHECK(n == -1,      "prev truncated: lone continuation");
  CHECK(cp == 0x1234, "prev truncated: cp unchanged");

  // Truncated 3-byte: two continuations, no lead; subpart = 2 
  n = utf8_decode_prev("\x82\xAC", 2, &cp);
  CHECK(n == -2, "prev truncated: two continuations no lead");

  // Truncated 4-byte: three continuations, no lead; subpart = 3 
  n = utf8_decode_prev("\x80\x98\x9F", 3, &cp);
  CHECK(n == -3, "prev truncated: three continuations no lead");
}

static void test_prev_sequence(void) {
  // Walk "A\xC3\xA9\xE2\x82\xAC" backwards (A, é, €) 
  const char *src = "A\xC3\xA9\xE2\x82\xAC";
  size_t len = 6;
  uint32_t cp;
  int n;

  n = utf8_decode_prev(src, len, &cp);
  CHECK(n == 3 && cp == 0x20AC, "prev seq: U+20AC");
  len -= (size_t)n;

  n = utf8_decode_prev(src, len, &cp);
  CHECK(n == 2 && cp == 0x00E9, "prev seq: U+00E9");
  len -= (size_t)n;

  n = utf8_decode_prev(src, len, &cp);
  CHECK(n == 1 && cp == 'A', "prev seq: A");
  len -= (size_t)n;

  CHECK(utf8_decode_prev(src, len, &cp) == 0, "prev seq: end");
}

static void test_replace_success(void) {
  uint32_t cp;
  CHECK(utf8_decode_prev_replace("A", 1, &cp) == 1 && cp == 'A',         "prev replace: ASCII");
  CHECK(utf8_decode_prev_replace("\xC3\xA9", 2, &cp) == 2 && cp == 0xE9, "prev replace: U+00E9");
  CHECK(utf8_decode_prev_replace("", 0, &cp) == 0,                       "prev replace: end");
}

static void test_replace_invalid(void) {
  uint32_t cp;
  int n;

  // Bare lead: advance 1, emit U+FFFD 
  n = utf8_decode_prev_replace("\xC3", 1, &cp);
  CHECK(n == 1 && cp == 0xFFFD, "prev replace: bare lead");

  // Overlong C0 80: advance 1, emit U+FFFD 
  n = utf8_decode_prev_replace("\xC0\x80", 2, &cp);
  CHECK(n == 1 && cp == 0xFFFD, "prev replace: overlong C0 80");

  // Surrogate ED A0 80: lead rejects after 2 conts, backs up, advance = 2 
  n = utf8_decode_prev_replace("\xED\xA0\x80", 3, &cp);
  CHECK(n == 2 && cp == 0xFFFD, "prev replace: surrogate");
}

static void test_replace_truncated(void) {
  uint32_t cp;
  int n;

  // Lone continuation: advance 1, emit U+FFFD 
  n = utf8_decode_prev_replace("\x80", 1, &cp);
  CHECK(n == 1 && cp == 0xFFFD, "prev replace truncated: lone continuation");

  // Two continuations no lead: advance 2, emit U+FFFD 
  n = utf8_decode_prev_replace("\x82\xAC", 2, &cp);
  CHECK(n == 2 && cp == 0xFFFD, "prev replace truncated: two continuations");

  // Three continuations no lead: advance 3, emit U+FFFD 
  n = utf8_decode_prev_replace("\x80\x98\x9F", 3, &cp);
  CHECK(n == 3 && cp == 0xFFFD, "prev replace truncated: three continuations");
}

static void test_replace_sequence(void) {
  // "\xC3\xA9\xF5\xE2\x82\xAC" backwards: €, error (0xF5), é
  // 0xF5 is an invalid lead — always rejected immediately
  const char *src = "\xC3\xA9\xF5\xE2\x82\xAC";
  size_t len = 6;
  uint32_t cp;
  int n;

  n = utf8_decode_prev_replace(src, len, &cp);
  CHECK(n == 3 && cp == 0x20AC, "prev replace seq: U+20AC");
  len -= (size_t)n;

  n = utf8_decode_prev_replace(src, len, &cp);
  CHECK(n == 1 && cp == 0xFFFD, "prev replace seq: U+FFFD for 0xF5");
  len -= (size_t)n;

  n = utf8_decode_prev_replace(src, len, &cp);
  CHECK(n == 2 && cp == 0x00E9, "prev replace seq: U+00E9");
  len -= (size_t)n;

  CHECK(utf8_decode_prev_replace(src, len, &cp) == 0, "prev replace seq: end");
}

int main(void) {
  test_prev_end();
  test_prev_ascii();
  test_prev_2byte();
  test_prev_3byte();
  test_prev_4byte();
  test_prev_invalid();
  test_prev_truncated();
  test_prev_sequence();
  test_replace_success();
  test_replace_invalid();
  test_replace_truncated();
  test_replace_sequence();

  if (TestFailed)
    printf("Failed %zu of %zu tests.\n", TestFailed, TestCount);
  else
    printf("All %zu tests passed.\n", TestCount);

  return TestFailed ? 1 : 0;
}
