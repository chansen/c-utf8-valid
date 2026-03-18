#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef UTF8_DFA_64
#  include "utf8_dfa64.h"
#else
#  include "utf8_dfa32.h"
#endif
#include "utf8_distance.h"

static size_t TestCount  = 0;
static size_t TestFailed = 0;

#define CHECK(cond, msg)          \
  do {                            \
    TestCount++;                  \
    if (!(cond)) {                \
      printf("FAIL: %s (line %d)\n", msg, __LINE__); \
      TestFailed++;               \
    }                             \
  } while (0)

// "A é € 𐍈" = 1+2+3+4 = 10 bytes, 4 codepoints 
static const char   *mixed     = "A\xC3\xA9\xE2\x82\xAC\xF0\x90\x8D\x88";
static const size_t  mixed_len = 10;

static void test_distance_empty(void) {
  CHECK(utf8_distance("", 0) == 0,
        "distance empty: 0");
}

static void test_distance_ascii(void) {
  CHECK(utf8_distance("ABC", 3) == 3,
        "distance ascii: 3");
}

static void test_distance_mixed(void) {
  CHECK(utf8_distance(mixed, mixed_len) == 4,
        "distance mixed: 4 codepoints");
}

static void test_distance_partial(void) {
  // just "A é" = 3 bytes, 2 codepoints 
  CHECK(utf8_distance(mixed, 3) == 2,
        "distance partial: 2 codepoints");
}

static void test_distance_illformed(void) {
  CHECK(utf8_distance("\x80", 1) == (size_t)-1,
        "distance illformed: bare continuation -> -1");
  CHECK(utf8_distance("A\x80", 2) == (size_t)-1,
        "distance illformed: valid then ill-formed -> -1");
  CHECK(utf8_distance("\xC3", 1) == (size_t)-1,
        "distance illformed: truncated -> -1");
  CHECK(utf8_distance("\xF5", 1) == (size_t)-1,
        "distance illformed: invalid lead -> -1");
}

int main(void) {
  test_distance_empty();
  test_distance_ascii();
  test_distance_mixed();
  test_distance_partial();
  test_distance_illformed();

  if (TestFailed)
    printf("Failed %zu of %zu tests.\n", TestFailed, TestCount);
  else
    printf("All %zu tests passed.\n", TestCount);

  return TestFailed ? 1 : 0;
}
