#include <stddef.h>
#include <stdio.h>

#ifdef UTF8_DFA_64
#  include "utf8_dfa64.h"
#else
#  include "utf8_dfa32.h"
#endif

#include "utf8_valid_stream.h"
#include "test_common.h"

static void
stream_check(const char* desc,
             utf8_valid_stream_status_t exp_status,
             size_t exp_consumed,
             size_t exp_advance,
             utf8_valid_stream_result_t got,
             unsigned line) {
  TestCount++;
  if (got.status != exp_status) {
    printf("utf8_valid_stream_check [%s] status == %d, expected %d at line %u\n",
           desc, (int)got.status, (int)exp_status, line);
    TestFailed++;
  }

  TestCount++;
  if (got.consumed != exp_consumed) {
    printf("utf8_valid_stream_check [%s] consumed == %zu, expected %zu at line %u\n",
           desc, got.consumed, exp_consumed, line);
    TestFailed++;
  }

  TestCount++;
  if (got.advance != exp_advance) {
    printf("utf8_valid_stream_check [%s] advance == %zu, expected %zu at line %u\n",
           desc, got.advance, exp_advance, line);
    TestFailed++;
  }
}

#define STREAM(desc, st, src, len, eof, exp_status, exp_consumed, exp_advance) do { \
  utf8_valid_stream_result_t _r = utf8_valid_stream_check(st, src, len, eof); \
  stream_check(desc, exp_status, exp_consumed, exp_advance, _r, __LINE__); \
} while (0)

