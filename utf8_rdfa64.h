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

/*
 * utf8_rdfa64.h -- Shift-based DFA for Reversed UTF-8 validation and decoding
 * =============================================================================
 *
 *  Validation and decoding counterpart to utf8_rdfa32.h. If you only need
 *  validation, utf8_rdfa32.h uses uint32_t rows and has a smaller table (1 KB
 *  vs 2 KB).
 *
 * CONCEPT
 * -------
 *
 *  Scans UTF-8 bytes right-to-left. Feed bytes one at a time starting from
 *  S_ACCEPT, innermost continuation byte first, lead byte last. Each return
 *  to S_ACCEPT marks a complete valid sequence boundary. S_ERROR is an
 *  absorbing trap: once entered, no byte can leave it.
 *
 *  Because the lead byte arrives last, the DFA must carry forward the range
 *  of the outermost continuation byte (adjacent to the lead) until the lead
 *  is seen and can validate it. Two things determine whether a lead byte is
 *  accepted:
 *
 *    (1) How many continuation bytes have been accumulated (depth 1, 2, or 3)
 *    (2) The range of the outermost continuation byte, which narrows which
 *        lead bytes are valid to reject non-shortest forms, surrogates, and
 *        codepoints above U+10FFFF
 *
 *
 * STATE DEFINITIONS
 * -----------------
 *
 *   State    Value  Meaning
 *   -------  -----  -----------------------------------------------------
 *   S_ERROR    0    Invalid byte seen (absorbing trap state)
 *   S_ACCEPT  18    Start state / valid sequence boundary
 *   S_TL1      6    Seen 1 continuation
 *   S_TL2LO   24    Seen 2 continuations; outermost in 80-9F (LOW)
 *   S_TL2HI   42    Seen 2 continuations; outermost in A0-BF (HIGH)
 *   S_TL3LO   12    Seen 3 continuations; outermost in 80-8F (LOW)
 *   S_TL3MH   30    Seen 3 continuations; outermost in 90-BF (MID-HIGH)
 *
 *  State values are shift amounts, multiples of 6 except S_ERROR = 0.
 *  Each row packs 7 × 6-bit transition targets into 48 bits of a uint64_t.
 *
 *  State values encode depth in bits 3:2, so the payload shift is:
 *
 *    shift = ((state >> 2) & 3) * 6
 *
 *  where depth 0 = S_ACCEPT/S_ERROR, depth 1 = TL1, depth 2 = TL2xx,
 *  depth 3 = TL3xx.
 *
 *
 * TRANSITION TABLE
 * ----------------
 *                                Current State
 *
 *  Input Byte   ACCEPT   TL1   TL2LO  TL2HI  TL3LO  TL3MH 
 *  ---------    ------  ------ ------ ------ ------ ------
 *   00..7F      ACCEPT    -      -      -      -      -   
 *   80..8F      TL1     TL2LO  TL3LO  TL3LO    -      -   
 *   90..9F      TL1     TL2LO  TL3MH  TL3MH    -      -   
 *   A0..BF      TL1     TL2HI  TL3MH  TL3MH    -      -   
 *   C0..C1        -       -      -      -      -      -
 *   C2..DF        -     ACCEPT   -      -      -      -   
 *   E0            -       -      -    ACCEPT   -      -   
 *   E1..EC        -       -    ACCEPT ACCEPT   -      -   
 *   ED            -       -    ACCEPT   -      -      -   
 *   EE..EF        -       -    ACCEPT ACCEPT   -      -   
 *   F0            -       -      -      -      -    ACCEPT
 *   F1..F3        -       -      -      -    ACCEPT ACCEPT 
 *   F4            -       -      -      -    ACCEPT   -   
 *   F5..FF        -       -      -      -      -      -   
 *                              
 *  Note: "-" means transition to S_ERROR (invalid in that context)
 *
 *
 * STATE FLOW DIAGRAMS
 * -------------------
 *
 *  1-byte (ASCII):
 *    ACCEPT ─[0x00–0x7F]─→ ACCEPT
 *
 *  2-byte (U+0080–U+07FF):
 *    ACCEPT ─[0x80–0xBF]─→ TL1 ─[0xC2–0xDF]─→ ACCEPT
 *
 *  3-byte (U+0800–U+FFFF, excluding surrogates U+D800–U+DFFF):
 *    ACCEPT ─[cont1]─→ TL1 ─[cont2]─→ TL2xx ─[lead]─→ ACCEPT
 *
 *    Outermost continuation determines TL2 state, lead must match:
 *
 *    [cont2] ────────→ [state] ──> [lead] ─────→ ACCEPT
 *      │                  │          │
 *      ├── 0x80–0x9F ─→ TL2LO ───────┴─ 0xED, 0xE1–0xEC, 0xEE–0xEF
 *      └── 0xA0–0xBF ─→ TL2HI ───────┴─ 0xE0, 0xE1–0xEC, 0xEE–0xEF
 *
 *  4-byte (U+10000–U+10FFFF):
 *    ACCEPT ─[cont1]─→ TL1 ─[cont2]─→ TL2xx ─[cont3]─→ TL3xx ─[lead]─→ ACCEPT
 *
 *    Outermost continuation determines TL3 state, lead must match:
 *
 *    [cont3] ────────→ [state] ──> [lead] ─────→ ACCEPT
 *      │                  │          │
 *      ├── 0x80–0x8F ─→ TL3LO ───────┴─ 0xF4, 0xF1–0xF3
 *      └── 0x90–0xBF ─→ TL3MH ───────┴─ 0xF0, 0xF1–0xF3
 *
 *
 * UTF-8 ENCODING FORM
 * -------------------
 *
 *    U+0000..U+007F       0xxxxxxx
 *    U+0080..U+07FF       110xxxxx 10xxxxxx
 *    U+0800..U+FFFF       1110xxxx 10xxxxxx 10xxxxxx
 *   U+10000..U+10FFFF     11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 *
 *
 *    U+0000..U+007F       00..7F
 *                      N  C0..C1  80..BF                   1100000x 10xxxxxx
 *    U+0080..U+07FF       C2..DF  80..BF
 *                      N  E0      80..9F  80..BF           11100000 100xxxxx
 *    U+0800..U+0FFF       E0      A0..BF  80..BF
 *    U+1000..U+CFFF       E1..EC  80..BF  80..BF
 *    U+D000..U+D7FF       ED      80..9F  80..BF
 *                      S  ED      A0..BF  80..BF           11101101 101xxxxx
 *    U+E000..U+FFFF       EE..EF  80..BF  80..BF
 *                      N  F0      80..8F  80..BF  80..BF   11110000 1000xxxx
 *   U+10000..U+3FFFF      F0      90..BF  80..BF  80..BF
 *   U+40000..U+FFFFF      F1..F3  80..BF  80..BF  80..BF
 *  U+100000..U+10FFFF     F4      80..8F  80..BF  80..BF   11110100 1000xxxx
 *
 *  Legend:
 *    N = Non-shortest form
 *    S = Surrogates
 *
 *
 * ROW BIT LAYOUT
 * --------------
 *
 *   bits  0..47  DFA transition states (7 states x 6 bits each, gap at 36)
 *   bits 48..55  unused
 *   bits 56..62  payload mask (7 bits):
 *                  0x7F  ASCII        (0xxxxxxx -> 7 bits)
 *                  0x3F  continuation (10xxxxxx -> 6 bits)
 *                  0x1F  2-byte lead  (110xxxxx -> 5 bits)
 *                  0x0F  3-byte lead  (1110xxxx -> 4 bits)
 *                  0x07  4-byte lead  (11110xxx -> 3 bits)
 *                  0x00  invalid
 *   bit  63      unused
 *
 *
 * PAYLOAD BIT ACCUMULATION
 * ------------------------
 *
 *  When decoding, payload bits are accumulated right-to-left:
 *
 *    cp = cp | (byte & mask) << shift    where shift = ((state >> 2) & 3) * 6
 *
 *  On S_ACCEPT state, cp holds the complete decoded codepoint.
 *  On S_ERROR state, substitute U+FFFD or signal failure.
 *
 *  The caller must set cp = 0 at the start of each new sequence, i.e.
 *  whenever the previous step returned S_ACCEPT or S_ERROR.
 *
 *
 * PERFORMANCE
 * -----------
 *
 *  - 7 states total (minimal for well-formed reverse UTF-8 validation)
 *  - Table-driven: 256-entry uint64_t table (2 KB, fits in L1 cache)
 *  - Branchless step: (table[byte] >> state) & 63
 *  - Validates and decodes in a single branchless pass
 *
 *
 * REFERENCES
 * ----------
 *
 *  - Unicode Standard §3.9: Unicode Encoding Forms
 *     <https://www.unicode.org/versions/latest/core-spec/chapter-3/#G31703>
 *
 *
 * USAGE PATTERN
 * -------------
 *
 *   utf8_rdfa_state_t state = UTF8_RDFA_ACCEPT;
 *   uint32_t codepoint = 0;
 *   while (len > 0) {
 *     state = utf8_rdfa_step_decode(state, buffer[--len], &codepoint);
 *     if (state == UTF8_RDFA_REJECT) {
 *       // Invalid UTF-8 at position len
 *       break;
 *     }
 *     if (state == UTF8_RDFA_ACCEPT) {
 *       // Complete codepoint decoded
 *       process(codepoint);
 *       codepoint = 0;
 *     }
 *   }
 *
 */
