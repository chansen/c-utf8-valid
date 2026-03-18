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

static void test_empty(void) {
  uint16_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf16("", 0, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "empty: status OK");
  CHECK(r.consumed == 0,                 "empty: consumed 0");
  CHECK(r.decoded  == 0,                 "empty: decoded 0");
  CHECK(r.written  == 0,                 "empty: written 0");
  CHECK(r.advance  == 0,                 "empty: advance 0");
}

static void test_ascii(void) {
  uint16_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf16("ABC", 3, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "ascii: status OK");
  CHECK(r.consumed == 3,                 "ascii: consumed 3");
  CHECK(r.decoded  == 3,                 "ascii: decoded 3");
  CHECK(r.written  == 3,                 "ascii: written 3");
  CHECK(dst[0] == 'A',                   "ascii: dst[0]");
  CHECK(dst[1] == 'B',                   "ascii: dst[1]");
  CHECK(dst[2] == 'C',                   "ascii: dst[2]");
}

static void test_bmp(void) {
  // "é€" = U+00E9 U+20AC — both in BMP, one unit each 
  const char *src = "\xC3\xA9\xE2\x82\xAC";
  uint16_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf16(src, 5, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "bmp: status OK");
  CHECK(r.consumed == 5,                 "bmp: consumed 5");
  CHECK(r.decoded  == 2,                 "bmp: decoded 2");
  CHECK(r.written  == 2,                 "bmp: written 2");
  CHECK(dst[0] == 0x00E9,                "bmp: U+00E9");
  CHECK(dst[1] == 0x20AC,                "bmp: U+20AC");
}

static void test_surrogate_pair(void) {
  // U+10348 GOTHIC LETTER HWAIR: F0 90 8D 88 -> D800 DF48 
  const char *src = "\xF0\x90\x8D\x88";
  uint16_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf16(src, 4, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "surrogate pair: status OK");
  CHECK(r.consumed == 4,                 "surrogate pair: consumed 4");
  CHECK(r.decoded  == 1,                 "surrogate pair: decoded 1");
  CHECK(r.written  == 2,                 "surrogate pair: written 2");
  CHECK(dst[0] == 0xD800,                "surrogate pair: high surrogate");
  CHECK(dst[1] == 0xDF48,                "surrogate pair: low surrogate");
}

static void test_mixed(void) {
  // "A é 𐍈" = U+0041 U+00E9 U+10348 -> 1+1+2 = 4 units 
  const char *src = "A\xC3\xA9\xF0\x90\x8D\x88";
  uint16_t dst[6];
  utf8_transcode_result_t r = utf8_transcode_utf16(src, 7, dst, 6);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "mixed: status OK");
  CHECK(r.consumed == 7,                 "mixed: consumed 7");
  CHECK(r.decoded  == 3,                 "mixed: decoded 3");
  CHECK(r.written  == 4,                 "mixed: written 4");
  CHECK(dst[0] == 'A',                   "mixed: dst[0] A");
  CHECK(dst[1] == 0x00E9,                "mixed: dst[1] U+00E9");
  CHECK(dst[2] == 0xD800,                "mixed: dst[2] high surrogate");
  CHECK(dst[3] == 0xDF48,                "mixed: dst[3] low surrogate");
}

static void test_exhausted(void) {
  // dst too small for BMP characters 
  uint16_t dst[2];
  utf8_transcode_result_t r = utf8_transcode_utf16("ABC", 3, dst, 2);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "exhausted: status");
  CHECK(r.consumed == 2,                        "exhausted: consumed 2");
  CHECK(r.decoded  == 2,                        "exhausted: decoded 2");
  CHECK(r.written  == 2,                        "exhausted: written 2");
  CHECK(dst[0] == 'A',                          "exhausted: dst[0]");
  CHECK(dst[1] == 'B',                          "exhausted: dst[1]");
}

static void test_exhausted_surrogate(void) {
  // dst has only one unit left but next codepoint needs a surrogate pair 
  const char *src = "A\xF0\x90\x8D\x88";
  uint16_t dst[2];
  utf8_transcode_result_t r = utf8_transcode_utf16(src, 5, dst, 2);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "exhausted surrogate: status");
  CHECK(r.consumed == 1,                        "exhausted surrogate: consumed 1");
  CHECK(r.decoded  == 1,                        "exhausted surrogate: decoded 1");
  CHECK(r.written  == 1,                        "exhausted surrogate: written 1");
  CHECK(dst[0] == 'A',                          "exhausted surrogate: dst[0]");
}

static void test_exhausted_resume(void) {
  const char *src = "ABCD";
  size_t len = 4;
  uint16_t dst[2];
  utf8_transcode_result_t r;

  r = utf8_transcode_utf16(src, len, dst, 2);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "resume: first status");
  CHECK(r.consumed == 2,                        "resume: first consumed");
  CHECK(r.decoded  == 2,                        "resume: first decoded");
  CHECK(r.written  == 2,                        "resume: first written");
  src += r.consumed;
  len -= r.consumed;

  r = utf8_transcode_utf16(src, len, dst, 2);
  CHECK(r.status   == UTF8_TRANSCODE_OK,        "resume: second status");
  CHECK(r.consumed == 2,                        "resume: second consumed");
  CHECK(r.decoded  == 2,                        "resume: second decoded");
  CHECK(r.written  == 2,                        "resume: second written");
  CHECK(dst[0] == 'C',                          "resume: dst[0]");
  CHECK(dst[1] == 'D',                          "resume: dst[1]");
}

