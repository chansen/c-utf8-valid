#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef UTF8_DFA_64
#  include "utf8_dfa64.h"
#else
#  include "utf8_dfa32.h"
#endif

#include "utf8_advance_forward.h"

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

static void test_forward_zero(void) {
  size_t advanced;
  CHECK(utf8_advance_forward(mixed, mixed_len, 0, &advanced) == 0,
        "forward: distance 0 -> offset 0");
  CHECK(advanced == 0, "forward: distance 0 -> advanced 0");
}

static void test_forward_one(void) {
  size_t advanced;
  CHECK(utf8_advance_forward(mixed, mixed_len, 1, &advanced) == 1,
        "forward: distance 1 -> offset 1");
  CHECK(advanced == 1, "forward: distance 1 -> advanced 1");
}

static void test_forward_two(void) {
  size_t advanced;
  CHECK(utf8_advance_forward(mixed, mixed_len, 2, &advanced) == 3,
        "forward: distance 2 -> offset 3");
  CHECK(advanced == 2, "forward: distance 2 -> advanced 2");
}

static void test_forward_three(void) {
  size_t advanced;
  CHECK(utf8_advance_forward(mixed, mixed_len, 3, &advanced) == 6,
        "forward: distance 3 -> offset 6");
  CHECK(advanced == 3, "forward: distance 3 -> advanced 3");
}

static void test_forward_exact(void) {
  size_t advanced;
  CHECK(utf8_advance_forward(mixed, mixed_len, 4, &advanced) == 10,
        "forward: distance 4 -> offset 10 (end)");
  CHECK(advanced == 4, "forward: distance 4 -> advanced 4");
}

static void test_forward_exceed(void) {
  size_t advanced;
  CHECK(utf8_advance_forward(mixed, mixed_len, 5, &advanced) == 10,
        "forward: distance 5 exceeds -> len");
  CHECK(advanced == 4, "forward: distance 5 exceeds -> advanced 4");

  CHECK(utf8_advance_forward(mixed, mixed_len, 100, &advanced) == 10,
        "forward: distance 100 exceeds -> len");
  CHECK(advanced == 4, "forward: distance 100 exceeds -> advanced 4");
}

static void test_forward_ascii(void) {
  size_t advanced;
  CHECK(utf8_advance_forward("ABC", 3, 2, &advanced) == 2,
        "forward ascii: distance 2 -> offset 2");
  CHECK(advanced == 2, "forward ascii: distance 2 -> advanced 2");
}

static void test_forward_empty(void) {
  size_t advanced;
  CHECK(utf8_advance_forward("", 0, 0, &advanced) == 0,
        "forward empty: distance 0 -> 0");
  CHECK(advanced == 0, "forward empty: distance 0 -> advanced 0");

  CHECK(utf8_advance_forward("", 0, 1, &advanced) == 0,
        "forward empty: distance 1 -> 0");
  CHECK(advanced == 0, "forward empty: distance 1 -> advanced 0");
}

static void test_forward_illformed(void) {
  size_t advanced;
  CHECK(utf8_advance_forward("\x80", 1, 1, &advanced) == (size_t)-1,
        "forward illformed: bare continuation -> -1");
  CHECK(advanced == 0, "forward illformed: bare continuation -> advanced 0");

  CHECK(utf8_advance_forward("A\x80", 2, 2, &advanced) == (size_t)-1,
        "forward illformed: valid then ill-formed -> -1");
  CHECK(advanced == 1, "forward illformed: valid then ill-formed -> advanced 1");

  CHECK(utf8_advance_forward("\xC3", 1, 1, &advanced) == (size_t)-1,
        "forward illformed: truncated -> -1");
  CHECK(advanced == 0, "forward illformed: truncated -> advanced 0");

  CHECK(utf8_advance_forward("\xF5", 1, 1, &advanced) == (size_t)-1,
        "forward illformed: invalid lead -> -1");
  CHECK(advanced == 0, "forward illformed: invalid lead -> advanced 0");
}

static void test_forward_null_advanced(void) {
  CHECK(utf8_advance_forward(mixed, mixed_len, 2, NULL) == 3,
        "forward null advanced: offset 3");
}

int main(void) {
  test_forward_zero();
  test_forward_one();
  test_forward_two();
  test_forward_three();
  test_forward_exact();
  test_forward_exceed();
  test_forward_ascii();
  test_forward_empty();
  test_forward_illformed();
  test_forward_null_advanced();

  if (TestFailed)
    printf("Failed %zu of %zu tests.\n", TestFailed, TestCount);
  else
    printf("All %zu tests passed.\n", TestCount);

  return TestFailed ? 1 : 0;
}