#ifndef UTF8_RDFA64_H
#define UTF8_RDFA64_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t utf8_rdfa_state_t;

#define UTF8_RDFA_REJECT ((utf8_rdfa_state_t)0)
#define UTF8_RDFA_ACCEPT ((utf8_rdfa_state_t)18)

#define S_ERROR    0
#define S_ACCEPT  18
#define S_TL1      6
#define S_TL2LO   24
#define S_TL2HI   42
#define S_TL3LO   12
#define S_TL3MH   30

/* clang-format off */

#define DFA_ROW(accept,error,tl1,tl2lo,tl2hi,tl3lo,tl3mh,mask) \
  ( ((utf8_rdfa_state_t)(accept)   << S_ACCEPT) \
  | ((utf8_rdfa_state_t)(error)    << S_ERROR)  \
  | ((utf8_rdfa_state_t)(tl1)      << S_TL1)    \
  | ((utf8_rdfa_state_t)(tl2lo)    << S_TL2LO)  \
  | ((utf8_rdfa_state_t)(tl2hi)    << S_TL2HI)  \
  | ((utf8_rdfa_state_t)(tl3lo)    << S_TL3LO)  \
  | ((utf8_rdfa_state_t)(tl3mh)    << S_TL3MH)  \
  | ((utf8_rdfa_state_t)((mask) & 0x7F) << 56)  \
  )

