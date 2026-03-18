#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "utf8_dfa64.h"
#include "test_common.h"

static utf8_dfa_state_t dfa_run_decode(const char *src, size_t len, uint32_t *cp) {
  utf8_dfa_state_t s = UTF8_DFA_ACCEPT;
  size_t i;
  *cp = 0;
  for (i = 0; i < len; i++)
    s = utf8_dfa_step_decode(s, (unsigned char)src[i], cp);
  return s;
}

#define EXPECT_DECODE(src, len, expected_cp) do { \
  char _esc[255*4+1]; \
  uint32_t _cp; \
  utf8_dfa_state_t _s = dfa_run_decode((src), (len), &_cp); \
  TestCount++; \
  if (_s != UTF8_DFA_ACCEPT) { \
    escape_str((src), (len), _esc); \
    printf("FAIL line %u: expected ACCEPT for \"%s\"\n", __LINE__, _esc); \
    TestFailed++; \
  } else { \
    TestCount++; \
    if (_cp != (uint32_t)(expected_cp)) { \
      escape_str((src), (len), _esc); \
      printf("FAIL line %u: \"%s\" decoded to U+%04X, expected U+%04X\n", \
            __LINE__, _esc, _cp, (uint32_t)(expected_cp)); \
      TestFailed++; \
    } \
  } \
} while (0)

#define CHECK(cond, msg) do { \
  TestCount++; \
  if (!(cond)) { \
    printf("FAIL: %s (line %d)\n", msg, __LINE__); \
    TestFailed++; \
  } \
} while (0)

static void test_unicode_scalar_value(void) {
  char src[4];

  /* U+0000..U+007F: 1-byte */
  for (uint32_t ord = 0x0000; ord <= 0x007F; ord++) {
    encode_ord(ord, 1, src);
    EXPECT_DECODE(src, 1, ord);
  }

  /* U+0080..U+07FF: 2-byte */
  for (uint32_t ord = 0x0080; ord <= 0x07FF; ord++) {
    encode_ord(ord, 2, src);
    EXPECT_DECODE(src, 2, ord);
  }

  /* U+0800..U+D7FF and U+E000..U+FFFF: 3-byte */
  for (uint32_t ord = 0x0800; ord <= 0xFFFF; ord++) {
    if (ord >= 0xD800 && ord <= 0xDFFF) 
      continue;
    encode_ord(ord, 3, src);
    EXPECT_DECODE(src, 3, ord);
  }

  /* U+10000..U+10FFFF: 4-byte */
  for (uint32_t ord = 0x10000; ord <= 0x10FFFF; ord++) {
    encode_ord(ord, 4, src);
    EXPECT_DECODE(src, 4, ord);
  }
}

static void test_truncated(void) {
  uint32_t cp;
  utf8_dfa_state_t st;

  // Truncated 2-byte 
  st = dfa_run_decode("\xC3", 1, &cp);
  CHECK(st != UTF8_DFA_ACCEPT && st != UTF8_DFA_REJECT, "truncated: 2-byte after lead");

  // Truncated 3-byte after 1 byte 
  st = dfa_run_decode("\xE2", 1, &cp);
  CHECK(st != UTF8_DFA_ACCEPT && st != UTF8_DFA_REJECT, "truncated: 3-byte after lead");

  // Truncated 3-byte after 2 bytes 
  st = dfa_run_decode("\xE2\x82", 2, &cp);
  CHECK(st != UTF8_DFA_ACCEPT && st != UTF8_DFA_REJECT, "truncated: 3-byte after 2");

  // Truncated 4-byte after 1 byte 
  st = dfa_run_decode("\xF0", 1, &cp);
  CHECK(st != UTF8_DFA_ACCEPT && st != UTF8_DFA_REJECT, "truncated: 4-byte after lead");

  // Truncated 4-byte after 3 bytes 
  st = dfa_run_decode("\xF0\x9F\x98", 3, &cp);
  CHECK(st != UTF8_DFA_ACCEPT && st != UTF8_DFA_REJECT, "truncated: 4-byte after 3");
}

static void test_step_progression(void) {
  uint32_t cp = 0;
  utf8_dfa_state_t st = UTF8_DFA_ACCEPT;

  st = utf8_dfa_step_decode(st, 0xF0, &cp);
  CHECK(st != UTF8_DFA_ACCEPT && st != UTF8_DFA_REJECT, "progression: F0 intermediate");
  st = utf8_dfa_step_decode(st, 0x9F, &cp);
  CHECK(st != UTF8_DFA_ACCEPT && st != UTF8_DFA_REJECT, "progression: 9F intermediate");
  st = utf8_dfa_step_decode(st, 0x98, &cp);
  CHECK(st != UTF8_DFA_ACCEPT && st != UTF8_DFA_REJECT, "progression: 98 intermediate");
  st = utf8_dfa_step_decode(st, 0x80, &cp);
  CHECK(st == UTF8_DFA_ACCEPT && cp == 0x1F600, "progression: 80 completes U+1F600");
}

int main(void) {
  test_unicode_scalar_value();
  test_truncated();
  test_step_progression();
  return report_results();
}
