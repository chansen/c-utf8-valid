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
 * bench.c -- compare implementations
 *
 * Compile:
 *   cc -std=c99 -O3 -march=native -o bench bench.c
 *
 * Usage:
 *   ./bench                        # benchmarks ./corpus/
 *   ./bench -d <directory>         # benchmarks [A-Za-z][A-Za-z]+.txt in directory
 *   ./bench -f <file>              # benchmarks single file
 *   ./bench -s <MB>                # resize input to <MB> megabytes before benchmarking
 *   ./bench -t <seconds>           # run each benchmark for <seconds> (default: 20)
 *   ./bench -n <reps>              # run each benchmark for <reps> repetitions
 *   ./bench -b <MB>                # run each benchmark until <MB> total data processed
 *
 * -t, -n, and -b are mutually exclusive.
 */

#if defined(__linux__)
#define _POSIX_C_SOURCE 199309L
#endif

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <ctype.h>
#include <time.h>

#include "utf8_dfa32.h"
#include "utf8_valid.h"

/* ===== Hoehrmann DFA validator ===== */
/* Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>        */
/* See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.        */

static const uint8_t hoehrmann_utf8d[] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3,
  0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,
  0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1,
  1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,
  1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1,
  1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
};

static __attribute__((noinline)) bool
utf8_valid_hoehrmann(const char* src, size_t len) {
  uint32_t state = 0;
  const unsigned char* s = (const unsigned char*)src;
  for (size_t i = 0; i < len; i++)
    state = hoehrmann_utf8d[256 + state * 16 + hoehrmann_utf8d[s[i]]];
  return state == 0;
}

// Previous (old) implementation of utf8_valid.h

static bool
utf8_check_old(const char *src, size_t len, size_t *cursor) {
  const unsigned char *cur = (const unsigned char *)src;
  const unsigned char *end = cur + len;
  unsigned char buf[4];
  uint32_t v;

  while (1) {
    const unsigned char *p;
    if (cur >= end - 3) {
      if (cur == end)
        break;
      memset(buf, 0, 4);
      memcpy(buf, cur, end - cur);
      p = (const unsigned char *)buf;
    } else {
      p = cur;
    }

    v = p[0];
    /* 0xxxxxxx */
    if ((v & 0x80) == 0) {
      cur += 1;
      continue;
    }

    v = (v << 8) | p[1];
    /* 110xxxxx 10xxxxxx */
    if ((v & 0xE0C0) == 0xC080) {
      /* Ensure that the top 4 bits is not zero */
      v = v & 0x1E00;
      if (v == 0)
        break;
      cur += 2;
      continue;
    }

    v = (v << 8) | p[2];
    /* 1110xxxx 10xxxxxx 10xxxxxx */
    if ((v & 0xF0C0C0) == 0xE08080) {
      /* Ensure that the top 5 bits is not zero and not a surrogate */
      v = v & 0x0F2000;
      if (v == 0 || v == 0x0D2000)
        break;
      cur += 3;
      continue;
    }

    v = (v << 8) | p[3];
    /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
    if ((v & 0xF8C0C0C0) == 0xF0808080) {
      /* Ensure that the top 5 bits is not zero and not out of range */
      v = v & 0x07300000;
      if (v == 0 || v > 0x04000000)
        break;
      cur += 4;
      continue;
    }

    break;
  }

  if (cursor)
    *cursor = (const char *)cur - src;

  return cur == end;
}

static __attribute__((noinline)) bool
utf8_valid_old(const char *src, size_t len) {
  return utf8_check_old(src, len, NULL);
}

typedef bool (*utf8_valid_fn)(const char* src, size_t len);

typedef struct {
  const char*   name;
  utf8_valid_fn fn;
} bench_entry_t;

static const bench_entry_t bench_entries[] = {
  {"hoehrmann",           utf8_valid_hoehrmann},
  {"utf8_valid_old",      utf8_valid_old},
  {"utf8_valid",          utf8_valid},
  {"utf8_valid_ascii",    utf8_valid_ascii},
};

#define BENCH_COUNT (sizeof(bench_entries) / sizeof(bench_entries[0]))

typedef enum { 
  MODE_TIME, 
  MODE_REPS, 
  MODE_BYTES 
} bench_mode_t;

