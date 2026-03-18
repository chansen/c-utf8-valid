#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "utf8_dfa64.h"
#include "utf8_transcode.h"

static size_t TestCount  = 0;
static size_t TestFailed = 0;

#define CHECK(cond, msg)          \
  do {                            \
    TestCount++;                  \
    if (!(cond)) {                \
      printf("FAIL: %s (line %d)\n", msg, __LINE__); \
      TestFailed++;               \
    }                             \
  } while (0)

static void test_decode_empty(void) {
  uint32_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf32("", 0, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "decode empty: status OK");
  CHECK(r.consumed == 0,                 "decode empty: consumed 0");
  CHECK(r.decoded  == 0,                 "decode empty: decoded 0");
  CHECK(r.advance  == 0,                 "decode empty: advance 0");
}

static void test_decode_ascii(void) {
  uint32_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf32("ABC", 3, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "decode ascii: status OK");
  CHECK(r.consumed == 3,                 "decode ascii: consumed 3");
  CHECK(r.decoded  == 3,                 "decode ascii: decoded 3");
  CHECK(dst[0] == 'A',                   "decode ascii: dst[0]");
  CHECK(dst[1] == 'B',                   "decode ascii: dst[1]");
  CHECK(dst[2] == 'C',                   "decode ascii: dst[2]");
}

static void test_decode_multibyte(void) {
  // "é€𐍈" = U+00E9 U+20AC U+10348 
  const char *src = "\xC3\xA9\xE2\x82\xAC\xF0\x90\x8D\x88";
  uint32_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf32(src, 9, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "decode multibyte: status OK");
  CHECK(r.consumed == 9,                 "decode multibyte: consumed 9");
  CHECK(r.decoded  == 3,                 "decode multibyte: decoded 3");
  CHECK(dst[0] == 0x00E9,                "decode multibyte: U+00E9");
  CHECK(dst[1] == 0x20AC,                "decode multibyte: U+20AC");
  CHECK(dst[2] == 0x10348,               "decode multibyte: U+10348");
}

static void test_decode_exhausted(void) {
  // dst too small 
  uint32_t dst[2];
  utf8_transcode_result_t r = utf8_transcode_utf32("ABC", 3, dst, 2);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "decode exhausted: status");
  CHECK(r.consumed == 2,                        "decode exhausted: consumed 2");
  CHECK(r.decoded  == 2,                        "decode exhausted: decoded 2");
  CHECK(dst[0] == 'A',                          "decode exhausted: dst[0]");
  CHECK(dst[1] == 'B',                          "decode exhausted: dst[1]");
}

static void test_decode_dst_exact(void) {
  // dst exactly fits all codepoints
  uint32_t dst[3];
  utf8_transcode_result_t r = utf8_transcode_utf32("ABC", 3, dst, 3);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "decode dst exact: status OK");
  CHECK(r.consumed == 3,                 "decode dst exact: consumed 3");
  CHECK(r.decoded  == 3,                 "decode dst exact: decoded 3");
}

static void test_decode_exhausted_resume(void) {
  // Decode "ABCD" two codepoints at a time 
  const char *src = "ABCD";
  size_t len = 4;
  uint32_t dst[2];
  utf8_transcode_result_t r;

  r = utf8_transcode_utf32(src, len, dst, 2);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "decode resume: first status");
  CHECK(r.consumed == 2,                        "decode resume: first consumed");
  CHECK(r.decoded  == 2,                        "decode resume: first decoded");
  src += r.consumed;
  len -= r.consumed;

  r = utf8_transcode_utf32(src, len, dst, 2);
  CHECK(r.status   == UTF8_TRANSCODE_OK,        "decode resume: second status");
  CHECK(r.consumed == 2,                        "decode resume: second consumed");
  CHECK(r.decoded  == 2,                        "decode resume: second decoded");
  CHECK(dst[0] == 'C',                          "decode resume: dst[0]");
  CHECK(dst[1] == 'D',                          "decode resume: dst[1]");
}

