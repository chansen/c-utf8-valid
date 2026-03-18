/*
 * Copyright (c) 2017-2026 Christian Hansen <chansen@cpan.org>
 * <https://github.com/chansen/c-utf8-valid>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef UTF8_DECODE_NEXT_H
#define UTF8_DECODE_NEXT_H
#include <stddef.h>
#include <stdint.h>

#ifndef UTF8_DFA64_H
#  error "include utf8_dfa64.h before utf8_decode_next.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * utf8_decode_next -- decode one codepoint from src[0..len).
 *
 * On success:   returns bytes consumed (1-4) and writes the codepoint 
 *               to *codepoint.
 * At end:       returns 0, *codepoint is unchanged.
 * On error:     returns the negated length of the maximal subpart (always
 *               negative, in the range -1..-3). *codepoint is unchanged. 
 *               The caller should advance by -return_value bytes before 
 *               calling again.
 *
 * The maximal subpart of an ill-formed subsequence is defined by Unicode:
 * the longest prefix starting at the ill-formed offset that is either the
 * initial subsequence of a well-formed sequence, or a single code unit.
 * Each maximal subpart produces one U+FFFD substitution character.
 */
static inline int utf8_decode_next(const char* src,
                                   size_t len,
                                   uint32_t* codepoint) {
  if (len == 0)
    return 0;

  const unsigned char* s = (const unsigned char*)src;
  utf8_dfa_state_t state = UTF8_DFA_ACCEPT;
  uint32_t cp = 0;
  size_t i = 0;

  do {
    state = utf8_dfa_step_decode(state, s[i++], &cp);
    if (state == UTF8_DFA_ACCEPT) {
      *codepoint = cp;
      return (int)i;
    }
    if (state == UTF8_DFA_REJECT) {
     /* The byte at s[i-1] triggered rejection. If it was the first
      * byte, it is itself the maximal subpart (length 1). Otherwise
      * the lead byte(s) already consumed form the maximal subpart
      * and the triggering byte belongs to the next sequence. */
     return -(int)(i > 1 ? i - 1 : 1);
    }
  } while (i < len);

  // Truncated sequence: maximal subpart is the bytes consumed so far
  return -(int)i;
}

/*
 * utf8_decode_next_replace -- like utf8_decode_next but on error writes
 * U+FFFD to *codepoint and returns the maximal subpart length as a
 * positive value. Never returns a negative value. Returns 0 only when
 * len is 0.
 */
static inline int utf8_decode_next_replace(const char* src,
                                           size_t len,
                                           uint32_t* codepoint) {
  if (len == 0)
    return 0;

  const unsigned char* s = (const unsigned char*)src;
  utf8_dfa_state_t state = UTF8_DFA_ACCEPT;
  uint32_t cp = 0;
  size_t i = 0;

  do {
    state = utf8_dfa_step_decode(state, s[i++], &cp);
    if (state == UTF8_DFA_ACCEPT) {
      *codepoint = cp;
      return (int)i;
    }
    if (state == UTF8_DFA_REJECT) {
      *codepoint = 0xFFFDu;
      return (int)(i > 1 ? i - 1 : 1);
    }
  } while (i < len);

  *codepoint = 0xFFFDu;
  return (int)i;
}

#ifdef __cplusplus
}
#endif
#endif /* UTF8_DECODE_NEXT_H */
