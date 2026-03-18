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
