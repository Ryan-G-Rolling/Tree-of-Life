/*
 * ui.h  ——  Member C 对外界面入口（01 文档 §6.4 约定，不可擅自改签名）
 *
 * 调用方向：main.c → ui_*；ui_* 只读 GameState，只写 pending_action。
 * 本文件及 src/ui.c、src/render.c、src/screens.c、src/input.c 是 Member C 的所有权范围。
 */
#ifndef UI_PUBLIC_H
#define UI_PUBLIC_H

#include "game_types.h"

void ui_init(void);

/* 读取输入并填充 pending_action（可为 NULL 表示本帧无动作）。
   每帧最多提交一个动作；弹层打开时吞掉底层点击。
   增量：返回 true 表示本帧产生了一个动作。原签名是 void，加返回值不影响已有调用方。 */
bool ui_update(const GameState *game, GameAction *pending_action);

/* 只读绘制。last_result 可为 NULL（首帧或动作未产生）。 */
void ui_render(const GameState *game, const ActionResult *last_result);

void ui_shutdown(void);

/* 增量补充（不改上面四个签名）：main.c 在 game_apply_action 之后调用，
   让 UI 知道“哪个动作 + 什么结果”，用于提示条与失败原因文案。 */
void ui_notify_result(const GameState *game, GameAction action, ActionResult result);

/* 增量补充：main.c 用于判断是否需要退出（目前等价于 WindowShouldClose，
   留给以后“返回主菜单”之类的扩展，不影响现有调用）。 */
int ui_should_close(void);

#endif /* UI_PUBLIC_H */
