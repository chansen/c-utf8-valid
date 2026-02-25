CC      = cc
CFLAGS  = -std=c99 -O2 -Wall -fsanitize=address,undefined
TESTS   = utf8_valid_test utf8_valid_test64 utf8_valid_test_file utf8_valid_test_file64

all: test

utf8_valid_test: utf8_valid_test.c utf8_valid.h
	$(CC) $(CFLAGS) -o utf8_valid_test utf8_valid_test.c

utf8_valid_test64: utf8_valid_test.c utf8_valid64.h
	$(CC) $(CFLAGS) -DUTF8_VALID_64 -o utf8_valid_test64 utf8_valid_test.c

utf8_valid_test_file: utf8_valid_test_file.c utf8_valid.h
	$(CC) $(CFLAGS) -o utf8_valid_test_file utf8_valid_test_file.c

utf8_valid_test_file64: utf8_valid_test_file.c utf8_valid64.h
	$(CC) $(CFLAGS) -DUTF8_VALID_64 -o utf8_valid_test_file64 utf8_valid_test_file.c

test: $(TESTS)
	./utf8_valid_test
	./utf8_valid_test64
	./utf8_valid_test_file utf8tests.txt
	./utf8_valid_test_file64 utf8tests.txt

clean:
	rm -f $(TESTS)

.PHONY: all test clean
