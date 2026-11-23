CC ?= cc
CPPFLAGS += -Iinclude
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror -pedantic

.PHONY: all test clean
all: test

build/test_boot_info: src/pc_boot_info.c tests/test_boot_info.c include/pc_boot_info.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) src/pc_boot_info.c tests/test_boot_info.c -o $@

test: build/test_boot_info
	./build/test_boot_info

clean:
	rm -rf build