#define ERR S_ERROR

//                         ACCEPT   ERR  TL1      TL2LO    TL2HI    TL3LO    TL3MH    mask
#define ASCII_ROW  DFA_ROW(S_ACCEPT,ERR, ERR,     ERR,     ERR,     ERR,     ERR,     0x7F)
#define CONT_80_8F DFA_ROW(S_TL1,   ERR, S_TL2LO, S_TL3LO, S_TL3LO, ERR,     ERR,     0x3F)
#define CONT_90_9F DFA_ROW(S_TL1,   ERR, S_TL2LO, S_TL3MH, S_TL3MH, ERR,     ERR,     0x3F)
#define CONT_A0_BF DFA_ROW(S_TL1,   ERR, S_TL2HI, S_TL3MH, S_TL3MH, ERR,     ERR,     0x3F)
#define LEAD2_ROW  DFA_ROW(ERR,     ERR, S_ACCEPT,ERR,     ERR,     ERR,     ERR,     0x1F)
#define E0_ROW     DFA_ROW(ERR,     ERR, ERR,     ERR,     S_ACCEPT,ERR,     ERR,     0x0F)
#define LEAD3_ROW  DFA_ROW(ERR,     ERR, ERR,     S_ACCEPT,S_ACCEPT,ERR,     ERR,     0x0F)
#define ED_ROW     DFA_ROW(ERR,     ERR, ERR,     S_ACCEPT,ERR,     ERR,     ERR,     0x0F)
#define F0_ROW     DFA_ROW(ERR,     ERR, ERR,     ERR,     ERR,     ERR,     S_ACCEPT,0x07)
#define LEAD4_ROW  DFA_ROW(ERR,     ERR, ERR,     ERR,     ERR,     S_ACCEPT,S_ACCEPT,0x07)
#define F4_ROW     DFA_ROW(ERR,     ERR, ERR,     ERR,     ERR,     S_ACCEPT,ERR,     0x07)
#define ERROR_ROW  DFA_ROW(ERR,     ERR, ERR,     ERR,     ERR,     ERR,     ERR,     0x00)

