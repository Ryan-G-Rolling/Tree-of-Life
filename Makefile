# Tree of Life · 最终集成版构建
#
# 三方合一：
#   A 控制层      src/game.c economy.c save.c
#   B 规则层      src/evolution.c species.c environment.c prestige.c legacy.c
#   C 界面层      src/main.c ui.c render.c screens.c input.c atmosphere.c ui_adapter.c
#
# 与单独开发版的差别：不再有 tests/mock 或 rules_stub，UI 直接查询真实 A/B。
#
#     mingw32-make            # 构建 treeoflife.exe
#     ./treeoflife.exe        # 必须在工程根目录运行（assets/ 是相对路径）
#     ./treeoflife.exe --shot --ss 3   # 4K 截图
#
CC      = gcc
STD     ?= -std=c11
WARN    ?= -Wall -Wextra -Wno-unused-parameter
OPT     ?= -O2
RAYLIB_DIR ?= .local/deps/raylib-5.5_win64_mingw-w64
INC     ?= -Iinclude -Idata
ifneq ($(wildcard $(RAYLIB_DIR)/include/raylib.h),)
INC += -I$(RAYLIB_DIR)/include
LIBS = $(RAYLIB_DIR)/lib/libraylib.a -lopengl32 -lgdi32 -lwinmm -lm
endif
LIBS    ?= -lraylib -lopengl32 -lgdi32 -lwinmm -lm

TARGET  := treeoflife.exe

CORE_SRC := src/game.c src/economy.c src/save.c \
            src/evolution.c src/species.c src/environment.c src/prestige.c src/legacy.c
UI_SRC   := src/main.c src/ui.c src/render.c src/screens.c src/input.c src/atmosphere.c src/tex.c \
            src/ui_adapter.c

RULES_SRC   := src/evolution.c src/species.c src/environment.c src/prestige.c src/legacy.c
CONTROL_SRC := src/game.c src/economy.c src/save.c
B_OBJ       := $(RULES_SRC:%.c=build/%.o)
A_OBJ       := $(CONTROL_SRC:%.c=build/%.o)

SRC     := $(UI_SRC) $(CORE_SRC)
OBJ     := $(SRC:%.c=build/%.o)

CFLAGS      := $(STD) $(WARN) $(OPT) $(INC)
# 测试用最严的告警级别（与 B 的原始验收标准一致）
TEST_FLAGS  := -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 $(INC)

# Windows 下没有 sh.exe，`mkdir -p` / `rm -rf` 会直接失败；按平台切换命令
ifeq ($(OS),Windows_NT)
MKDIR_P  = if not exist "$(subst /,\,$(1))" mkdir "$(subst /,\,$(1))"
CLEAN_CMD = if exist build rmdir /s /q build & if exist $(TARGET) del /q $(TARGET) & del /q shot_*.png 2>nul & exit /b 0
else
MKDIR_P  = mkdir -p $(1)
CLEAN_CMD = rm -rf build $(TARGET) shot_*.png
endif

.PHONY: all clean glyphs run shot test

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LIBS)

HEADERS := $(wildcard include/*.h data/*.h) src/game_internal.h

build/%.o: %.c $(HEADERS)
	@$(call MKDIR_P,$(dir $@))
	$(CC) $(CFLAGS) -c $< -o $@

# B 的规则层自测：21 组 / 36k+ 断言，只链接规则模块，不含 raylib
build/test_b.exe: tests/test_b.c $(B_OBJ)
	$(CC) $(TEST_FLAGS) $^ -lm -o $@

# 三方接线冒烟：A 的动作入口是否真的驱动了 B 的规则与存档
build/test_integration.exe: tests/test_integration.c $(A_OBJ) $(B_OBJ)
	$(CC) $(TEST_FLAGS) $^ -lm -o $@

build/test_regression.exe: tests/test_regression.c $(A_OBJ) $(B_OBJ)
	$(CC) $(TEST_FLAGS) $^ -lm -o $@

test: build/test_b.exe build/test_integration.exe build/test_regression.exe
	@echo "=== rules layer (member B) ==="
	@build/test_b.exe
	@echo "=== integration wiring (A + B) ==="
	@build/test_integration.exe
	@build/test_regression.exe

build/demo_b.exe: demo/demo_b.c $(B_OBJ)
	$(CC) $(TEST_FLAGS) $^ -lm -o $@

demo: build/demo_b.exe

glyphs:
	python tools/gen_glyphs.py

run: $(TARGET)
	./$(TARGET)

shot: $(TARGET)
	./$(TARGET) --shot --ss 3

clean:
	@$(CLEAN_CMD)

# Exercise the real action-to-UI feedback path without opening a window.
build/test_click_feedback.exe: tests/test_click_feedback.c $(filter-out build/src/main.o,$(OBJ))
	$(CC) $(TEST_FLAGS) $^ -o $@ $(LIBS)

.PHONY: test-ui
test-ui: build/test_click_feedback.exe
	build/test_click_feedback.exe