typedef struct {
  bench_mode_t mode;
  double       seconds;   /* MODE_TIME  */
  long         reps;      /* MODE_REPS  */
  double       bytes_mb;  /* MODE_BYTES */
  int          warmup;    /* -1 = auto  */
} bench_opts_t;

static double now_seconds(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void format_size(size_t bytes, char* buf, size_t buflen) {
  if (bytes >= (size_t)1024 * 1024 * 1024)
    snprintf(buf, buflen, "%.0f GB", bytes / (1024.0 * 1024.0 * 1024.0));
  else if (bytes >= 1024 * 1024)
    snprintf(buf, buflen, "%.0f MB", bytes / (1024.0 * 1024.0));
  else if (bytes >= 1024)
    snprintf(buf, buflen, "%.0f KB", bytes / 1024.0);
  else
    snprintf(buf, buflen, "%zu bytes", bytes);
}

static void format_count(size_t n, char* buf, size_t buflen) {
  if (n >= 1000000000)
    snprintf(buf, buflen, "%.0fB", n / 1000000000.0);
  else if (n >= 1000000)
    snprintf(buf, buflen, "%.0fM", n / 1000000.0);
  else if (n >= 1000)
    snprintf(buf, buflen, "%.0fK", n / 1000.0);
  else
    snprintf(buf, buflen, "%zu", n);
}

static unsigned char* resize_buf(const unsigned char* src,
                                 size_t src_len,
                                 size_t target_len,
                                 size_t* out_len) {
  unsigned char* buf = malloc(target_len);
  if (!buf) {
    fprintf(stderr, "OOM allocating %zu bytes\n", target_len);
    return NULL;
  }

  size_t off = 0;
  while (off < target_len) {
    size_t copy = src_len < target_len - off ? src_len : target_len - off;
    memcpy(buf + off, src, copy);
    off += copy;
  }

  // Retreat past any trailing continuation bytes
  size_t len = target_len;
  while (len > 0 && (buf[len - 1] & 0xC0) == 0x80)
    len--;

  // Retreat past the lead byte if its sequence is incomplete
  if (len > 0) {
    unsigned char lead = buf[len - 1];
    size_t seq = lead < 0x80 ? 1
               : lead < 0xE0 ? 2
               : lead < 0xF0 ? 3 : 4;
    if (len - 1 + seq > target_len)
      len--;
  }

  *out_len = len;
  return buf;
}

static void print_compiler(void) {
#if defined(__clang__)
  printf("Compiler: Clang %d.%d.%d\n", 
    __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
  printf("Compiler: GCC %d.%d.%d\n", 
    __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif
}

static void print_stats(const char* name, const unsigned char* s, size_t len) {
  static const char* labels[] = {
      "U+0000..U+007F",
      "U+0080..U+07FF",
      "U+0800..U+FFFF",
      "U+10000..U+10FFFF",
  };
  size_t counts[4] = {0, 0, 0, 0};
  size_t total = 0;
  size_t i = 0;

  while (i < len) {
    unsigned char b = s[i];
    if (b < 0x80) {
      counts[0]++;
      i += 1;
    } else if (b < 0xE0) {
      counts[1]++;
      i += 2;
    } else if (b < 0xF0) {
      counts[2]++;
      i += 3;
    } else {
      counts[3]++;
      i += 4;
    }
    total++;
  }

  char sizebuf[32], countbuf[32];
  format_size(len, sizebuf, sizeof(sizebuf));
  format_count(total, countbuf, sizeof(countbuf));
  printf("%s: %s; %s code points; %.2f units/point\n", 
    name, sizebuf, countbuf, (double)len / total);

  for (int r = 0; r < 4; r++) {
    if (counts[r]) {
      char cbuf[32];
      format_count(counts[r], cbuf, sizeof(cbuf));
      printf("  %-20s %6s  %4.1f%%\n", labels[r], cbuf,
             100.0 * counts[r] / total);
    }
  }
}

static unsigned char* read_file(const char* path, size_t* out_len) {
  FILE* f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "Cannot open %s\n", path);
    return NULL;
  }

  unsigned char tmp[65536];
  unsigned char* buf = NULL;
  size_t len = 0, cap = 0;
  size_t n;

  while ((n = fread(tmp, 1, sizeof(tmp), f)) > 0) {
    if (len + n > cap) {
      cap = (len + n) * 2 + 65536;
      unsigned char* nb = realloc(buf, cap);
      if (!nb) {
        fclose(f);
        free(buf);
        fprintf(stderr, "OOM reading %s\n", path);
        return NULL;
      }
      buf = nb;
    }
    memcpy(buf + len, tmp, n);
    len += n;
  }

  if (ferror(f)) {
    fclose(f);
    free(buf);
    fprintf(stderr, "Read error: %s\n", path);
    return NULL;
  }

  fclose(f);
  *out_len = len;
  return buf;
}

static double run_bench(utf8_valid_fn fn,
                        const unsigned char* buf,
                        size_t len,
                        const bench_opts_t* opts) {
  double mb = len / (1024.0 * 1024.0);

  int warmup;
  if (opts->warmup >= 0) {
    warmup = opts->warmup;
  } else {
    double warmup_mb = 256.0;
    long warmup_iters = (long)(warmup_mb / mb);
    if (warmup_iters < 1)
      warmup_iters = 1;
    if (warmup_iters > 100)
      warmup_iters = 100;
    warmup = (int)warmup_iters;
  }

  for (int w = 0; w < warmup; w++)
    (void)fn((const char*)buf, len);

  double t = now_seconds();
  long iters = 0;

  switch (opts->mode) {
    case MODE_TIME:
      while (now_seconds() - t < opts->seconds) {
        (void)fn((const char*)buf, len);
        iters++;
      }
      break;
    case MODE_REPS:
      for (iters = 0; iters < opts->reps; iters++)
        (void)fn((const char*)buf, len);
      break;
    case MODE_BYTES: {
      double target_iters = (opts->bytes_mb * 1024.0 * 1024.0) / len;
      long n = (long)(target_iters + 0.5);
      if (n < 1)
        n = 1;
      for (iters = 0; iters < n; iters++)
        (void)fn((const char*)buf, len);
      break;
    }
  }

  double elapsed = now_seconds() - t;
  return (double)iters / elapsed * mb;
}

static void bench_file(const char* path,
                       size_t target_mb,
                       const bench_opts_t* opts) {
  size_t file_len;
  unsigned char* file_buf = read_file(path, &file_len);
  if (!file_buf)
    return;

  const char* name = strrchr(path, '/');
  name = name ? name + 1 : path;

  unsigned char* buf;
  size_t len;

  if (target_mb > 0) {
    size_t target_bytes = target_mb * 1024 * 1024;
    buf = resize_buf(file_buf, file_len, target_bytes, &len);
    free(file_buf);
    if (!buf)
      return;
  } else {
    buf = file_buf;
    len = file_len;
  }

  print_stats(name, buf, len);

  for (size_t i = 1; i < BENCH_COUNT; i++) {
    bool r0 = bench_entries[0].fn((const char*)buf, len);
    bool ri = bench_entries[i].fn((const char*)buf, len);
    if (r0 != ri) {
      fprintf(stderr, "MISMATCH on %s: %s=%d %s=%d\n", path,
              bench_entries[0].name, r0, bench_entries[i].name, ri);
      free(buf);
      return;
    }
  }

  double rates[BENCH_COUNT];
  for (size_t i = 0; i < BENCH_COUNT; i++)
    rates[i] = run_bench(bench_entries[i].fn, buf, len, opts);

  size_t order[BENCH_COUNT];
  for (size_t i = 0; i < BENCH_COUNT; i++)
    order[i] = i;
  for (size_t i = 1; i < BENCH_COUNT; i++)
    for (size_t j = i; j > 0 && rates[order[j]] < rates[order[j - 1]]; j--) {
      size_t tmp = order[j];
      order[j] = order[j - 1];
      order[j - 1] = tmp;
    }

  for (size_t i = 0; i < BENCH_COUNT; i++) {
    size_t idx = order[i];
    if (i == 0)
      printf("  %-22s  %8.0f MB/s\n", bench_entries[idx].name, rates[idx]);
    else
      printf("  %-22s  %8.0f MB/s  (%+.0f%%)\n", bench_entries[idx].name,
             rates[idx], (rates[idx] / rates[order[0]] - 1.0) * 100.0);
  }
  printf("\n");

  free(buf);
}

static int is_bench_file(const char* name) {
  int i = 0;
  while (isalpha((unsigned char)name[i]))
    i++;
  if (i < 2)
    return 0;
  return strcasecmp(name + i, ".txt") == 0;
}

static int cmp_str(const void* a, const void* b) {
  return strcmp(*(const char**)a, *(const char**)b);
}

static void bench_dir(const char* dir,
                      size_t target_mb,
                      const bench_opts_t* opts) {
  DIR* d = opendir(dir);
  if (!d) {
    perror(dir);
    return;
  }

  char* names[256];
  int n = 0;
  struct dirent* e;
  while ((e = readdir(d)) != NULL) {
    if (is_bench_file(e->d_name) && n < 256) {
      names[n] = malloc(strlen(e->d_name) + 1);
      if (names[n])
        strcpy(names[n++], e->d_name);
    }
  }
  closedir(d);

  if (n == 0) {
    fprintf(stderr, "No [A-Za-z][A-Za-z].txt files found in %s\n", dir);
    return;
  }

  qsort(names, n, sizeof(names[0]), cmp_str);

  for (int i = 0; i < n; i++) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
    bench_file(path, target_mb, opts);
    free(names[i]);
  }
}

