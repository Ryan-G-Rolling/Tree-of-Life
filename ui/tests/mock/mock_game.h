/*
 * mock_game.h  ——  假的 Member A（临时）
 *
 * 签名与 01 文档 §6.1 / §6.3 完全一致，只是实现由 C 自己写，
 * 用来在没有 A 的 game.c/economy.c/save.c 时也能跑通整条链路。
 *
 * 集成时：把 tests/mock/mock_game.c 从链接里去掉，换成 A 的真实实现，
 * main.c 只需把 UI_MOCK 宏关掉。UI 代码一行都不用改。
 */
#ifndef MOCK_GAME_H
#define MOCK_GAME_H

#include "game_types.h"

void game_init(GameState *game);
void game_update(GameState *game, double dt);
ActionResult game_apply_action(GameState *game, GameAction action);
void game_start_new_run(GameState *game);

typedef enum {
    SAVE_OK,
    SAVE_FILE_ERROR,
    SAVE_BAD_VERSION,
    SAVE_INVALID_DATA
} SaveResult;

SaveResult save_write(const char *path, const GameState *game);
SaveResult save_read(const char *path, GameState *out_game);

/* 仅截图/演示用：填充一份“看起来像玩了几分钟”的状态 */
void mock_fill_demo_state(GameState *game);

#endif /* MOCK_GAME_H */
