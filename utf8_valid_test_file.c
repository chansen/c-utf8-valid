#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#define UTF8_VALID_FAST_PATH4 1
#define UTF8_VALID_FAST_PATH16 1
#include "utf8_valid.h"

static size_t TestCount = 0;
static size_t TestFailed = 0;

/*
 * Parse a hex string like "C2 A9 80" into bytes.
 * Returns number of bytes parsed.
 */
static size_t parse_hex(const char* src, unsigned char* dst, size_t dst_len) {
  size_t len = 0;
  while (*src) {
    while (*src == ' ' || *src == '\t')
      src++;
    if (!*src)
      break;
    if (!isxdigit((unsigned char)src[0]) || !isxdigit((unsigned char)src[1])) {
      fprintf(stderr, "Bad hex at: %s\n", src);
      break;
    }
    assert(len < dst_len);
    char buf[3] = {src[0], src[1], 0};
    dst[len++] = (unsigned char)strtol(buf, NULL, 16);
    src += 2;
  }
  return len;
}

/*
 * Parse and run a single test line from utf8tests.txt.
 * Format:
 *   num:valid:ASCII bytes
 *   num:valid hex:hexString
 *   num:invalid hex:hexString:hexString2:hexString3
 */
static void run_test_line(char* line, unsigned lineno) {
  /* Skip comments and blank lines */
  if (line[0] == '#' || line[0] == '\n' || line[0] == '\r' || line[0] == '\0')
    return;

  /* Parse num */
  char* p = strchr(line, ':');
  if (!p)
    return;
  *p++ = '\0';
  /* const char *num = line; -- available if needed */

  /* Parse valid/invalid */
  char* kind = p;
  p = strchr(p, ':');
  if (!p)
    return;
  *p++ = '\0';

  /* Trim trailing whitespace from kind */
  char* end = kind + strlen(kind) - 1;
  while (end > kind && (*end == ' ' || *end == '\t'))
    *end-- = '\0';

  int is_valid;
  int is_hex;

  if (strcmp(kind, "valid") == 0) {
    is_valid = 1;
    is_hex = 0;
  } else if (strcmp(kind, "valid hex") == 0) {
    is_valid = 1;
    is_hex = 1;
  } else if (strcmp(kind, "invalid hex") == 0) {
    is_valid = 0;
    is_hex = 1;
  } else {
    return;
  }

  /* Parse input bytes */
  unsigned char src[256];
  size_t src_len;

  if (is_hex) {
    /* Next field is hex string */
    char* hex = p;
    p = strchr(p, ':');
    if (p)
      *p++ = '\0';
    src_len = parse_hex(hex, src, sizeof(src));
  } else {
    /* ASCII bytes directly */
    src_len = strlen(p);
    assert(src_len < sizeof(src));
    memcpy(src, p, src_len);
    /* strip newline */
    while (src_len > 0 &&
           (src[src_len - 1] == '\n' || src[src_len - 1] == '\r'))
      src_len--;
  }

  /* Run utf8_check */
  size_t cursor;
  bool got = utf8_check((const char*)src, src_len, &cursor);

  TestCount++;
  if ((int)got != is_valid) {
    fprintf(stderr, "FAIL line %u: expected %s, got %s\n", lineno,
            is_valid ? "valid" : "invalid", got ? "valid" : "invalid");
    TestFailed++;
  }
}

static void run_file(const char* path) {
  FILE* f = fopen(path, "r");
  if (!f) {
    fprintf(stderr, "Cannot open %s\n", path);
    exit(1);
  }

  char line[4096];
  unsigned lineno = 0;
  while (fgets(line, sizeof(line), f)) {
    lineno++;
    /* Strip trailing newline */
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
      line[--len] = '\0';
    run_test_line(line, lineno);
  }

  fclose(f);
}

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s utf8tests.txt\n", argv[0]);
    return 1;
  }

  run_file(argv[1]);

  if (TestFailed)
    printf("Failed %zu tests of %zu.\n", TestFailed, TestCount);
  else
    printf("Passed %zu tests.\n", TestCount);

  return TestFailed ? 1 : 0;
}