static void usage(const char* prog) {
  fprintf(stderr,
    "Usage: %s [-d <dir>] [-f <file>] [-s <MB>] [-t <secs>|-n <reps>|-b <MB>] [-w <warmups>]\n"
    "\n"
    "  -d <dir>      benchmark all .txt files in directory (default: ./corpus)\n"
    "  -f <file>     benchmark single file\n"
    "  -s <MB>       resize input to <MB> megabytes before benchmarking\n"
    "  -t <secs>     run each function for <secs> seconds (default: 20)\n"
    "  -n <reps>     run each function for <reps> repetitions\n"
    "  -b <MB>       run each function until <MB> total data processed\n"
    "  -w <warmups>  number of warmup iterations (default: auto)\n",
    prog);
}

int main(int argc, char** argv) {
  const char* dir = NULL;
  const char* file = NULL;
  size_t target_mb = 0;

  bench_opts_t opts = {MODE_TIME, 20.0, 0, 0.0, -1};
  int mode_set = 0;
  int warmup_set = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
      dir = argv[++i];
    } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
      file = argv[++i];
    } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
      long v = atol(argv[++i]);
      if (v <= 0) {
        fprintf(stderr, "Invalid -s value: %s\n", argv[i]);
        return 1;
      }
      target_mb = (size_t)v;
    } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
      if (mode_set) {
        fprintf(stderr, "-t/-n/-b are mutually exclusive\n");
        return 1;
      }
      opts.mode = MODE_TIME;
      opts.seconds = atof(argv[++i]);
      if (opts.seconds <= 0.0) {
        fprintf(stderr, "Invalid -t value: %s\n", argv[i]);
        return 1;
      }
      mode_set = 1;
    } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
      if (mode_set) {
        fprintf(stderr, "-t/-n/-b are mutually exclusive\n");
        return 1;
      }
      opts.mode = MODE_REPS;
      opts.reps = atol(argv[++i]);
      if (opts.reps <= 0) {
        fprintf(stderr, "Invalid -n value: %s\n", argv[i]);
        return 1;
      }
      mode_set = 1;
    } else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
      if (mode_set) {
        fprintf(stderr, "-t/-n/-b are mutually exclusive\n");
        return 1;
      }
      opts.mode = MODE_BYTES;
      opts.bytes_mb = atof(argv[++i]);
      if (opts.bytes_mb <= 0.0) {
        fprintf(stderr, "Invalid -b value: %s\n", argv[i]);
        return 1;
      }
      mode_set = 1;
    } else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
      if (warmup_set) {
        fprintf(stderr, "-w specified twice\n");
        return 1;
      }
      opts.warmup = (int)atol(argv[++i]);
      if (opts.warmup < 0) {
        fprintf(stderr, "Invalid -w value: %s\n", argv[i]);
        return 1;
      }
      warmup_set = 1;
    } else {
      usage(argv[0]);
      return 1;
    }
  }

  if (file && dir) {
    fprintf(stderr, "-d and -f are mutually exclusive\n");
    return 1;
  }
  
  print_compiler();

  if (file)
    bench_file(file, target_mb, &opts);
  else
    bench_dir(dir ? dir : "benchmark/corpus", target_mb, &opts);

  return 0;
}
