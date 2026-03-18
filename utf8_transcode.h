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
#ifndef UTF8_TRANSCODE_H
#define UTF8_TRANSCODE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * utf8_transcode_status_t -- outcome of a transcoding operation.
 *
 *   UTF8_TRANSCODE_OK         src fully consumed, no errors.
 *   UTF8_TRANSCODE_EXHAUSTED  dst full before src was consumed.
 *   UTF8_TRANSCODE_ILLFORMED  stopped at an ill-formed sequence.
 *   UTF8_TRANSCODE_TRUNCATED  src ends in the middle of a sequence.
 */
typedef enum {
  UTF8_TRANSCODE_OK,
  UTF8_TRANSCODE_EXHAUSTED,
  UTF8_TRANSCODE_ILLFORMED,
  UTF8_TRANSCODE_TRUNCATED,
} utf8_transcode_status_t;

/*
 * utf8_transcode_result_t -- result of a transcoding operation.
 *
 *   status:    outcome of the operation (see utf8_transcode_status_t).
 *   consumed:  bytes read from src.
 *   decoded:   codepoints decoded from src.
 *   written:   code units written to dst.
 *   advance:   bytes to skip past the ill-formed sequence on ILLFORMED or
 *              TRUNCATED, else 0. Resume at src[consumed+advance].
 */
typedef struct {
  utf8_transcode_status_t status;
  size_t consumed;
  size_t decoded;
  size_t written;
  size_t advance;
} utf8_transcode_result_t;

/*
 * utf8_transcode_utf16 -- transcode UTF-8 to UTF-16.
 *
 * src[0..src_len) is the input buffer. dst[0..dst_len) receives UTF-16
 * code units. Codepoints above U+FFFF are encoded as surrogate pairs and
 * consume two units. Both src and dst are fully caller-owned.
 *
 * Returns a utf8_transcode_result_t describing the outcome:
 *
 *   status:
 *     UTF8_TRANSCODE_OK         src fully consumed, no errors.
 *     UTF8_TRANSCODE_EXHAUSTED  dst full before src was consumed.
 *     UTF8_TRANSCODE_ILLFORMED  stopped at an ill-formed sequence.
 *     UTF8_TRANSCODE_TRUNCATED  src ends in the middle of a sequence.
 *
 *   consumed:  bytes read from src.
 *   decoded:   codepoints decoded from src.
 *   written:   UTF-16 code units written to dst.
 *   advance:   bytes to skip past the ill-formed sequence (ILLFORMED or
 *              TRUNCATED, else 0). Resume at src[consumed+advance].
 *
 * utf8_transcode_utf16_replace() is identical but replaces each ill-formed
 * sequence with U+FFFD and continues. Status is always OK or EXHAUSTED;
 * advance is always 0.
 */
static inline utf8_transcode_result_t utf8_transcode_utf16(const char *src,
                                                           size_t src_len,
                                                           uint16_t *dst,
                                                           size_t dst_len) {
  const unsigned char *s = (const unsigned char *)src;
  size_t pos = 0;
  size_t decoded = 0;
  size_t written = 0;

  while (pos < src_len && written < dst_len) {
    utf8_dfa_state_t state = UTF8_DFA_ACCEPT;
    uint32_t cp = 0;
    size_t start = pos;

    do {
      state = utf8_dfa_step_decode(state, s[pos++], &cp);
      if (state == UTF8_DFA_ACCEPT) {
        if (cp <= 0xFFFFu) {
          dst[written++] = (uint16_t)cp;
        } else {
          // Surrogate pair: need two units
          if (written + 1 >= dst_len) {
            pos = start;
            goto exhausted;
          }
          cp -= 0x10000u;
          dst[written++] = (uint16_t)(0xD800u + (cp >> 10));
          dst[written++] = (uint16_t)(0xDC00u + (cp & 0x3FFu));
        }
        decoded++;
        goto next;
      }
      if (state == UTF8_DFA_REJECT) {
        size_t advance = pos > start + 1 ? pos - start - 1 : 1;
        return (utf8_transcode_result_t){.status   = UTF8_TRANSCODE_ILLFORMED,
                                        .consumed  = start,
                                        .decoded   = decoded,
                                        .written   = written,
                                        .advance   = advance};
      }
    } while (pos < src_len);

    // Truncated: src ended in the middle of a sequence 
    return (utf8_transcode_result_t){.status   = UTF8_TRANSCODE_TRUNCATED,
                                     .consumed = start,
                                     .decoded  = decoded,
                                     .written  = written,
                                     .advance  = pos - start};
  next:;
  }

exhausted:;
  utf8_transcode_status_t status = pos < src_len ? UTF8_TRANSCODE_EXHAUSTED : UTF8_TRANSCODE_OK;
  return (utf8_transcode_result_t){.status   = status,
                                   .consumed = pos,
                                   .decoded  = decoded,
                                   .written  = written,
                                   .advance  = 0};
}

