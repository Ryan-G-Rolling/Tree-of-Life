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
LIBS = $(RAYLIB_DIR)/lib/libraylib.a -lopengl32 -lgdi32 -lwinmm -lole32 -lshell32 -lm
endif
LIBS    ?= -lraylib -lopengl32 -lgdi32 -lwinmm -lole32 -lshell32 -lm

TARGET  := treeoflife.exe

# ---- 单文件发布（make dist）--------------------------------------------------
# 把 assets/ 里的字体、两张纹理、三个音效用 objcopy 编成 .o 链进 exe，
# 运行时从内存解码，不需要旁边的 assets/ 目录；再 -static 把 libgcc 也并进去，
# 结果只有一个依赖系统 DLL（user32/gdi32/opengl32 等）的 TreeOfLife.exe。
DIST_DIR   := dist
DIST_EXE   := $(DIST_DIR)/TreeOfLife.exe
EMB_DIR    := build/emb
EMB_OBJS   := $(EMB_DIR)/font.o $(EMB_DIR)/nebula.o $(EMB_DIR)/specimen.o \
              $(EMB_DIR)/click.o $(EMB_DIR)/buy.o $(EMB_DIR)/evolve.o
EMB_OBJCOPY = objcopy -I binary -O pe-x86-64 -B i386:x86-64
DIST_LDFLAGS = -static -static-libgcc

CORE_SRC := src/game.c src/economy.c src/save.c \
            src/evolution.c src/species.c src/environment.c src/prestige.c src/legacy.c src/buffs.c
UI_SRC   := src/main.c src/ui.c src/render.c src/screens.c src/input.c src/atmosphere.c src/tex.c \
            src/ui_adapter.c src/embedded.c src/codex.c

RULES_SRC   := src/evolution.c src/species.c src/environment.c src/prestige.c src/legacy.c src/buffs.c
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
EMB_COPY = copy /y
CDDIR    = cd /d $(subst /,\,$(1))
else
MKDIR_P  = mkdir -p $(1)
CLEAN_CMD = rm -rf build $(TARGET) shot_*.png
EMB_COPY = cp -f
CDDIR    = cd $(1)
endif

.PHONY: all clean glyphs run shot test dist report

PYTHON ?= python

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

build/test_gameplay.exe: tests/test_gameplay.c $(A_OBJ) $(B_OBJ)
	$(CC) $(TEST_FLAGS) $^ -lm -o $@

build/test_codex.exe: tests/test_codex.c src/codex.c $(HEADERS)
	@$(call MKDIR_P,build)
	$(CC) $(TEST_FLAGS) $(filter %.c,$^) -o $@

test: build/test_b.exe build/test_integration.exe build/test_regression.exe build/test_gameplay.exe build/test_codex.exe
	@echo "=== rules layer (member B) ==="
	@build/test_b.exe
	@echo "=== integration wiring (A + B) ==="
	@build/test_integration.exe
	@build/test_regression.exe
	@build/test_gameplay.exe
	@build/test_codex.exe

# Separate binaries keep instrumented objects out of the release build.
SAN_FLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -O1 -g $(INC) \
             -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
SAN_TESTS := test_b test_integration test_regression test_gameplay test_codex
build/sanitize/test_codex.exe: tests/test_codex.c src/codex.c $(HEADERS)
	@$(call MKDIR_P,build/sanitize)
	$(CC) $(SAN_FLAGS) $(filter %.c,$^) -o $@
build/sanitize/%.exe: tests/%.c $(CONTROL_SRC) $(RULES_SRC) $(HEADERS)
	@$(call MKDIR_P,build/sanitize)
	$(CC) $(SAN_FLAGS) $< $(CONTROL_SRC) $(RULES_SRC) -lm -o $@

.PHONY: test-sanitize
test-sanitize: $(SAN_TESTS:%=build/sanitize/%.exe)
	build/sanitize/test_b.exe
	build/sanitize/test_integration.exe
	build/sanitize/test_regression.exe
	build/sanitize/test_gameplay.exe
	build/sanitize/test_codex.exe

build/demo_b.exe: demo/demo_b.c $(B_OBJ)
	$(CC) $(TEST_FLAGS) $^ -lm -o $@

demo: build/demo_b.exe

# B 的 1000 种子回归与验收表（写入 tree-of-life-b/assets/）
report: build/demo_b.exe build/test_b.exe
	$(PYTHON) tools/reports/run_report.py

glyphs:
	python tools/gen_glyphs.py

run: $(TARGET)
	./$(TARGET)

