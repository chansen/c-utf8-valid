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

#include "utf8_valid_stream.h"
#include "test_common.h"

static void stream_check(const char* desc,
                         size_t exp_n,
                         size_t exp_cursor,
                         size_t got_n,
                         size_t got_cursor,
                         unsigned line) {
  TestCount++;
  if (got_n != exp_n) {
    printf("utf8_valid_stream_check [%s] returned %zu, expected %zu at line %u\n",
           desc, got_n, exp_n, line);
    TestFailed++;
  }
  if (got_n == (size_t)-1) {
    TestCount++;
    if (got_cursor != exp_cursor) {
      printf("utf8_valid_stream_check [%s] cursor == %zu, expected %zu at line %u\n",
             desc, got_cursor, exp_cursor, line);
      TestFailed++;
    }
  }
}

#define STREAM_OK(desc, st, src, len, eof, exp_n) do { \
  size_t _cursor = 0; \
  size_t _n = utf8_valid_stream_check(st, src, len, eof, &_cursor); \
  stream_check(desc, exp_n, 0, _n, _cursor, __LINE__); \
} while (0)

#define STREAM_ERR(desc, st, src, len, eof, exp_cursor) do { \
  size_t _cursor = 0; \
  size_t _n = utf8_valid_stream_check(st, src, len, eof, &_cursor); \
  stream_check(desc, (size_t)-1, exp_cursor, _n, _cursor, __LINE__); \
} while (0)

void
test_streaming() {
  utf8_valid_stream_t st;

  // Clean chunk -- no split, no error 
  utf8_valid_stream_init(&st);
  STREAM_OK("clean ASCII chunk", &st, "hello", 5, false, 5);
  STREAM_OK("clean ASCII chunk eof", &st, "world", 5, true, 5);

  // Clean multibyte chunk 
  utf8_valid_stream_init(&st);
  STREAM_OK("clean 2-byte chunk", &st, "\xC3\xA9\xC3\xA9", 4, true, 4);

  // 2-byte sequence split across two chunks 
  utf8_valid_stream_init(&st);
  STREAM_OK("2-byte split chunk 1", &st, "\xC3", 1, false, 0);
  STREAM_OK("2-byte split chunk 2", &st, "\xA9", 1, false, 1);

  // 3-byte sequence split after first byte 
  utf8_valid_stream_init(&st);
  STREAM_OK("3-byte split 1/3", &st, "\xE2", 1, false, 0);
  STREAM_OK("3-byte split 2/3", &st, "\x82", 1, false, 0);
  STREAM_OK("3-byte split 3/3", &st, "\xAC", 1, true, 1);

  // 4-byte sequence split at every boundary 
  utf8_valid_stream_init(&st);
  STREAM_OK("4-byte split 1/4", &st, "\xF0", 1, false, 0);
  STREAM_OK("4-byte split 2/4", &st, "\x9F", 1, false, 0);
  STREAM_OK("4-byte split 3/4", &st, "\x98", 1, false, 0);
  STREAM_OK("4-byte split 4/4", &st, "\x80", 1, true, 1);

  // Multiple sequences in one chunk 
  utf8_valid_stream_init(&st);
  STREAM_OK("multi-seq chunk", &st,
            "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80", 9, true, 9);

  // Invalid byte mid-chunk 
  utf8_valid_stream_init(&st);
  STREAM_ERR("invalid mid-chunk", &st, "ab\x80""cd", 5, false, 2);

  // Invalid byte at start of chunk 
  utf8_valid_stream_init(&st);
  STREAM_ERR("invalid at start", &st, "\x80""hello", 6, false, 0);

  // Invalid byte at end of chunk 
  utf8_valid_stream_init(&st);
  STREAM_ERR("invalid at end", &st, "hello\x80", 6, false, 5);

  // Truncated sequence at EOF 
  utf8_valid_stream_init(&st);
  STREAM_ERR("truncated 2-byte at eof", &st, "\xC3", 1, true, 0);

  utf8_valid_stream_init(&st);
  STREAM_ERR("truncated 3-byte at eof", &st, "\xE2\x82", 2, true, 0);

  utf8_valid_stream_init(&st);
  STREAM_ERR("truncated 4-byte at eof", &st, "\xF0\x9F\x98", 3, true, 0);

  // Resume after invalid byte 
  utf8_valid_stream_init(&st);
  STREAM_ERR("invalid before resume", &st, "ab\x80", 3, false, 2);
  STREAM_OK("resume after invalid", &st, "cd", 2, true, 2);

  // Resume after truncated EOF 
  utf8_valid_stream_init(&st);
  STREAM_ERR("truncated before resume", &st, "\xC3", 1, true, 0);
  STREAM_OK("resume after truncated eof", &st, "ok", 2, true, 2);

  // Empty chunk 
  utf8_valid_stream_init(&st);
  STREAM_OK("empty chunk", &st, "", 0, false, 0);
  STREAM_OK("empty chunk eof", &st, "", 0, true, 0);

  // Single-byte chunks 
  utf8_valid_stream_init(&st);
  STREAM_OK("single-byte 1", &st, "h", 1, false, 1);
  STREAM_OK("single-byte 2", &st, "i", 1, false, 1);
  STREAM_OK("single-byte 3", &st, "!", 1, true,  1);

  // Split followed by more valid input 
  utf8_valid_stream_init(&st);
  STREAM_OK("split then more 1", &st, "abc\xC3",   4, false, 3);
  STREAM_OK("split then more 2", &st, "\xA9""def", 4, true,  4);

  // cursor NULL on error -- should not crash 
  utf8_valid_stream_init(&st);
  {
    size_t n = utf8_valid_stream_check(&st, "\x80", 1, false, NULL);
    TestCount++;
    if (n != (size_t)-1) {
      printf("utf8_valid_stream_check [null cursor] returned %zu, expected -1\n", n);
      TestFailed++;
    }
  }

  // P1: cursor on error reports last_accept, not the error byte index 
  utf8_valid_stream_init(&st);
  STREAM_ERR("P1 lead then invalid cont", &st, "\xF3\xC5", 2, false, 0);

  utf8_valid_stream_init(&st);
  STREAM_ERR("P1 valid seq then lead+invalid", &st, "\xC3\xA9\xF3\xC5", 4, false, 2);
}

int
main(int argc, char **argv) {
  test_streaming();
  return report_results();
}