static void test_illformed(void) {
  // "A\x80B": bare continuation in the middle 
  uint16_t dst[4];
  utf8_transcode_result_t r;

  r = utf8_transcode_utf16("A\x80" "B", 3, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_ILLFORMED, "illformed: status");
  CHECK(r.consumed == 1,                        "illformed: consumed 1");
  CHECK(r.decoded  == 1,                        "illformed: decoded 1");
  CHECK(r.written  == 1,                        "illformed: written 1");
  CHECK(r.advance  == 1,                        "illformed: advance 1");
  CHECK(dst[0] == 'A',                          "illformed: dst[0]");
}

static void test_illformed_resume(void) {
  const char *src = "A\x80" "B";
  size_t len = 3;
  uint16_t dst[4];
  utf8_transcode_result_t r;

  r = utf8_transcode_utf16(src, len, dst, 4);
  CHECK(r.status == UTF8_TRANSCODE_ILLFORMED, "illformed resume: first call");
  src += r.consumed + r.advance;
  len -= r.consumed + r.advance;

  r = utf8_transcode_utf16(src, len, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "illformed resume: second call");
  CHECK(r.decoded  == 1,                 "illformed resume: decoded 1");
  CHECK(r.written  == 1,                 "illformed resume: written 1");
  CHECK(dst[0]     == 'B',               "illformed resume: dst[0]");
}

static void test_illformed_lead_then_bad_cont(void) {
  uint16_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf16("\xE2\x41", 2, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_ILLFORMED, "illformed lead+bad: status");
  CHECK(r.consumed == 0,                        "illformed lead+bad: consumed 0");
  CHECK(r.decoded  == 0,                        "illformed lead+bad: decoded 0");
  CHECK(r.written  == 0,                        "illformed lead+bad: written 0");
  CHECK(r.advance  == 1,                        "illformed lead+bad: advance 1");

  r = utf8_transcode_utf16("\xF0\x9F\x41", 3, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_ILLFORMED, "illformed 2valid+bad: status");
  CHECK(r.consumed == 0,                        "illformed 2valid+bad: consumed 0");
  CHECK(r.decoded  == 0,                        "illformed 2valid+bad: decoded 0");
  CHECK(r.written  == 0,                        "illformed 2valid+bad: written 0");
  CHECK(r.advance  == 2,                        "illformed 2valid+bad: advance 2");
}

static void test_truncated(void) {
  uint16_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf16("\xE2\x82", 2, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_TRUNCATED, "truncated: status");
  CHECK(r.consumed == 0,                        "truncated: consumed 0");
  CHECK(r.decoded  == 0,                        "truncated: decoded 0");
  CHECK(r.written  == 0,                        "truncated: written 0");
  CHECK(r.advance  == 2,                        "truncated: advance 2");
}

static void test_truncated_after_valid(void) {
  uint16_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf16("A\xC3", 2, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_TRUNCATED, "truncated after valid: status");
  CHECK(r.consumed == 1,                        "truncated after valid: consumed 1");
  CHECK(r.decoded  == 1,                        "truncated after valid: decoded 1");
  CHECK(r.written  == 1,                        "truncated after valid: written 1");
  CHECK(r.advance  == 1,                        "truncated after valid: advance 1");
  CHECK(dst[0]     == 'A',                      "truncated after valid: dst[0]");
}

