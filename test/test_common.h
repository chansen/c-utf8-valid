#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

static size_t TestCount  = 0;
static size_t TestFailed = 0;

/*
 * Encodes the given ordinal [0, 7FFFFFFF] using the UTF-8 encoding scheme
 * to the given sequence length [1, 6]. Can produce well-formed and
 * ill-formed UTF-8.
 */
static inline char *
encode_ord(uint32_t ord, size_t len, char *dst) {
  static const uint32_t kMask[6] = { 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
  static const uint32_t kMax[6]  = { 1u <<  7, 1u << 11, 1u << 16,
                                     1u << 21, 1u << 26, 1u << 31 };
  size_t i;

  assert(len >= 1);
  assert(len <= 6);
  assert(ord < kMax[len - 1]);

  for (i = len - 1; i > 0; i--) {
    dst[i] = (char)((ord & 0x3F) | 0x80);
    ord >>= 6;
  }
  dst[0] = (char)(ord | kMask[len - 1]);
  return dst;
}

static inline char *
escape_str(const char *src, size_t len, char *dst) {
  static const char * const kHex = "0123456789ABCDEF";
  size_t i;
  char *d;

  for (d = dst, i = 0; i < len; i++) {
    const unsigned char c = (unsigned char)src[i];
    if (c >= ' ' && c <= '~') {
      if (c == '\\' || c == '"')
        *d++ = '\\';
      *d++ = (char)c;
    }
    else {
      *d++ = '\\';
      *d++ = 'x';
      *d++ = kHex[c >> 4];
      *d++ = kHex[c & 0x0F];
    }
  }
  *d = '\0';
  return dst;
}

static inline int
report_results(void) {
  if (TestFailed)
    printf("FAILED %zu tests of %zu.\n", TestFailed, TestCount);
  else
    printf("Passed %zu tests.\n", TestCount);
  return TestFailed ? 1 : 0;
}
