#ifndef BUFFS_H
#define BUFFS_H

/*
 * Member B · buffs.h
 * 随机增益因子：页面上随机刷新的可点击细胞 / 发光 DNA。
 *
 * 规则只有这一份实现：
 *   - 每 45–90 s 刷新一个因子，最多同时存在 1 个；
 *   - 点击后获得限时增益或一次性资源包；
 *   - 稀有（强）增益需要 150 s 冷却。
 * C 侧只读取 buffs_factor_visible / buff_visible_* 与查询函数做渲染。
 */

#include "game_types.h"

/* 新纪元开始时调用：清空增益状态并安排第一次刷新 */
void buffs_init_run(GameState *game);

/* 固定步内推进：刷新因子、倒计时已激活的限时增益 */
void buffs_update(GameState *game, double dt);

/* 点击当前因子；没有因子时返回 ACTION_INVALID_TARGET */
ActionCode buffs_collect(GameState *game);

/* 当前限时增益的倍率；无增益时为 1.0 */
double buffs_production_multiplier(const GameState *game);
double buffs_click_multiplier(const GameState *game);

/* 渲染辅助：当前是否有因子、名称文本 */
bool buffs_factor_visible(const GameState *game);
const char *buffs_type_name(int buff_type);
const char *buffs_icon_name(int icon);

#endif
