CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude
LDLIBS = -lm
BUILD = build
CORE = src/game.c src/economy.c src/save.c tests/rules_stub.c
HEADERS = $(wildcard include/*.h data/*.h tests/*.h) src/game_internal.h
ifeq ($(OS),Windows_NT)
EXE = .exe
else
EXE =
endif
TESTS = $(BUILD)/test_member_a$(EXE) $(BUILD)/test_game_boundaries$(EXE) $(BUILD)/test_save_validation$(EXE)

.PHONY: all test clean headers
all: $(TESTS)

$(BUILD):
ifeq ($(OS),Windows_NT)
	if not exist "$(BUILD)" mkdir "$(BUILD)"
else
	mkdir -p $(BUILD)
endif

$(BUILD)/test_%$(EXE): tests/test_%.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDLIBS)

$(BUILD)/header_smoke$(EXE): tests/header_smoke.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

headers: $(BUILD)/header_smoke$(EXE)
	$(BUILD)/header_smoke$(EXE)

test: $(TESTS)
	$(BUILD)/test_member_a$(EXE)
	$(BUILD)/test_game_boundaries$(EXE)
	$(BUILD)/test_save_validation$(EXE)

clean:
	cmd /c if exist $(BUILD) rmdir /s /q $(BUILD)