static void test_replace_clean(void) {
  uint16_t dst[4];
  utf8_transcode_result_t r;

  r = utf8_transcode_utf16_replace("ABC", 3, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace clean: status OK");
  CHECK(r.consumed == 3,                 "replace clean: consumed 3");
  CHECK(r.decoded  == 3,                 "replace clean: decoded 3");
  CHECK(r.written  == 3,                 "replace clean: written 3");
}

static void test_replace_bare_cont(void) {
  uint16_t dst[4];
  utf8_transcode_result_t r;

  r = utf8_transcode_utf16_replace("\x80", 1, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace bare cont: status OK");
  CHECK(r.consumed == 1,                 "replace bare cont: consumed 1");
  CHECK(r.decoded  == 1,                 "replace bare cont: decoded 1");
  CHECK(r.written  == 1,                 "replace bare cont: written 1");
  CHECK(dst[0]     == 0xFFFD,            "replace bare cont: U+FFFD");
}

static void test_replace_mixed(void) {
  uint16_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf16_replace("A\x80" "B", 3, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace mixed: status OK");
  CHECK(r.consumed == 3,                 "replace mixed: consumed 3");
  CHECK(r.decoded  == 3,                 "replace mixed: decoded 3");
  CHECK(r.written  == 3,                 "replace mixed: written 3");
  CHECK(dst[0]     == 'A',               "replace mixed: dst[0]");
  CHECK(dst[1]     == 0xFFFD,            "replace mixed: dst[1]");
  CHECK(dst[2]     == 'B',               "replace mixed: dst[2]");
}

static void test_replace_lead_bad_cont(void) {
  uint16_t dst[4];
  utf8_transcode_result_t r;

  r = utf8_transcode_utf16_replace("\xE2\x41", 2, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace lead+bad: status OK");
  CHECK(r.consumed == 2,                 "replace lead+bad: consumed 2");
  CHECK(r.decoded  == 2,                 "replace lead+bad: decoded 2");
  CHECK(r.written  == 2,                 "replace lead+bad: written 2");
  CHECK(dst[0]     == 0xFFFD,            "replace lead+bad: dst[0] U+FFFD");
  CHECK(dst[1]     == 'A',               "replace lead+bad: dst[1] A");

  r = utf8_transcode_utf16_replace("\xF0\x9F\x41", 3, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace 2valid+bad: status OK");
  CHECK(r.consumed == 3,                 "replace 2valid+bad: consumed 3");
  CHECK(r.decoded  == 2,                 "replace 2valid+bad: decoded 2");
  CHECK(r.written  == 2,                 "replace 2valid+bad: written 2");
  CHECK(dst[0]     == 0xFFFD,            "replace 2valid+bad: dst[0] U+FFFD");
  CHECK(dst[1]     == 'A',               "replace 2valid+bad: dst[1] A");
}

static void test_replace_truncated(void) {
  uint16_t dst[4];
  utf8_transcode_result_t r;

  r = utf8_transcode_utf16_replace("\xE2\x82", 2, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace truncated: status OK");
  CHECK(r.consumed == 2,                 "replace truncated: consumed 2");
  CHECK(r.decoded  == 1,                 "replace truncated: decoded 1");
  CHECK(r.written  == 1,                 "replace truncated: written 1");
  CHECK(dst[0]     == 0xFFFD,            "replace truncated: U+FFFD");
}

static void test_replace_exhausted(void) {
  uint16_t dst[4];
  utf8_transcode_result_t r;

  r = utf8_transcode_utf16_replace("ABCD", 4, dst, 2);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "replace exhausted: status");
  CHECK(r.consumed == 2,                        "replace exhausted: consumed 2");
  CHECK(r.decoded  == 2,                        "replace exhausted: decoded 2");
  CHECK(r.written  == 2,                        "replace exhausted: written 2");

  r = utf8_transcode_utf16_replace("ABCD", 4, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace exact: status OK");
  CHECK(r.decoded  == 4,                 "replace exact: decoded 4");
  CHECK(r.written  == 4,                 "replace exact: written 4");
}

static void test_replace_surrogate_pair(void) {
  // U+10348 -> surrogate pair D800 DF48 
  const char *src = "\xF0\x90\x8D\x88";
  uint16_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf16_replace(src, 4, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace surrogate pair: status OK");
  CHECK(r.consumed == 4,                 "replace surrogate pair: consumed 4");
  CHECK(r.decoded  == 1,                 "replace surrogate pair: decoded 1");
  CHECK(r.written  == 2,                 "replace surrogate pair: written 2");
  CHECK(dst[0]     == 0xD800,            "replace surrogate pair: high surrogate");
  CHECK(dst[1]     == 0xDF48,            "replace surrogate pair: low surrogate");
}

static void test_replace_surrogate_encoding(void) {
  // ED A0 80 (U+D800 encoded as UTF-8 surrogate):
  // maximal subparts are 1+1+1, producing three U+FFFD per Unicode
  uint16_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf16_replace("\xED\xA0\x80", 3, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace surrogate encoding: status OK");
  CHECK(r.consumed == 3,                 "replace surrogate encoding: consumed 3");
  CHECK(r.decoded  == 3,                 "replace surrogate encoding: decoded 3");
  CHECK(r.written  == 3,                 "replace surrogate encoding: written 3");
  CHECK(dst[0]     == 0xFFFD,            "replace surrogate encoding: dst[0] U+FFFD");
  CHECK(dst[1]     == 0xFFFD,            "replace surrogate encoding: dst[1] U+FFFD");
  CHECK(dst[2]     == 0xFFFD,            "replace surrogate encoding: dst[2] U+FFFD");
}

int main(void) {
  test_empty();
  test_ascii();
  test_bmp();
  test_surrogate_pair();
  test_mixed();
  test_exhausted();
  test_exhausted_surrogate();
  test_exhausted_resume();
  test_illformed();
  test_illformed_resume();
  test_illformed_lead_then_bad_cont();
  test_truncated();
  test_truncated_after_valid();
  test_replace_clean();
  test_replace_bare_cont();
  test_replace_mixed();
  test_replace_lead_bad_cont();
  test_replace_truncated();
  test_replace_exhausted();
  test_replace_surrogate_pair();
  test_replace_surrogate_encoding();

  if (TestFailed)
    printf("Failed %zu of %zu tests.\n", TestFailed, TestCount);
  else
    printf("All %zu tests passed.\n", TestCount);

  return TestFailed ? 1 : 0;
}
