CC = gcc
DEBUG ?= 1
BUILD = build/$(DEBUG)
CPPFLAGS = -Iinclude
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
ifeq ($(DEBUG),1)
CPPFLAGS += -DDEBUG
endif
.PHONY: all clean test
all: $(BUILD)/server $(BUILD)/client
$(BUILD):
	mkdir -p $@
$(BUILD)/libsysprobe.so: src/common.c include/common.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -Wl,-soname,libsysprobe.so -o $@ $<
$(BUILD)/server: src/server.c include/common.h $(BUILD)/libsysprobe.so
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< -L$(BUILD) -lsysprobe -Wl,-rpath,'$$ORIGIN'
$(BUILD)/client: src/client.c include/common.h $(BUILD)/libsysprobe.so
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< -L$(BUILD) -lsysprobe -Wl,-rpath,'$$ORIGIN'
test: all
	python3 tests/integration.py
clean:
	rm -rf build
