/*
 * tex.h  ——  摄影纹理层（Member C）
 *
 * 界面原本 100% 由程序化色块构成，暖色版看起来会有点"平"。这一层给整张
 * 背景叠一张真实的显微标本照片：不是装饰性插图，而是让底色带上真实的
 * 细胞质感 —— 透过树脂看标本的那种颗粒与层次。
 *
 * 使用约定：
 *   - 只在 `atm_draw_static()` 之后、任何面板之前画一次（全屏 1280×800）。
 *     面板不透明，纹理只从卡片缝隙和全屏页透出来；它永远不覆盖文字。
 *   - 颜色由 tint 决定：RGB 是乘算（压暗 + 定色相），A 是混合强度。
 *   - 缺文件时静默跳过，界面仍能正常跑（不用纹理也能交付）。
 */
#ifndef TEX_H
#define TEX_H

#include "raylib.h"

/* 是否有可用纹理（懒加载，第一次调用时尝试读 assets/tex/specimen.png）。 */
bool tex_ready(void);

/* 把纹理铺到 dst。未加载成功时什么都不画。 */
void tex_draw(Rectangle dst, Color tint);

/* 释放显存（ui_shutdown 调用）。 */
void tex_shutdown(void);

#endif /* TEX_H */