static const utf8_rdfa_state_t utf8_rdfa[256] = {
  // 00-7F
  [0x00]=ASCII_ROW,[0x01]=ASCII_ROW,[0x02]=ASCII_ROW,[0x03]=ASCII_ROW,
  [0x04]=ASCII_ROW,[0x05]=ASCII_ROW,[0x06]=ASCII_ROW,[0x07]=ASCII_ROW,
  [0x08]=ASCII_ROW,[0x09]=ASCII_ROW,[0x0A]=ASCII_ROW,[0x0B]=ASCII_ROW,
  [0x0C]=ASCII_ROW,[0x0D]=ASCII_ROW,[0x0E]=ASCII_ROW,[0x0F]=ASCII_ROW,
  [0x10]=ASCII_ROW,[0x11]=ASCII_ROW,[0x12]=ASCII_ROW,[0x13]=ASCII_ROW,
  [0x14]=ASCII_ROW,[0x15]=ASCII_ROW,[0x16]=ASCII_ROW,[0x17]=ASCII_ROW,
  [0x18]=ASCII_ROW,[0x19]=ASCII_ROW,[0x1A]=ASCII_ROW,[0x1B]=ASCII_ROW,
  [0x1C]=ASCII_ROW,[0x1D]=ASCII_ROW,[0x1E]=ASCII_ROW,[0x1F]=ASCII_ROW,
  [0x20]=ASCII_ROW,[0x21]=ASCII_ROW,[0x22]=ASCII_ROW,[0x23]=ASCII_ROW,
  [0x24]=ASCII_ROW,[0x25]=ASCII_ROW,[0x26]=ASCII_ROW,[0x27]=ASCII_ROW,
  [0x28]=ASCII_ROW,[0x29]=ASCII_ROW,[0x2A]=ASCII_ROW,[0x2B]=ASCII_ROW,
  [0x2C]=ASCII_ROW,[0x2D]=ASCII_ROW,[0x2E]=ASCII_ROW,[0x2F]=ASCII_ROW,
  [0x30]=ASCII_ROW,[0x31]=ASCII_ROW,[0x32]=ASCII_ROW,[0x33]=ASCII_ROW,
  [0x34]=ASCII_ROW,[0x35]=ASCII_ROW,[0x36]=ASCII_ROW,[0x37]=ASCII_ROW,
  [0x38]=ASCII_ROW,[0x39]=ASCII_ROW,[0x3A]=ASCII_ROW,[0x3B]=ASCII_ROW,
  [0x3C]=ASCII_ROW,[0x3D]=ASCII_ROW,[0x3E]=ASCII_ROW,[0x3F]=ASCII_ROW,
  [0x40]=ASCII_ROW,[0x41]=ASCII_ROW,[0x42]=ASCII_ROW,[0x43]=ASCII_ROW,
  [0x44]=ASCII_ROW,[0x45]=ASCII_ROW,[0x46]=ASCII_ROW,[0x47]=ASCII_ROW,
  [0x48]=ASCII_ROW,[0x49]=ASCII_ROW,[0x4A]=ASCII_ROW,[0x4B]=ASCII_ROW,
  [0x4C]=ASCII_ROW,[0x4D]=ASCII_ROW,[0x4E]=ASCII_ROW,[0x4F]=ASCII_ROW,
  [0x50]=ASCII_ROW,[0x51]=ASCII_ROW,[0x52]=ASCII_ROW,[0x53]=ASCII_ROW,
  [0x54]=ASCII_ROW,[0x55]=ASCII_ROW,[0x56]=ASCII_ROW,[0x57]=ASCII_ROW,
  [0x58]=ASCII_ROW,[0x59]=ASCII_ROW,[0x5A]=ASCII_ROW,[0x5B]=ASCII_ROW,
  [0x5C]=ASCII_ROW,[0x5D]=ASCII_ROW,[0x5E]=ASCII_ROW,[0x5F]=ASCII_ROW,
  [0x60]=ASCII_ROW,[0x61]=ASCII_ROW,[0x62]=ASCII_ROW,[0x63]=ASCII_ROW,
  [0x64]=ASCII_ROW,[0x65]=ASCII_ROW,[0x66]=ASCII_ROW,[0x67]=ASCII_ROW,
  [0x68]=ASCII_ROW,[0x69]=ASCII_ROW,[0x6A]=ASCII_ROW,[0x6B]=ASCII_ROW,
  [0x6C]=ASCII_ROW,[0x6D]=ASCII_ROW,[0x6E]=ASCII_ROW,[0x6F]=ASCII_ROW,
  [0x70]=ASCII_ROW,[0x71]=ASCII_ROW,[0x72]=ASCII_ROW,[0x73]=ASCII_ROW,
  [0x74]=ASCII_ROW,[0x75]=ASCII_ROW,[0x76]=ASCII_ROW,[0x77]=ASCII_ROW,
  [0x78]=ASCII_ROW,[0x79]=ASCII_ROW,[0x7A]=ASCII_ROW,[0x7B]=ASCII_ROW,
  [0x7C]=ASCII_ROW,[0x7D]=ASCII_ROW,[0x7E]=ASCII_ROW,[0x7F]=ASCII_ROW,
  // 80-8F
  [0x80]=CONT_80_8F,[0x81]=CONT_80_8F,[0x82]=CONT_80_8F,[0x83]=CONT_80_8F,
  [0x84]=CONT_80_8F,[0x85]=CONT_80_8F,[0x86]=CONT_80_8F,[0x87]=CONT_80_8F,
  [0x88]=CONT_80_8F,[0x89]=CONT_80_8F,[0x8A]=CONT_80_8F,[0x8B]=CONT_80_8F,
  [0x8C]=CONT_80_8F,[0x8D]=CONT_80_8F,[0x8E]=CONT_80_8F,[0x8F]=CONT_80_8F,
  // 90-9F
  [0x90]=CONT_90_9F,[0x91]=CONT_90_9F,[0x92]=CONT_90_9F,[0x93]=CONT_90_9F,
  [0x94]=CONT_90_9F,[0x95]=CONT_90_9F,[0x96]=CONT_90_9F,[0x97]=CONT_90_9F,
  [0x98]=CONT_90_9F,[0x99]=CONT_90_9F,[0x9A]=CONT_90_9F,[0x9B]=CONT_90_9F,
  [0x9C]=CONT_90_9F,[0x9D]=CONT_90_9F,[0x9E]=CONT_90_9F,[0x9F]=CONT_90_9F,
  // A0-BF
  [0xA0]=CONT_A0_BF,[0xA1]=CONT_A0_BF,[0xA2]=CONT_A0_BF,[0xA3]=CONT_A0_BF,
  [0xA4]=CONT_A0_BF,[0xA5]=CONT_A0_BF,[0xA6]=CONT_A0_BF,[0xA7]=CONT_A0_BF,
  [0xA8]=CONT_A0_BF,[0xA9]=CONT_A0_BF,[0xAA]=CONT_A0_BF,[0xAB]=CONT_A0_BF,
  [0xAC]=CONT_A0_BF,[0xAD]=CONT_A0_BF,[0xAE]=CONT_A0_BF,[0xAF]=CONT_A0_BF,
  [0xB0]=CONT_A0_BF,[0xB1]=CONT_A0_BF,[0xB2]=CONT_A0_BF,[0xB3]=CONT_A0_BF,
  [0xB4]=CONT_A0_BF,[0xB5]=CONT_A0_BF,[0xB6]=CONT_A0_BF,[0xB7]=CONT_A0_BF,
  [0xB8]=CONT_A0_BF,[0xB9]=CONT_A0_BF,[0xBA]=CONT_A0_BF,[0xBB]=CONT_A0_BF,
  [0xBC]=CONT_A0_BF,[0xBD]=CONT_A0_BF,[0xBE]=CONT_A0_BF,[0xBF]=CONT_A0_BF,
  // C0-C1
  [0xC0]=ERROR_ROW,[0xC1]=ERROR_ROW,
  // C2-DF
  [0xC2]=LEAD2_ROW,[0xC3]=LEAD2_ROW,[0xC4]=LEAD2_ROW,[0xC5]=LEAD2_ROW,
  [0xC6]=LEAD2_ROW,[0xC7]=LEAD2_ROW,[0xC8]=LEAD2_ROW,[0xC9]=LEAD2_ROW,
  [0xCA]=LEAD2_ROW,[0xCB]=LEAD2_ROW,[0xCC]=LEAD2_ROW,[0xCD]=LEAD2_ROW,
  [0xCE]=LEAD2_ROW,[0xCF]=LEAD2_ROW,[0xD0]=LEAD2_ROW,[0xD1]=LEAD2_ROW,
  [0xD2]=LEAD2_ROW,[0xD3]=LEAD2_ROW,[0xD4]=LEAD2_ROW,[0xD5]=LEAD2_ROW,
  [0xD6]=LEAD2_ROW,[0xD7]=LEAD2_ROW,[0xD8]=LEAD2_ROW,[0xD9]=LEAD2_ROW,
  [0xDA]=LEAD2_ROW,[0xDB]=LEAD2_ROW,[0xDC]=LEAD2_ROW,[0xDD]=LEAD2_ROW,
  [0xDE]=LEAD2_ROW,[0xDF]=LEAD2_ROW,
  // E0
  [0xE0]=E0_ROW,
  // E1-EC
  [0xE1]=LEAD3_ROW,[0xE2]=LEAD3_ROW,[0xE3]=LEAD3_ROW,[0xE4]=LEAD3_ROW,
  [0xE5]=LEAD3_ROW,[0xE6]=LEAD3_ROW,[0xE7]=LEAD3_ROW,[0xE8]=LEAD3_ROW,
  [0xE9]=LEAD3_ROW,[0xEA]=LEAD3_ROW,[0xEB]=LEAD3_ROW,[0xEC]=LEAD3_ROW,
  // ED
  [0xED]=ED_ROW,
  // EE-EF
  [0xEE]=LEAD3_ROW,[0xEF]=LEAD3_ROW,
  // F0
  [0xF0]=F0_ROW,
  // F1-F3
  [0xF1]=LEAD4_ROW,[0xF2]=LEAD4_ROW,[0xF3]=LEAD4_ROW,
  // F4
  [0xF4]=F4_ROW,
  // F5-FF
  [0xF5]=ERROR_ROW,[0xF6]=ERROR_ROW,[0xF7]=ERROR_ROW,[0xF8]=ERROR_ROW,
  [0xF9]=ERROR_ROW,[0xFA]=ERROR_ROW,[0xFB]=ERROR_ROW,[0xFC]=ERROR_ROW,
  [0xFD]=ERROR_ROW,[0xFE]=ERROR_ROW,[0xFF]=ERROR_ROW,
};

