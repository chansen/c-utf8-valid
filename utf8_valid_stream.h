/*
 * Copyright (c) 2026 Christian Hansen <chansen@cpan.org>
 * <https://github.com/chansen/c-utf8>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef UTF8_VALID_STREAM_H
#define UTF8_VALID_STREAM_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#if defined(UTF8_DFA32_H) && defined(UTF8_DFA64_H)
#  error "utf8_dfa32.h and utf8_dfa64.h are mutually exclusive"
#elif !defined(UTF8_DFA32_H) && !defined(UTF8_DFA64_H)
#  error "include utf8_dfa32.h or utf8_dfa64.h before utf8_valid_stream.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Streaming API
 *
 * utf8_valid_stream_t holds the DFA state between calls. Initialize with
 * utf8_valid_stream_init() before the first call to utf8_valid_stream_check().
 *
 * utf8_valid_stream_check() validates the next chunk of a UTF-8 stream and
 * returns the number of bytes forming complete, valid sequences. The caller
 * should feed consecutive, non-overlapping byte ranges from the stream on
 * each call; any bytes beyond the returned count are already consumed by the
 * DFA and carried in the stream state.
 *
 * If eof is true and the stream does not end on a sequence boundary,
 * the input is treated as ill-formed.
 *
 * On error, (size_t)-1 is returned and *cursor, if non-NULL, is set
 * to the byte offset of the start of the invalid or truncated sequence
 * within src. The stream state is automatically reset to UTF8_DFA_ACCEPT so
 * the caller can resume from the next byte without reinitializing.
 */

typedef struct {
  utf8_dfa_state_t state;
} utf8_valid_stream_t;

static inline void
utf8_valid_stream_init(utf8_valid_stream_t *s) {
  s->state = UTF8_DFA_ACCEPT;
}

static inline size_t utf8_valid_stream_check(utf8_valid_stream_t* s,
                                             const char* src,
                                             size_t len,
                                             bool eof,
                                             size_t* cursor) {
  const unsigned char* p = (const unsigned char*)src;
  utf8_dfa_state_t state = s->state;
  size_t last_accept = 0;

  for (size_t i = 0; i < len; i++) {
    state = utf8_dfa_step(state, p[i]);
    if (state == UTF8_DFA_ACCEPT)
      last_accept = i + 1;
    else if (state == UTF8_DFA_REJECT) {
      s->state = UTF8_DFA_ACCEPT;
      if (cursor)
        *cursor = last_accept;
      return (size_t)-1;
    }
  }

  s->state = state;

  if (state != UTF8_DFA_ACCEPT) {
    if (eof) {
      s->state = UTF8_DFA_ACCEPT;
      if (cursor)
        *cursor = last_accept;
      return (size_t)-1;
    }
    return last_accept;
  }

  return len;
}

#ifdef __cplusplus
}
#endif
#endif
