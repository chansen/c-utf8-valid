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
