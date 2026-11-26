# Tree of Life · Member B 构建脚本（本文件只负责 B 的模块自测，不链接 raylib）

CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
INCLUDE := -Iinclude -Idata
BUILD   := build

B_SRC := src/evolution.c src/species.c src/environment.c src/prestige.c src/legacy.c
B_OBJ := $(B_SRC:src/%.c=$(BUILD)/%.o)

.PHONY: all test clean

all: $(BUILD)/test_b $(BUILD)/demo_b

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

$(BUILD)/test_b: tests/test_b.c $(B_OBJ) | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDE) $^ -lm -o $@

$(BUILD)/demo_b: demo/demo_b.c $(B_OBJ) | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDE) $^ -lm -o $@

test: $(BUILD)/test_b
	@$(BUILD)/test_b

demo: $(BUILD)/demo_b
	@$(BUILD)/demo_b

clean:
	@rm -rf $(BUILD)
