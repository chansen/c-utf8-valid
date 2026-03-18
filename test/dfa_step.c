#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#ifdef UTF8_DFA_64
#  include "utf8_dfa64.h"
#else
#  include "utf8_dfa32.h"
#endif

#include "test_common.h"

static utf8_dfa_state_t dfa_run(const char *src, size_t len) {
  return utf8_dfa_run(UTF8_DFA_ACCEPT, (const unsigned char *)src, len);
}

static bool dfa_accepts(const char *src, size_t len) {
  return dfa_run(src, len) == UTF8_DFA_ACCEPT;
}

#define EXPECT_ACCEPT(src, len) do { \
  char _esc[255*4+1]; \
  TestCount++; \
  if (!dfa_accepts((src), (len))) { \
    escape_str((src), (len), _esc); \
    printf("FAIL line %u: expected ACCEPT for \"%s\"\n", __LINE__, _esc); \
    TestFailed++; \
  } \
} while (0)

#define EXPECT_REJECT(src, len) do { \
  char _esc[255*4+1]; \
  TestCount++; \
  if (dfa_accepts((src), (len))) { \
    escape_str((src), (len), _esc); \
    printf("FAIL line %u: expected REJECT for \"%s\"\n", __LINE__, _esc); \
    TestFailed++; \
  } \
} while (0)
  
#define CHECK(cond, msg) do { \
  TestCount++; \
  if (!(cond)) { \
    printf("FAIL: %s (line %d)\n", msg, __LINE__); \
    TestFailed++; \
  } \
} while (0)

static void
test_unicode_scalar_value(void) {
  char src[4];

  /* U+0000..U+007F: 1-byte */
  for (uint32_t ord = 0x0000; ord <= 0x007F; ord++) {
    encode_ord(ord, 1, src);
    EXPECT_ACCEPT(src, 1);
  }

  /* U+0080..U+07FF: 2-byte */
  for (uint32_t ord = 0x0080; ord <= 0x07FF; ord++) {
    encode_ord(ord, 2, src);
    EXPECT_ACCEPT(src, 2);
  }

  /* U+0800..U+D7FF and U+E000..U+FFFF: 3-byte */
  for (uint32_t ord = 0x0800; ord <= 0xFFFF; ord++) {
    if (ord >= 0xD800 && ord <= 0xDFFF) 
      continue;
    encode_ord(ord, 3, src);
    EXPECT_ACCEPT(src, 3);
  }

  /* U+10000..U+10FFFF: 4-byte */
  for (uint32_t ord = 0x10000; ord <= 0x10FFFF; ord++) {
    encode_ord(ord, 4, src);
    EXPECT_ACCEPT(src, 4);
  }
}

static void
test_surrogates(void) {
  char src[3];

  for (uint32_t ord = 0xD800; ord <= 0xDFFF; ord++) {
    encode_ord(ord, 3, src);
    EXPECT_REJECT(src, 3);
  }
}

static void
test_non_shortest_form(void) {
  char src[4];

  /* 2-byte encoding of U+0000..U+007F */
  for (uint32_t ord = 0x0000; ord <= 0x007F; ord++) {
    encode_ord(ord, 2, src);
    EXPECT_REJECT(src, 2);
  }

  /* 3-byte encoding of U+0000..U+07FF */
  for (uint32_t ord = 0x0000; ord <= 0x07FF; ord++) {
    encode_ord(ord, 3, src);
    EXPECT_REJECT(src, 3);
  }

  /* 4-byte encoding of U+0000..U+FFFF */
  for (uint32_t ord = 0x0000; ord <= 0xFFFF; ord++) {
    encode_ord(ord, 4, src);
    EXPECT_REJECT(src, 4);
  }
}

static void
test_non_unicode(void) {
  char src[4];

  /* U+110000..U+1FFFFF: 4-byte encodings above the Unicode ceiling */
  for (uint32_t ord = 0x110000; ord <= 0x1FFFFF; ord++) {
    encode_ord(ord, 4, src);
    EXPECT_REJECT(src, 4);
  }
}

static void
test_bare_continuations(void) {
  char src[1];
  for (unsigned int b = 0x80; b <= 0xBF; b++) {
    src[0] = (char)b;
    EXPECT_REJECT(src, 1);
  }
}

static void
test_5_and_6_byte_leads(void) {
  char src[1];

  for (unsigned int b = 0xF5; b <= 0xFF; b++) {
    src[0] = (char)b;
    EXPECT_REJECT(src, 1);
  }
}

static void test_truncated(void) {
  utf8_dfa_state_t st;

  // Truncated 2-byte 
  st = dfa_run("\xC3", 1);
  CHECK(st != UTF8_DFA_ACCEPT && st != UTF8_DFA_REJECT, "truncated: 2-byte after lead");

  // Truncated 3-byte after 1 byte 
  st = dfa_run("\xE2", 1);
  CHECK(st != UTF8_DFA_ACCEPT && st != UTF8_DFA_REJECT, "truncated: 3-byte after lead");

  // Truncated 3-byte after 2 bytes 
  st = dfa_run("\xE2\x82", 2);
  CHECK(st != UTF8_DFA_ACCEPT && st != UTF8_DFA_REJECT, "truncated: 3-byte after 2");

  // Truncated 4-byte after 1 byte 
  st = dfa_run("\xF0", 1);
  CHECK(st != UTF8_DFA_ACCEPT && st != UTF8_DFA_REJECT, "truncated: 4-byte after lead");

  // Truncated 4-byte after 3 bytes 
  st = dfa_run("\xF0\x9F\x98", 3);
  CHECK(st != UTF8_DFA_ACCEPT && st != UTF8_DFA_REJECT, "truncated: 4-byte after 3");
}

static void test_step_progression(void) {
  utf8_dfa_state_t st = UTF8_DFA_ACCEPT;

  st = utf8_dfa_step(st, 0xF0);
  CHECK(st != UTF8_DFA_ACCEPT && st != UTF8_DFA_REJECT, "progression: F0 intermediate");
  st = utf8_dfa_step(st, 0x9F);
  CHECK(st != UTF8_DFA_ACCEPT && st != UTF8_DFA_REJECT, "progression: 9F intermediate");
  st = utf8_dfa_step(st, 0x98);
  CHECK(st != UTF8_DFA_ACCEPT && st != UTF8_DFA_REJECT, "progression: 98 intermediate");
  st = utf8_dfa_step(st, 0x80);
  CHECK(st == UTF8_DFA_ACCEPT, "progression: 80 completes U+1F600");
}

int
main(void) {
  test_unicode_scalar_value();
  test_surrogates();
  test_non_shortest_form();
  test_non_unicode();
  test_bare_continuations();
  test_5_and_6_byte_leads();
  test_truncated();
  test_step_progression();
  return report_results();
}
