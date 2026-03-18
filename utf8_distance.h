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
#ifndef UTF8_DISTANCE_H
#define UTF8_DISTANCE_H
#include <stddef.h>
#include <stdint.h>

#if defined(UTF8_DFA32_H) && defined(UTF8_DFA64_H)
#  error "utf8_dfa32.h and utf8_dfa64.h are mutually exclusive"
#elif !defined(UTF8_DFA32_H) && !defined(UTF8_DFA64_H)
#  error "include utf8_dfa32.h or utf8_dfa64.h before utf8_distance.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * utf8_distance -- count the number of codepoints in src[0..len).
 *
 * Returns the number of codepoints in src[0..len), or (size_t)-1 if
 * src[0..len) contains ill-formed UTF-8.
 */
static inline size_t utf8_distance(const char *src, size_t len) {
  const unsigned char *s = (const unsigned char *)src;
  utf8_dfa_state_t state = UTF8_DFA_ACCEPT;
  size_t count = 0;

  for (;len >= 4; len -= 4, s += 4) {
    utf8_dfa_state_t s0 = utf8_dfa_step(state, s[0]);
    utf8_dfa_state_t s1 = utf8_dfa_step(s0,    s[1]);
    utf8_dfa_state_t s2 = utf8_dfa_step(s1,    s[2]);
    utf8_dfa_state_t s3 = utf8_dfa_step(s2,    s[3]);
    count += (s0 == UTF8_DFA_ACCEPT) + (s1 == UTF8_DFA_ACCEPT)
           + (s2 == UTF8_DFA_ACCEPT) + (s3 == UTF8_DFA_ACCEPT);
    state = s3;
  }

  for (size_t i = 0; i < len; i++) {
    state = utf8_dfa_step(state, s[i]);
    if (state == UTF8_DFA_ACCEPT)
      count++;
  }

  return state == UTF8_DFA_ACCEPT ? count : (size_t)-1;
}

#ifdef __cplusplus
}
#endif
#endif /* UTF8_DISTANCE_H */
