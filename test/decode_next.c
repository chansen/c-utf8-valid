#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "utf8_dfa64.h"
#include "utf8_decode_next.h"

static size_t TestCount  = 0;
static size_t TestFailed = 0;

#define CHECK(cond, msg) do { \
  TestCount++; \
  if (!(cond)) { \
      printf("FAIL: %s (line %d)\n", msg, __LINE__); \
      TestFailed++; \
  } \
} while (0)

static void test_next_end(void) {
  uint32_t cp = 0x1234;
  CHECK(utf8_decode_next("", 0, &cp) == 0, "next end: returns 0");
  CHECK(cp == 0x1234,                      "next end: cp unchanged");
}

static void test_next_ascii(void) {
  uint32_t cp;
  CHECK(utf8_decode_next("A",    1, &cp) == 1 && cp == 'A',   "next ascii: A");
  CHECK(utf8_decode_next("\x00", 1, &cp) == 1 && cp == 0,     "next ascii: U+0000");
  CHECK(utf8_decode_next("\x7F", 1, &cp) == 1 && cp == 0x7F,  "next ascii: U+007F");
}

static void test_next_2byte(void) {
  uint32_t cp;
  CHECK(utf8_decode_next("\xC2\x80", 2, &cp) == 2 && cp == 0x0080, "next 2byte: U+0080");
  CHECK(utf8_decode_next("\xC3\xA9", 2, &cp) == 2 && cp == 0x00E9, "next 2byte: U+00E9");
  CHECK(utf8_decode_next("\xDF\xBF", 2, &cp) == 2 && cp == 0x07FF, "next 2byte: U+07FF");
}

static void test_next_3byte(void) {
  uint32_t cp;
  CHECK(utf8_decode_next("\xE0\xA0\x80", 3, &cp) == 3 && cp == 0x0800,  "next 3byte: U+0800");
  CHECK(utf8_decode_next("\xE2\x82\xAC", 3, &cp) == 3 && cp == 0x20AC,  "next 3byte: U+20AC");
  CHECK(utf8_decode_next("\xED\x9F\xBF", 3, &cp) == 3 && cp == 0xD7FF,  "next 3byte: U+D7FF");
  CHECK(utf8_decode_next("\xEE\x80\x80", 3, &cp) == 3 && cp == 0xE000,  "next 3byte: U+E000");
  CHECK(utf8_decode_next("\xEF\xBF\xBF", 3, &cp) == 3 && cp == 0xFFFF,  "next 3byte: U+FFFF");
}

static void test_next_4byte(void) {
  uint32_t cp;
  CHECK(utf8_decode_next("\xF0\x90\x80\x80", 4, &cp) == 4 && cp == 0x10000,  "next 4byte: U+10000");
  CHECK(utf8_decode_next("\xF0\x9F\x98\x80", 4, &cp) == 4 && cp == 0x1F600,  "next 4byte: U+1F600");
  CHECK(utf8_decode_next("\xF4\x8F\xBF\xBF", 4, &cp) == 4 && cp == 0x10FFFF, "next 4byte: U+10FFFF");
}

static void test_next_invalid(void) {
  uint32_t cp = 0x1234;
  int n;

  // Bare continuation: maximal subpart = 1 
  n = utf8_decode_next("\x80", 1, &cp);
  CHECK(n == -1,        "next invalid: bare continuation returns -1");
  CHECK(cp == 0x1234,   "next invalid: cp unchanged on error");

  // Another bare continuation 
  n = utf8_decode_next("\xBF", 1, &cp);
  CHECK(n == -1,        "next invalid: 0xBF returns -1");

  // Overlong C0 80: maximal subpart = 1 (C0 is never valid) 
  n = utf8_decode_next("\xC0\x80", 2, &cp);
  CHECK(n == -1,        "next invalid: overlong C0 80");

  // Overlong E0 80 80: E0 valid lead, 80 not valid second byte for E0,
  // maximal subpart = 1
  n = utf8_decode_next("\xE0\x80\x80", 3, &cp);
  CHECK(n == -1,        "next invalid: overlong E0 80");

  // Surrogate ED A0 80: ED valid lead, A0 not valid second for ED,
  // maximal subpart = 1
  n = utf8_decode_next("\xED\xA0\x80", 3, &cp);
  CHECK(n == -1,        "next invalid: surrogate ED A0");

  // Above U+10FFFF: F4 90 80 80 — F4 valid lead, 90 not valid for F4,
  // maximal subpart = 1
  n = utf8_decode_next("\xF4\x90\x80\x80", 4, &cp);
  CHECK(n == -1,        "next invalid: above U+10FFFF");

  // Invalid lead F5: maximal subpart = 1 
  n = utf8_decode_next("\xF5", 1, &cp);
  CHECK(n == -1,        "next invalid: F5");

  // Valid lead then invalid continuation: E2 41
  n = utf8_decode_next("\xE2\x41", 2, &cp);
  CHECK(n == -1,        "next invalid: E2 followed by ASCII");
}

