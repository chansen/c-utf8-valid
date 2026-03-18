/*
 * Copyright (c) 2017-2026 Christian Hansen <chansen@cpan.org>
 * <https://github.com/chansen/c-utf8-valid>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met: 
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
