# Tree of Life · 构建脚本
#
# 默认用测试替身编译（MOCK=1），这样 Member C 的界面可以在没有 A/B 代码时独立运行：
#     make                 # 出 treeoflife.exe（含 tests/mock 假实现）
#     ./treeoflife.exe     # 必须在项目根目录运行，因为 assets/ 是相对路径
#
# 集成阶段把 A/B 的真实 .c 加进来并关掉替身：
#     make MOCK=0 EXTRA_SRC="src/game.c src/economy.c src/save.c src/evolution.c src/species.c src/environment.c src/prestige.c src/legacy.c"
#
# 截图自检（不需要人手点）：
#     ./treeoflife.exe --shot

CC      ?= gcc
STD     ?= -std=c11
WARN    ?= -Wall -Wextra -Wno-unused-parameter
OPT     ?= -O2
INC     ?= -Iinclude
LIBS    ?= -lraylib -lopengl32 -lgdi32 -lwinmm

MOCK    ?= 1
TARGET  ?= treeoflife.exe

UI_SRC  := src/main.c src/ui.c src/render.c src/screens.c src/input.c src/atmosphere.c

ifeq ($(MOCK),1)
MOCK_SRC := tests/mock/mock_game.c tests/mock/mock_tables.c tests/mock/mock_adapter.c tests/mock/selftest.c
DEFINES  := -DUI_MOCK -Itests/mock
else
MOCK_SRC :=
DEFINES  :=
endif

SRC := $(UI_SRC) $(MOCK_SRC) $(EXTRA_SRC)
OBJ := $(SRC:%.c=build/%.o)

CFLAGS  := $(STD) $(WARN) $(OPT) $(INC) $(DEFINES)

.PHONY: all clean glyphs sfx run shot test

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LIBS)

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

glyphs:
	python tools/gen_glyphs.py

run: $(TARGET)
	./$(TARGET)

shot: $(TARGET)
	./$(TARGET) --shot

test: $(TARGET)
	./$(TARGET) --selftest

clean:
	rm -rf build $(TARGET) shot_*.png
