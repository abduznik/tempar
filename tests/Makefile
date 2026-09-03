# Host-side unit tests (no PSP SDK needed) — runs on any dev machine / CI.
# Exercises the REAL pure-logic code via the shared headers in src/include.

CC ?= cc
CFLAGS = -Wall -Wextra -O2 -I../src/include -I.

BINS = test_address test_config_core

all: $(BINS)

test_address: test_address.c test_common.h ../src/include/addr.h
	$(CC) $(CFLAGS) -o $@ test_address.c

test_config_core: test_config_core.c test_common.h ../src/include/config_core.h
	$(CC) $(CFLAGS) -o $@ test_config_core.c

run: all
	@for b in $(BINS); do echo "== $$b =="; ./$$b || exit 1; done

clean:
	rm -f $(BINS)

.PHONY: all run clean