static void test_decode_illformed(void) {
  // "A\x80B": bare continuation in the middle 
  uint32_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf32("A\x80" "B", 3, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_ILLFORMED, "decode illformed: status");
  CHECK(r.consumed == 1,                        "decode illformed: consumed 1 (A)");
  CHECK(r.decoded  == 1,                        "decode illformed: decoded 1");
  CHECK(r.advance  == 1,                        "decode illformed: advance 1");
  CHECK(dst[0] == 'A',                          "decode illformed: dst[0]");
}

static void test_decode_illformed_resume(void) {
  // Resume after ILLFORMED by skipping subpart 
  const char *src = "A\x80" "B";
  size_t len = 3;
  uint32_t dst[4];
  utf8_transcode_result_t r;

  r = utf8_transcode_utf32(src, len, dst, 4);
  CHECK(r.status == UTF8_TRANSCODE_ILLFORMED, "illformed resume: first call");
  src += r.consumed + r.advance;
  len -= r.consumed + r.advance;

  r = utf8_transcode_utf32(src, len, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK,  "illformed resume: second call");
  CHECK(r.decoded  == 1,                  "illformed resume: decoded 1");
  CHECK(dst[0]     == 'B',                "illformed resume: dst[0]");
}

static void test_decode_illformed_lead_then_bad_cont(void) {
  // E2 41: valid lead, invalid continuation, advance = 1 (just E2) 
  uint32_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf32("\xE2\x41", 2, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_ILLFORMED, "illformed lead+bad: status");
  CHECK(r.consumed == 0,                        "illformed lead+bad: consumed 0");
  CHECK(r.decoded  == 0,                        "illformed lead+bad: decoded 0");
  CHECK(r.advance  == 1,                        "illformed lead+bad: advance 1");

  // F0 9F 41: two valid bytes then bad continuation, advance = 2 
  r = utf8_transcode_utf32("\xF0\x9F\x41", 3, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_ILLFORMED, "illformed 2valid+bad: status");
  CHECK(r.consumed == 0,                        "illformed 2valid+bad: consumed 0");
  CHECK(r.decoded  == 0,                        "illformed 2valid+bad: decoded 0");
  CHECK(r.advance  == 2,                        "illformed 2valid+bad: advance 2");
}

static void test_decode_truncated(void) {
  // Truncated 3-byte sequence: E2 82 with no third byte 
  uint32_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf32("\xE2\x82", 2, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_TRUNCATED, "decode truncated: status");
  CHECK(r.consumed == 0,                        "decode truncated: consumed 0");
  CHECK(r.decoded  == 0,                        "decode truncated: decoded 0");
  CHECK(r.advance  == 2,                        "decode truncated: advance 2");
}

static void test_decode_truncated_after_valid(void) {
  // "A" then truncated 2-byte: \xC3 is 1 byte, advance = 1 
  uint32_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf32("A\xC3", 2, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_TRUNCATED, "truncated after valid: status");
  CHECK(r.consumed == 1,                        "truncated after valid: consumed 1");
  CHECK(r.decoded  == 1,                        "truncated after valid: decoded 1");
  CHECK(r.advance  == 1,                        "truncated after valid: advance 1");
  CHECK(dst[0]     == 'A',                      "truncated after valid: dst[0]");
}