static void
test_streaming(void) {
  utf8_valid_stream_t st;

  utf8_valid_stream_init(&st);
  STREAM("clean ASCII chunk", &st, "hello", 5, false,
         UTF8_VALID_STREAM_OK, 5, 0);
  STREAM("clean ASCII chunk eof", &st, "world", 5, true,
         UTF8_VALID_STREAM_OK, 5, 0);

  utf8_valid_stream_init(&st);
  STREAM("clean 2-byte chunk", &st, "\xC3\xA9\xC3\xA9", 4, true,
         UTF8_VALID_STREAM_OK, 4, 0);

  utf8_valid_stream_init(&st);
  STREAM("2-byte split chunk 1", &st, "\xC3", 1, false,
         UTF8_VALID_STREAM_PARTIAL, 0, 0);
  STREAM("2-byte split chunk 2", &st, "\xA9", 1, false,
         UTF8_VALID_STREAM_OK, 1, 0);

  utf8_valid_stream_init(&st);
  STREAM("3-byte split 1/3", &st, "\xE2", 1, false,
         UTF8_VALID_STREAM_PARTIAL, 0, 0);
  STREAM("3-byte split 2/3", &st, "\x82", 1, false,
         UTF8_VALID_STREAM_PARTIAL, 0, 0);
  STREAM("3-byte split 3/3", &st, "\xAC", 1, true,
         UTF8_VALID_STREAM_OK, 1, 0);

  utf8_valid_stream_init(&st);
  STREAM("4-byte split 1/4", &st, "\xF0", 1, false,
         UTF8_VALID_STREAM_PARTIAL, 0, 0);
  STREAM("4-byte split 2/4", &st, "\x9F", 1, false,
         UTF8_VALID_STREAM_PARTIAL, 0, 0);
  STREAM("4-byte split 3/4", &st, "\x98", 1, false,
         UTF8_VALID_STREAM_PARTIAL, 0, 0);
  STREAM("4-byte split 4/4", &st, "\x80", 1, true,
         UTF8_VALID_STREAM_OK, 1, 0);

  utf8_valid_stream_init(&st);
  STREAM("multi-seq chunk", &st,
         "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80", 9, true,
         UTF8_VALID_STREAM_OK, 9, 0);

  utf8_valid_stream_init(&st);
  STREAM("16-byte ASCII block", &st, "abcdefghijklmnop", 16, true,
         UTF8_VALID_STREAM_OK, 16, 0);

  utf8_valid_stream_init(&st);
  STREAM("32-byte ASCII blocks", &st,
         "abcdefghijklmnopqrstuvwxyz012345", 32, true,
         UTF8_VALID_STREAM_OK, 32, 0);

  utf8_valid_stream_init(&st);
  STREAM("block then partial lead", &st, "abcdefghijklmnop\xC3", 17, false,
         UTF8_VALID_STREAM_PARTIAL, 16, 0);
  STREAM("complete lead after block", &st, "\xA9", 1, true,
         UTF8_VALID_STREAM_OK, 1, 0);

  utf8_valid_stream_init(&st);
  STREAM("reject at block end", &st, "abcdefghijklmno\x80", 16, false,
         UTF8_VALID_STREAM_ILLFORMED, 15, 1);

  utf8_valid_stream_init(&st);
  STREAM("reject after clean block", &st, "abcdefghijklmnop\x80", 17, false,
         UTF8_VALID_STREAM_ILLFORMED, 16, 1);

  utf8_valid_stream_init(&st);
  STREAM("invalid mid-chunk", &st, "ab\x80""cd", 5, false,
         UTF8_VALID_STREAM_ILLFORMED, 2, 1);

  utf8_valid_stream_init(&st);
  STREAM("invalid at start", &st, "\x80""hello", 6, false,
         UTF8_VALID_STREAM_ILLFORMED, 0, 1);

  utf8_valid_stream_init(&st);
  STREAM("invalid at end", &st, "hello\x80", 6, false,
         UTF8_VALID_STREAM_ILLFORMED, 5, 1);

  utf8_valid_stream_init(&st);
  STREAM("truncated 2-byte at eof", &st, "\xC3", 1, true,
         UTF8_VALID_STREAM_TRUNCATED, 0, 1);

  utf8_valid_stream_init(&st);
  STREAM("truncated 3-byte at eof", &st, "\xE2\x82", 2, true,
         UTF8_VALID_STREAM_TRUNCATED, 0, 2);

  utf8_valid_stream_init(&st);
  STREAM("truncated 4-byte at eof", &st, "\xF0\x9F\x98", 3, true,
         UTF8_VALID_STREAM_TRUNCATED, 0, 3);

  utf8_valid_stream_init(&st);
  STREAM("invalid before resume", &st, "ab\x80", 3, false,
         UTF8_VALID_STREAM_ILLFORMED, 2, 1);
  STREAM("resume after invalid", &st, "cd", 2, true,
         UTF8_VALID_STREAM_OK, 2, 0);

  utf8_valid_stream_init(&st);
  STREAM("truncated before resume", &st, "\xC3", 1, true,
         UTF8_VALID_STREAM_TRUNCATED, 0, 1);
  STREAM("resume after truncated eof", &st, "ok", 2, true,
         UTF8_VALID_STREAM_OK, 2, 0);

  utf8_valid_stream_init(&st);
  STREAM("empty chunk", &st, "", 0, false,
         UTF8_VALID_STREAM_OK, 0, 0);
  STREAM("empty chunk eof", &st, "", 0, true,
         UTF8_VALID_STREAM_OK, 0, 0);

  utf8_valid_stream_init(&st);
  STREAM("single-byte 1", &st, "h", 1, false,
         UTF8_VALID_STREAM_OK, 1, 0);
  STREAM("single-byte 2", &st, "i", 1, false,
         UTF8_VALID_STREAM_OK, 1, 0);
  STREAM("single-byte 3", &st, "!", 1, true,
         UTF8_VALID_STREAM_OK, 1, 0);

  utf8_valid_stream_init(&st);
  STREAM("split then more 1", &st, "abc\xC3", 4, false,
         UTF8_VALID_STREAM_PARTIAL, 3, 0);
  STREAM("split then more 2", &st, "\xA9""def", 4, true,
         UTF8_VALID_STREAM_OK, 4, 0);

  utf8_valid_stream_init(&st);
  STREAM("carried lead", &st, "\xC3", 1, false,
         UTF8_VALID_STREAM_PARTIAL, 0, 0);
  STREAM("carried lead then ASCII", &st, "A", 1, false,
         UTF8_VALID_STREAM_ILLFORMED, 0, 0);
  STREAM("retry ASCII after carried lead", &st, "A", 1, true,
         UTF8_VALID_STREAM_OK, 1, 0);

  utf8_valid_stream_init(&st);
  STREAM("carried lead + valid cont", &st, "\xE2", 1, false,
         UTF8_VALID_STREAM_PARTIAL, 0, 0);
  STREAM("carried lead + cont then ASCII", &st, "\x82""A", 2, false,
         UTF8_VALID_STREAM_ILLFORMED, 0, 1);
  STREAM("retry ASCII after carried lead + cont", &st, "A", 1, true,
         UTF8_VALID_STREAM_OK, 1, 0);

  utf8_valid_stream_init(&st);
  STREAM("truncated from previous chunk", &st, "\xC3", 1, false,
         UTF8_VALID_STREAM_PARTIAL, 0, 0);
  STREAM("eof after carried lead", &st, "", 0, true,
         UTF8_VALID_STREAM_TRUNCATED, 0, 0);
}

int
main(int argc, char **argv) {
  test_streaming();
  return report_results();
}
