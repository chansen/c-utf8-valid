CPPFLAGS  = -I.
CC       ?= cc

TEST_DIR        = test
TEST_DATA       = $(TEST_DIR)/data/utf8tests.txt
TEST_CFLAGS     = -std=c99 -O2 -Wall -fsanitize=address,undefined

BENCH_DIR       = benchmark
BENCH_SRC       = $(BENCH_DIR)/bench.c
BENCH_BIN       = bench
BENCH_OPTFLAGS ?= -O3 -march=native
BENCH_CFLAGS   ?= -std=c99 -Wall $(BENCH_OPTFLAGS)

TESTS = \
	dfa_step32 \
	dfa_step64 \
	dfa_step_decode \
	rdfa_step32 \
	rdfa_step64 \
	rdfa_step_decode \
	valid32 \
	valid64 \
	valid_stream32 \
	valid_stream64 \
	valid_file32 \
	valid_file64 \
	advance_forward32 \
	advance_forward64 \
	advance_backward32 \
	advance_backward64 \
	distance32 \
	distance64 \
	decode_next \
	decode_prev \
	transcode_utf16 \
	transcode_utf32

all: test

dfa_step32: $(TEST_DIR)/dfa_step.c utf8_dfa32.h
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/dfa_step.c

dfa_step64: $(TEST_DIR)/dfa_step.c utf8_dfa64.h
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/dfa_step.c

dfa_step_decode: $(TEST_DIR)/dfa_step_decode.c utf8_dfa64.h
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/dfa_step_decode.c

rdfa_step32: $(TEST_DIR)/rdfa_step.c utf8_rdfa32.h
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/rdfa_step.c

rdfa_step64: $(TEST_DIR)/rdfa_step.c utf8_rdfa64.h
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_RDFA_64 -o $@ $(TEST_DIR)/rdfa_step.c

rdfa_step_decode: $(TEST_DIR)/rdfa_step_decode.c utf8_rdfa64.h
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/rdfa_step_decode.c

valid32: $(TEST_DIR)/valid.c utf8_dfa32.h utf8_valid.h
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/valid.c

valid64: $(TEST_DIR)/valid.c utf8_dfa64.h utf8_valid.h
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/valid.c

valid_stream32: $(TEST_DIR)/valid_stream.c utf8_dfa32.h utf8_valid_stream.h
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/valid_stream.c

valid_stream64: $(TEST_DIR)/valid_stream.c utf8_dfa64.h utf8_valid_stream.h
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/valid_stream.c

valid_file32: $(TEST_DIR)/valid_file.c utf8_dfa32.h utf8_valid.h
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/valid_file.c

valid_file64: $(TEST_DIR)/valid_file.c utf8_dfa64.h utf8_valid.h
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/valid_file.c

advance_forward32: $(TEST_DIR)/valid_file.c utf8_dfa32.h utf8_advance_forward.h
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/advance_forward.c

advance_forward64: $(TEST_DIR)/valid_file.c utf8_dfa64.h utf8_advance_forward.h
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64  -o $@ $(TEST_DIR)/advance_forward.c

advance_backward32: $(TEST_DIR)/valid_file.c utf8_rdfa32.h utf8_advance_backward.h
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/advance_backward.c

advance_backward64: $(TEST_DIR)/valid_file.c utf8_rdfa64.h utf8_advance_backward.h
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_RDFA_64  -o $@ $(TEST_DIR)/advance_backward.c
	
distance32: $(TEST_DIR)/valid_file.c utf8_dfa32.h utf8_distance.h
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/distance.c

distance64: $(TEST_DIR)/valid_file.c utf8_dfa64.h utf8_distance.h
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64  -o $@ $(TEST_DIR)/distance.c

decode_next: $(TEST_DIR)/decode_next.c utf8_dfa64.h utf8_decode_next.h
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/decode_next.c

decode_prev: $(TEST_DIR)/decode_prev.c utf8_rdfa64.h utf8_decode_prev.h
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/decode_prev.c

transcode_utf16: $(TEST_DIR)/transcode_utf16.c utf8_dfa64.h utf8_transcode.h
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/transcode_utf16.c

transcode_utf32: $(TEST_DIR)/transcode_utf32.c utf8_dfa64.h utf8_transcode.h
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/transcode_utf32.c

test: $(TESTS)
	./dfa_step32
	./dfa_step64
	./dfa_step_decode
	./rdfa_step32
	./rdfa_step64
	./rdfa_step_decode
	./valid32
	./valid64
	./valid_stream32
	./valid_stream64
	./valid_file32 $(TEST_DATA)
	./valid_file64 $(TEST_DATA)
	./advance_forward32
	./advance_forward64
	./advance_backward32
	./advance_backward64
	./distance32
	./distance64
	./decode_next
	./decode_prev
	./transcode_utf16
	./transcode_utf32

bench: $(BENCH_SRC) utf8_valid.h
	$(CC) $(CPPFLAGS) $(BENCH_CFLAGS) -o $(BENCH_BIN) $(BENCH_SRC)

clean:
	rm -f $(TESTS) $(BENCH_BIN)

.PHONY: all test clean bench