static void test_replace_clean(void) {
  uint32_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf32_replace("ABC", 3, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace clean: status OK");
  CHECK(r.consumed == 3,                 "replace clean: consumed 3");
  CHECK(r.decoded  == 3,                 "replace clean: decoded 3");
}

static void test_replace_bare_cont(void) {
  // \x80 replaced with U+FFFD 
  uint32_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf32_replace("\x80", 1, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace bare cont: status OK");
  CHECK(r.consumed == 1,                 "replace bare cont: consumed 1");
  CHECK(r.decoded  == 1,                 "replace bare cont: decoded 1");
  CHECK(dst[0]     == 0xFFFD,            "replace bare cont: U+FFFD");
}

static void test_replace_mixed(void) {
  // "A\x80B": A, U+FFFD for \x80, B 
  uint32_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf32_replace("A\x80" "B", 3, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace mixed: status OK");
  CHECK(r.consumed == 3,                 "replace mixed: consumed 3");
  CHECK(r.decoded  == 3,                 "replace mixed: decoded 3");
  CHECK(dst[0]     == 'A',               "replace mixed: dst[0]");
  CHECK(dst[1]     == 0xFFFD,            "replace mixed: dst[1]");
  CHECK(dst[2]     == 'B',               "replace mixed: dst[2]");
}

static void test_replace_lead_bad_cont(void) {
  // E2 41: U+FFFD for E2, then 'A' 
  uint32_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf32_replace("\xE2\x41", 2, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace lead+bad: status OK");
  CHECK(r.consumed == 2,                 "replace lead+bad: consumed 2");
  CHECK(r.decoded  == 2,                 "replace lead+bad: decoded 2");
  CHECK(dst[0]     == 0xFFFD,            "replace lead+bad: dst[0] U+FFFD");
  CHECK(dst[1]     == 'A',               "replace lead+bad: dst[1] A");
  // F0 9F 41: two valid bytes then bad continuation
  // U+FFFD for F0 9F, then 'A' */
  r = utf8_transcode_utf32_replace("\xF0\x9F\x41", 3, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace 2valid+bad: status OK");
  CHECK(r.consumed == 3,                 "replace 2valid+bad: consumed 3");
  CHECK(r.decoded  == 2,                 "replace 2valid+bad: decoded 2");
  CHECK(dst[0]     == 0xFFFD,            "replace 2valid+bad: dst[0] U+FFFD");
  CHECK(dst[1]     == 'A',               "replace 2valid+bad: dst[1] A");
}

static void test_replace_truncated(void) {
  // Truncated sequence at end: emit U+FFFD 
  uint32_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf32_replace("\xE2\x82", 2, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace truncated: status OK");
  CHECK(r.consumed == 2,                 "replace truncated: consumed 2");
  CHECK(r.decoded  == 1,                 "replace truncated: decoded 1");
  CHECK(dst[0]     == 0xFFFD,            "replace truncated: U+FFFD");
}

static void test_replace_exhausted(void) {
  // dst fills up mid-input, remaining src not consumed 
  uint32_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf32_replace("ABCD", 4, dst, 2);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "replace exhausted: status");
  CHECK(r.consumed == 2,                        "replace exhausted: consumed 2");
  CHECK(r.decoded  == 2,                        "replace exhausted: decoded 2");

  // dst exactly fits
  r = utf8_transcode_utf32_replace("ABCD", 4, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace exact: status OK");
  CHECK(r.decoded  == 4,                 "replace exact: decoded 4");
}

static void test_replace_surrogate(void) {
  // ED A0 80 (U+D800 encoded as surrogate):
  // maximal subparts are 1+1+1, producing three U+FFFD per Unicode
  uint32_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf32_replace("\xED\xA0\x80", 3, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace surrogate: status OK");
  CHECK(r.consumed == 3,                 "replace surrogate: consumed 3");
  CHECK(r.decoded  == 3,                 "replace surrogate: decoded 3");
  CHECK(dst[0]     == 0xFFFD,            "replace surrogate: dst[0] U+FFFD");
  CHECK(dst[1]     == 0xFFFD,            "replace surrogate: dst[1] U+FFFD");
  CHECK(dst[2]     == 0xFFFD,            "replace surrogate: dst[2] U+FFFD");
}

int main(void) {
  test_decode_empty();
  test_decode_ascii();
  test_decode_multibyte();
  test_decode_exhausted();
  test_decode_dst_exact();
  test_decode_exhausted_resume();
  test_decode_illformed();
  test_decode_illformed_resume();
  test_decode_illformed_lead_then_bad_cont();
  test_decode_truncated();
  test_decode_truncated_after_valid();
  test_replace_clean();
  test_replace_bare_cont();
  test_replace_mixed();
  test_replace_lead_bad_cont();
  test_replace_truncated();
  test_replace_exhausted();
  test_replace_surrogate();

  if (TestFailed)
    printf("Failed %zu of %zu tests.\n", TestFailed, TestCount);
  else
    printf("All %zu tests passed.\n", TestCount);

  return TestFailed ? 1 : 0;
}
