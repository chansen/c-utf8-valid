CC      = cc
CFLAGS  = -std=c99 -O2 -Wall -fsanitize=address,undefined

TESTS   = utf8_valid_test utf8_valid_test_file

all: test

utf8_valid_test: utf8_valid_test.c utf8_valid.h
	$(CC) $(CFLAGS) -o $@ $<

utf8_valid_test_file: utf8_valid_test_file.c utf8_valid.h
	$(CC) $(CFLAGS) -o $@ $<

test: $(TESTS)
	./utf8_valid_test
	./utf8_valid_test_file utf8tests.txt

clean:
	rm -f $(TESTS)

.PHONY: all test clean