shot: $(TARGET)
	./$(TARGET) --shot --ss 3

# ---- 单文件发布 --------------------------------------------------------------
# objcopy 的符号名取自**输入文件名**（带路径时斜杠也会变下划线），所以先把资源
# 拷成 build/emb/ 下的短名再转，符号才稳定是 _binary_font_ttf_start / _end。
$(EMB_DIR)/font.o: assets/fonts/ui_zh.ttf
	@$(call MKDIR_P,$(EMB_DIR))
	$(EMB_COPY) $< $(EMB_DIR)/font.ttf
	$(call CDDIR,$(EMB_DIR)) && $(EMB_OBJCOPY) font.ttf font.o

$(EMB_DIR)/nebula.o: assets/tex/nebula.png
	@$(call MKDIR_P,$(EMB_DIR))
	$(EMB_COPY) $< $(EMB_DIR)/nebula.png
	$(call CDDIR,$(EMB_DIR)) && $(EMB_OBJCOPY) nebula.png nebula.o

$(EMB_DIR)/specimen.o: assets/tex/specimen.png
	@$(call MKDIR_P,$(EMB_DIR))
	$(EMB_COPY) $< $(EMB_DIR)/specimen.png
	$(call CDDIR,$(EMB_DIR)) && $(EMB_OBJCOPY) specimen.png specimen.o

$(EMB_DIR)/click.o: assets/sfx/click.wav
	@$(call MKDIR_P,$(EMB_DIR))
	$(EMB_COPY) $< $(EMB_DIR)/click.wav
	$(call CDDIR,$(EMB_DIR)) && $(EMB_OBJCOPY) click.wav click.o

$(EMB_DIR)/buy.o: assets/sfx/buy.wav
	@$(call MKDIR_P,$(EMB_DIR))
	$(EMB_COPY) $< $(EMB_DIR)/buy.wav
	$(call CDDIR,$(EMB_DIR)) && $(EMB_OBJCOPY) buy.wav buy.o

$(EMB_DIR)/evolve.o: assets/sfx/evolve.wav
	@$(call MKDIR_P,$(EMB_DIR))
	$(EMB_COPY) $< $(EMB_DIR)/evolve.wav
	$(call CDDIR,$(EMB_DIR)) && $(EMB_OBJCOPY) evolve.wav evolve.o

# embedded.c 编两份：普通版（无宏，全部退回文件路径）与内嵌版（-DEMBED_ASSETS）。
build/emb_obj/embedded.o: src/embedded.c include/embedded.h
	@$(call MKDIR_P,build/emb_obj)
	$(CC) $(CFLAGS) -DEMBED_ASSETS -c $< -o $@

# 单文件 exe：复用普通构建的 .o，只把 embedded.o 换成内嵌版并追加资源 .o。
dist: $(OBJ) build/emb_obj/embedded.o $(EMB_OBJS)
	@$(call MKDIR_P,$(DIST_DIR))
	$(CC) $(filter-out build/src/embedded.o,$(OBJ)) build/emb_obj/embedded.o $(EMB_OBJS) \
	      -o $(DIST_EXE) $(LIBS) $(DIST_LDFLAGS)
	@echo "dist: $(DIST_EXE)"

clean:
	@$(CLEAN_CMD)

# Exercise the real action-to-UI feedback path without opening a window.
build/test_click_feedback.exe: tests/test_click_feedback.c $(filter-out build/src/main.o,$(OBJ))
	$(CC) $(TEST_FLAGS) $^ -o $@ $(LIBS)

.PHONY: test-ui
test-ui: build/test_click_feedback.exe
	build/test_click_feedback.exe

# Only reachable production layout sections are linked; no drawing backend needed.
.PHONY: test-layout
build/test_ui_layout.exe: tests/test_ui_layout.c $(CORE_SRC) src/ui.c src/ui_adapter.c src/screens.c src/codex.c $(HEADERS)
	@$(call MKDIR_P,build)
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections $(filter %.c,$^) \
	      -Wl,--gc-sections -lm -o $@
test-layout: build/test_ui_layout.exe
	build/test_ui_layout.exe

build/sanitize/test_ui_layout.exe: tests/test_ui_layout.c $(CORE_SRC) src/ui.c src/ui_adapter.c src/screens.c src/codex.c $(HEADERS)
	@$(call MKDIR_P,build/sanitize)
	$(CC) $(SAN_FLAGS) -Wno-unused-parameter -ffunction-sections -fdata-sections \
	      $(filter %.c,$^) -Wl,--gc-sections -lm -o $@

