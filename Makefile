# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Zak Noble-Clarke

CC := cc
CFLAGS := -std=c11 -Wall -Wextra -Werror
CPPFLAGS := -Iinclude

manifest := $(shell cat sources.list)
srcs := $(filter src/%.c,$(manifest))
headers := $(filter %.h,$(manifest))

.PHONY: test clean

test: build/test_abi build/test_batch build/test_response
	./build/test_abi
	./build/test_batch
	./build/test_response

build/test_abi: tests/test_abi.c $(srcs) $(headers) sources.list
	mkdir -p build
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ tests/test_abi.c $(srcs)

build/test_batch: tests/test_batch.c $(srcs) $(headers) sources.list
	mkdir -p build
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ tests/test_batch.c $(srcs)

build/test_response: tests/test_response.c $(srcs) $(headers) sources.list
	mkdir -p build
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ tests/test_response.c $(srcs)

clean:
	rm -f build/test_abi build/test_batch build/test_response tests/test_abi tests/test_batch