static void test_next_truncated(void) {
  uint32_t cp = 0x1234;
  int n;

  // Truncated 2-byte: maximal subpart = 1 
  n = utf8_decode_next("\xC3", 1, &cp);
  CHECK(n == -1,      "next truncated: 2-byte");
  CHECK(cp == 0x1234, "next truncated: cp unchanged");

  // Truncated 3-byte after 1 byte: maximal subpart = 1 
  n = utf8_decode_next("\xE2", 1, &cp);
  CHECK(n == -1,      "next truncated: 3-byte after lead");

  // Truncated 3-byte after 2 bytes: maximal subpart = 2 
  n = utf8_decode_next("\xE2\x82", 2, &cp);
  CHECK(n == -2,      "next truncated: 3-byte after 2");

  // Truncated 4-byte after 1 byte: maximal subpart = 1 
  n = utf8_decode_next("\xF0", 1, &cp);
  CHECK(n == -1,      "next truncated: 4-byte after lead");

  // Truncated 4-byte after 2 bytes: maximal subpart = 2 
  n = utf8_decode_next("\xF0\x9F", 2, &cp);
  CHECK(n == -2,      "next truncated: 4-byte after 2");

  // Truncated 4-byte after 3 bytes: maximal subpart = 3 
  n = utf8_decode_next("\xF0\x9F\x98", 3, &cp);
  CHECK(n == -3,      "next truncated: 4-byte after 3");
}

static void test_next_sequence(void) {
  // Walk through "A\xC3\xA9\xE2\x82\xAC" (A, é, €) 
  const char *src = "A\xC3\xA9\xE2\x82\xAC";
  size_t len = 6;
  uint32_t cp;
  int n;

  n = utf8_decode_next(src, len, &cp);
  CHECK(n == 1 && cp == 'A', "next seq: A");
  src += n; len -= (size_t)n;

  n = utf8_decode_next(src, len, &cp);
  CHECK(n == 2 && cp == 0x00E9, "next seq: U+00E9");
  src += n; len -= (size_t)n;

  n = utf8_decode_next(src, len, &cp);
  CHECK(n == 3 && cp == 0x20AC, "next seq: U+20AC");
  src += n; len -= (size_t)n;

  CHECK(utf8_decode_next(src, len, &cp) == 0, "next seq: end");
}

static void test_replace_success(void) {
  uint32_t cp;
  // Success case identical to utf8_decode_next 
  CHECK(utf8_decode_next_replace("A", 1, &cp) == 1 && cp == 'A',         "replace: ASCII");
  CHECK(utf8_decode_next_replace("\xC3\xA9", 2, &cp) == 2 && cp == 0xE9, "replace: U+00E9");
  CHECK(utf8_decode_next_replace("", 0, &cp) == 0,                       "replace: end");
}

static void test_replace_invalid(void) {
  uint32_t cp;
  int n;

  // Bare continuation: advance 1, emit U+FFFD 
  n = utf8_decode_next_replace("\x80", 1, &cp);
  CHECK(n == 1 && cp == 0xFFFD, "replace: bare continuation");

  // Overlong: advance 1, emit U+FFFD 
  n = utf8_decode_next_replace("\xC0\x80", 2, &cp);
  CHECK(n == 1 && cp == 0xFFFD, "replace: overlong C0");

  // Surrogate: advance 1, emit U+FFFD 
  n = utf8_decode_next_replace("\xED\xA0\x80", 3, &cp);
  CHECK(n == 1 && cp == 0xFFFD, "replace: surrogate");
}

static void test_replace_truncated(void) {
  uint32_t cp;
  int n;

  // Truncated 2-byte: advance 1, emit U+FFFD 
  n = utf8_decode_next_replace("\xC3", 1, &cp);
  CHECK(n == 1 && cp == 0xFFFD, "replace truncated: 2-byte");

  // Truncated 3-byte after 2: advance 2, emit U+FFFD 
  n = utf8_decode_next_replace("\xE2\x82", 2, &cp);
  CHECK(n == 2 && cp == 0xFFFD, "replace truncated: 3-byte after 2");

  // Truncated 4-byte after 3: advance 3, emit U+FFFD 
  n = utf8_decode_next_replace("\xF0\x9F\x98", 3, &cp);
  CHECK(n == 3 && cp == 0xFFFD, "replace truncated: 4-byte after 3");
}

static void test_replace_sequence(void) {
  // "A \x80 \xC3\xA9" — ASCII, error, valid 2-byte 
  const char *src = "A\x80\xC3\xA9";
  size_t len = 4;
  uint32_t cp;
  int n;

  n = utf8_decode_next_replace(src, len, &cp);
  CHECK(n == 1 && cp == 'A',    "replace seq: A");
  src += n; len -= (size_t)n;

  n = utf8_decode_next_replace(src, len, &cp);
  CHECK(n == 1 && cp == 0xFFFD, "replace seq: U+FFFD for 0x80");
  src += n; len -= (size_t)n;

  n = utf8_decode_next_replace(src, len, &cp);
  CHECK(n == 2 && cp == 0x00E9, "replace seq: U+00E9");
  src += n; len -= (size_t)n;

  CHECK(utf8_decode_next_replace(src, len, &cp) == 0, "replace seq: end");
}

int main(void) {
  test_next_end();
  test_next_ascii();
  test_next_2byte();
  test_next_3byte();
  test_next_4byte();
  test_next_invalid();
  test_next_truncated();
  test_next_sequence();
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