/* clang-format on */

#undef S_ERROR
#undef S_ACCEPT
#undef S_TL1
#undef S_TL2LO
#undef S_TL2HI
#undef S_TL3LO
#undef S_TL3MH

#undef ERR
#undef DFA_ROW
#undef ASCII_ROW
#undef CONT_80_8F
#undef CONT_90_9F
#undef CONT_A0_BF
#undef LEAD2_ROW
#undef LEAD3_ROW
#undef LEAD4_ROW
#undef ERROR_ROW
#undef E0_ROW
#undef ED_ROW
#undef F0_ROW
#undef F4_ROW

static inline utf8_rdfa_state_t utf8_rdfa_step(utf8_rdfa_state_t state,
                                               unsigned char c) {
  return (utf8_rdfa[c] >> state) & 63;
}

static inline utf8_rdfa_state_t utf8_rdfa_step_decode(utf8_rdfa_state_t state,
                                                      unsigned char c,
                                                      uint32_t* codepoint) {
  utf8_rdfa_state_t row = utf8_rdfa[c];
  uint32_t shift = ((state >> 2) & 3) * 6;
  *codepoint = *codepoint | (c & (uint32_t)(row >> 56)) << shift;
  return (row >> state) & 63;
}

static inline utf8_rdfa_state_t utf8_rdfa_run(utf8_rdfa_state_t state,
                                              const unsigned char* src,
                                              size_t len) {
  while (len > 0)
    state = utf8_rdfa_step(state, src[--len]);
  return state;
}

#ifdef __cplusplus
}
#endif
#endif // UTF8_RDFA64_H
