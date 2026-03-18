#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef UTF8_RDFA_64
#  include "utf8_rdfa64.h"
#else
#  include "utf8_rdfa32.h"
#endif

#include "utf8_advance_backward.h"

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

static void test_backward_zero(void) {
  size_t advanced;
  CHECK(utf8_advance_backward(mixed, mixed_len, 0, &advanced) == mixed_len,
        "backward: distance 0 -> offset len");
  CHECK(advanced == 0, "backward: distance 0 -> advanced 0");
}

static void test_backward_one(void) {
  size_t advanced;
  CHECK(utf8_advance_backward(mixed, mixed_len, 1, &advanced) == 6,
        "backward: distance 1 -> offset 6");
  CHECK(advanced == 1, "backward: distance 1 -> advanced 1");
}

static void test_backward_two(void) {
  size_t advanced;
  CHECK(utf8_advance_backward(mixed, mixed_len, 2, &advanced) == 3,
        "backward: distance 2 -> offset 3");
  CHECK(advanced == 2, "backward: distance 2 -> advanced 2");
}

static void test_backward_three(void) {
  size_t advanced;
  CHECK(utf8_advance_backward(mixed, mixed_len, 3, &advanced) == 1,
        "backward: distance 3 -> offset 1");
  CHECK(advanced == 3, "backward: distance 3 -> advanced 3");
}

static void test_backward_exact(void) {
  size_t advanced;
  CHECK(utf8_advance_backward(mixed, mixed_len, 4, &advanced) == 0,
        "backward: distance 4 -> offset 0");
  CHECK(advanced == 4, "backward: distance 4 -> advanced 4");
}

static void test_backward_exceed(void) {
  size_t advanced;
  CHECK(utf8_advance_backward(mixed, mixed_len, 5, &advanced) == 0,
        "backward: distance 5 exceeds -> offset 0");
  CHECK(advanced == 4, "backward: distance 5 exceeds -> advanced 4");

  CHECK(utf8_advance_backward(mixed, mixed_len, 100, &advanced) == 0,
        "backward: distance 100 exceeds -> offset 0");
  CHECK(advanced == 4, "backward: distance 100 exceeds -> advanced 4");
}

static void test_backward_ascii(void) {
  size_t advanced;
  CHECK(utf8_advance_backward("ABC", 3, 2, &advanced) == 1,
        "backward ascii: distance 2 -> offset 1");
  CHECK(advanced == 2, "backward ascii: distance 2 -> advanced 2");
}

static void test_backward_empty(void) {
  size_t advanced;
  CHECK(utf8_advance_backward("", 0, 0, &advanced) == 0,
        "backward empty: distance 0 -> 0");
  CHECK(advanced == 0, "backward empty: distance 0 -> advanced 0");

  CHECK(utf8_advance_backward("", 0, 1, &advanced) == 0,
        "backward empty: distance 1 -> 0");
  CHECK(advanced == 0, "backward empty: distance 1 -> advanced 0");
}

static void test_backward_illformed(void) {
  size_t advanced;
  CHECK(utf8_advance_backward("\x80", 1, 1, &advanced) == (size_t)-1,
        "backward illformed: bare continuation -> -1");
  CHECK(advanced == 0, "backward illformed: bare continuation -> advanced 0");

  CHECK(utf8_advance_backward("\xC3", 1, 1, &advanced) == (size_t)-1,
        "backward illformed: truncated -> -1");
  CHECK(advanced == 0, "backward illformed: truncated -> advanced 0");

  CHECK(utf8_advance_backward("\xF5", 1, 1, &advanced) == (size_t)-1,
        "backward illformed: invalid lead -> -1");
  CHECK(advanced == 0, "backward illformed: invalid lead -> advanced 0");
}

static void test_backward_null_advanced(void) {
  CHECK(utf8_advance_backward(mixed, mixed_len, 2, NULL) == 3,
        "backward null advanced: offset 3");
}

int main(void) {
  test_backward_zero();
  test_backward_one();
  test_backward_two();
  test_backward_three();
  test_backward_exact();
  test_backward_exceed();
  test_backward_ascii();
  test_backward_empty();
  test_backward_illformed();
  test_backward_null_advanced();

  if (TestFailed)
    printf("Failed %zu of %zu tests.\n", TestFailed, TestCount);
  else
    printf("All %zu tests passed.\n", TestCount);

  return TestFailed ? 1 : 0;
}