static inline utf8_transcode_result_t utf8_transcode_utf16_replace(const char *src,
                                                                   size_t src_len,
                                                                   uint16_t *dst,
                                                                   size_t dst_len) {
  const unsigned char *s = (const unsigned char *)src;
  size_t pos = 0;
  size_t decoded = 0;
  size_t written = 0;

  while (pos < src_len && written < dst_len) {
    utf8_dfa_state_t state = UTF8_DFA_ACCEPT;
    uint32_t cp = 0;
    size_t start = pos;

    do {
      state = utf8_dfa_step_decode(state, s[pos++], &cp);
      if (state == UTF8_DFA_ACCEPT) {
        if (cp <= 0xFFFFu) {
          dst[written++] = (uint16_t)cp;
        } else {
          // Surrogate pair: need two units
          if (written + 1 >= dst_len) {
            pos = start;
            goto exhausted;
          }
          cp -= 0x10000u;
          dst[written++] = (uint16_t)(0xD800u + (cp >> 10));
          dst[written++] = (uint16_t)(0xDC00u + (cp & 0x3FFu));
        }
        decoded++;
        goto next;
      }
      if (state == UTF8_DFA_REJECT) {
        // Back up: the rejecting byte belongs to the next sequence 
        if (pos > start + 1)
          pos--;
        dst[written++] = 0xFFFDu;
        decoded++;
        goto next;
      }
    } while (pos < src_len);

    // Truncated sequence at end of src: emit one U+FFFD
    dst[written++] = 0xFFFDu;
    decoded++;
    break;
  next:;
  }

exhausted:;
  utf8_transcode_status_t status = pos < src_len ? UTF8_TRANSCODE_EXHAUSTED : UTF8_TRANSCODE_OK;
  return (utf8_transcode_result_t){.status   = status,
                                   .consumed = pos,
                                   .decoded  = decoded,
                                   .written  = written,
                                   .advance  = 0};
}

/*
 * utf8_transcode_utf32 -- transcode UTF-8 to UTF-32.
 *
 * src[0..src_len) is the input buffer. dst[0..dst_len) receives UTF-32
 * code units. Both src and dst are fully caller-owned.
 *
 * Returns a utf8_transcode_result_t describing the outcome:
 *
 *   status:
 *     UTF8_TRANSCODE_OK         src fully consumed, no errors.
 *     UTF8_TRANSCODE_EXHAUSTED  dst full before src was consumed.
 *     UTF8_TRANSCODE_ILLFORMED  stopped at an ill-formed sequence.
 *     UTF8_TRANSCODE_TRUNCATED  src ends in the middle of a sequence.
 *
 *   consumed:  bytes read from src.
 *   decoded:   codepoints decoded from src.
 *   written:   UTF-32 code units written to dst (equals decoded for UTF-32).
 *   advance:   bytes to skip past the ill-formed sequence (ILLFORMED or
 *              TRUNCATED, else 0). Resume at src[consumed+advance].
 *
 * utf8_transcode_utf32_replace() is identical but replaces each ill-formed
 * sequence with U+FFFD and continues. Status is always OK or EXHAUSTED;
 * advance is always 0.
 */
static inline utf8_transcode_result_t utf8_transcode_utf32(const char* src,
                                                           size_t src_len,
                                                           uint32_t* dst,
                                                           size_t dst_len) {
  const unsigned char* s = (const unsigned char*)src;
  size_t pos = 0;
  size_t decoded = 0;

  while (pos < src_len && decoded < dst_len) {
    utf8_dfa_state_t state = UTF8_DFA_ACCEPT;
    uint32_t cp = 0;
    size_t start = pos;

    do {
      state = utf8_dfa_step_decode(state, s[pos++], &cp);
      if (state == UTF8_DFA_ACCEPT) {
        dst[decoded++] = cp;
        goto next;
      }
      if (state == UTF8_DFA_REJECT) {
        size_t advance = pos > start + 1 ? pos - start - 1 : 1;
        return (utf8_transcode_result_t){.status   = UTF8_TRANSCODE_ILLFORMED,
                                         .consumed = start,
                                         .decoded  = decoded,
                                         .written  = decoded,
                                         .advance  = advance};
      }
    } while (pos < src_len);

    // Truncated: src ended in the middle of a sequence
    return (utf8_transcode_result_t){.status   = UTF8_TRANSCODE_TRUNCATED,
                                     .consumed = start,
                                     .decoded  = decoded,
                                     .written  = decoded,
                                     .advance  = pos - start};
  next:;
  }

  utf8_transcode_status_t status = pos < src_len ? UTF8_TRANSCODE_EXHAUSTED : UTF8_TRANSCODE_OK;
  return (utf8_transcode_result_t){.status   = status,
                                   .consumed = pos,
                                   .decoded  = decoded,
                                   .written  = decoded,
                                   .advance  = 0};
}

static inline utf8_transcode_result_t utf8_transcode_utf32_replace(const char* src,
                                                                   size_t src_len,
                                                                   uint32_t* dst,
                                                                   size_t dst_len) {
  const unsigned char* s = (const unsigned char*)src;
  size_t pos = 0;
  size_t decoded = 0;

  while (pos < src_len && decoded < dst_len) {
    utf8_dfa_state_t state = UTF8_DFA_ACCEPT;
    uint32_t cp = 0;
    size_t start = pos;

    do {
      state = utf8_dfa_step_decode(state, s[pos++], &cp);
      if (state == UTF8_DFA_ACCEPT) {
        dst[decoded++] = cp;
        goto next;
      }
      if (state == UTF8_DFA_REJECT) {
        // Back up: the rejecting byte belongs to the next sequence
        if (pos > start + 1)
          pos--;
        dst[decoded++] = 0xFFFDu;
        goto next;
      }
    } while (pos < src_len);

    // Truncated sequence at end of src: emit one U+FFFD
    dst[decoded++] = 0xFFFDu;
    break;
  next:;
  }

  utf8_transcode_status_t status = pos < src_len ? UTF8_TRANSCODE_EXHAUSTED : UTF8_TRANSCODE_OK;
  return (utf8_transcode_result_t){.status   = status,
                                   .consumed = pos,
                                   .decoded  = decoded,
                                   .written  = decoded,
                                   .advance  = 0};
}

#ifdef __cplusplus
}
#endif
#endif /* UTF8_TRANSCODE_H */
