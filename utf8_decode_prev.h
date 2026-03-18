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
#ifndef UTF8_DECODE_PREV_H
#define UTF8_DECODE_PREV_H
#include <stddef.h>
#include <stdint.h>

#ifndef UTF8_RDFA64_H
#  error "include utf8_rdfa64.h before utf8_decode_prev.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * utf8_decode_prev -- decode one codepoint ending at src[len-1].
 *
 * On success:   returns bytes consumed (1-4) and writes the codepoint
 *               to *codepoint. The caller should step back by the return
 *               value: next call uses src[0..len-return_value).
 * At end:       returns 0, *codepoint is unchanged.
 * On error:     returns the negated number of bytes to step back (always
 *               negative, in the range -1..-3). *codepoint is unchanged.
 *               The caller should step back by -return_value bytes before
 *               calling again.
 *
 * Computes the maximal subpart of each ill-formed subsequence, mirroring
 * utf8_decode_next(). Results may differ: the reverse DFA consumes
 * continuation bytes before seeing the lead, so all consumed bytes are
 * reported as one ill-formed unit when rejection occurs. For example,
 * "\x80\x80" produces two negated subpart lengths forward but one reverse.
 */
static inline int utf8_decode_prev(const char *src,
                                  size_t len,
                                  uint32_t *codepoint) {
 if (len == 0)
   return 0;

 const unsigned char *s = (const unsigned char *)src;
 utf8_rdfa_state_t state = UTF8_RDFA_ACCEPT;
 uint32_t cp = 0;
 size_t i = len;

 do {
   state = utf8_rdfa_step_decode(state, s[--i], &cp);
   if (state == UTF8_RDFA_ACCEPT) {
     *codepoint = cp;
     return (int)(len - i);
   }
   if (state == UTF8_RDFA_REJECT) {
     // Single byte (i == len-1): subpart = 1
     // Multiple bytes: subpart = bytes consumed before the rejecting byte
     return -(int)(i < len - 1 ? len - i - 1 : 1);
   }
 } while (i > 0);

 // Truncated sequence
 return -(int)(len - i);
}

/*
 * utf8_decode_prev_replace -- like utf8_decode_prev but on error writes
 * U+FFFD to *codepoint and returns the advance as a positive value.
 * Never returns a negative value. Returns 0 only when len is 0.
 *
 * Computes the maximal subpart of each ill-formed subsequence, mirroring
 * utf8_decode_next_replace(). Results may differ: the reverse DFA consumes
 * continuation bytes before seeing the lead, so all consumed bytes are
 * reported as one ill-formed unit when rejection occurs. For example,
 * "\x80\x80" produces two U+FFFD forward but one reverse.
 */
static inline int utf8_decode_prev_replace(const char *src,
                                          size_t len,
                                          uint32_t *codepoint) {
 if (len == 0)
   return 0;

 const unsigned char *s = (const unsigned char *)src;
 utf8_rdfa_state_t state = UTF8_RDFA_ACCEPT;
 uint32_t cp = 0;
 size_t i = len;

 do {
   state = utf8_rdfa_step_decode(state, s[--i], &cp);
   if (state == UTF8_RDFA_ACCEPT) {
     *codepoint = cp;
     return (int)(len - i);
   }
   if (state == UTF8_RDFA_REJECT) {
     *codepoint = 0xFFFDu;
     return (int)(i < len - 1 ? len - i - 1 : 1);
   }
 } while (i > 0);

 *codepoint = 0xFFFDu;
 return (int)(len - i);
}

#ifdef __cplusplus
}
#endif
#endif /* UTF8_DECODE_PREV_H */
