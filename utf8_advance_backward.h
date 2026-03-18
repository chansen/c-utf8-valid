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
#ifndef UTF8_ADVANCE_BACKWARD_H
#define UTF8_ADVANCE_BACKWARD_H
#include <stddef.h>
#include <stdint.h>

#if defined(UTF8_RDFA32_H) && defined(UTF8_RDFA64_H)
#  error "utf8_rdfa32.h and utf8_rdfa64.h are mutually exclusive"
#elif !defined(UTF8_RDFA32_H) && !defined(UTF8_RDFA64_H)
#  error "include utf8_rdfa32.h or utf8_rdfa64.h before utf8_advance_backward.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * utf8_advance_backward -- advance backward by distance codepoints.
 *
 * Returns the byte offset of the start of the codepoint distance positions
 * before the end of src[0..len), or 0 if distance exceeds the number of
 * codepoints in the buffer. Returns (size_t)-1 if src[0..len) contains
 * ill-formed UTF-8. If advanced is non-NULL, writes the number of codepoints
 * actually advanced before stopping.
 */
static inline size_t utf8_advance_backward(const char *src,
                                           size_t len,
                                           size_t distance,
                                           size_t *advanced) {
  const unsigned char *s = (const unsigned char *)src;
  utf8_rdfa_state_t state = UTF8_RDFA_ACCEPT;
  size_t i = len;
  size_t count = 0;

  while (distance - count >= 4 && i >= 4) {
    utf8_rdfa_state_t s0 = utf8_rdfa_step(state, s[i - 1]);
    utf8_rdfa_state_t s1 = utf8_rdfa_step(s0,    s[i - 2]);
    utf8_rdfa_state_t s2 = utf8_rdfa_step(s1,    s[i - 3]);
    utf8_rdfa_state_t s3 = utf8_rdfa_step(s2,    s[i - 4]);
    count += (s0 == UTF8_RDFA_ACCEPT) + (s1 == UTF8_RDFA_ACCEPT)
           + (s2 == UTF8_RDFA_ACCEPT) + (s3 == UTF8_RDFA_ACCEPT);
    state = s3;
    i -= 4;
  }

  while (i > 0 && count < distance) {
    state = utf8_rdfa_step(state, s[--i]);
    if (state == UTF8_RDFA_ACCEPT)
      count++;
  }

  if (advanced)
    *advanced = count;
  return state == UTF8_RDFA_ACCEPT ? i : (size_t)-1;
}

#ifdef __cplusplus
}
#endif
#endif /* UTF8_ADVANCE_BACKWARD_H */
