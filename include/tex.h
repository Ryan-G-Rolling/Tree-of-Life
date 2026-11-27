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

/* ---------- 生物插图层（第 17 版） ----------
 * assets/tex/morph/ 下的 19 张透明底科学插画，按形态族取图：
 * 有图就用图（更"真"的标本质感），缺图返回 false，调用方回退程序化矢量形态。
 * 懒加载：第一次用到某个形态族时才读盘，读不到就永久回退。 */
typedef enum {
    TEXM_COCCUS = 0,
    TEXM_ROD,
    TEXM_EUKARYOTE,
    TEXM_COLONY,
    TEXM_TISSUE,
    TEXM_SYMBIO,
    TEXM_NEURAL,
    TEXM_TECHNO,
    TEXM_DIATOM,
    TEXM_CILIATE,
    TEXM_AMOEBA,
    TEXM_FILAMENT,
    TEXM_SPIRAL,
    TEXM_FISH,
    TEXM_JELLY,
    TEXM_FERN,
    TEXM_FUNGUS,
    TEXM_CORAL,
    TEXM_HIVE,
    TEXM_COUNT
} TexMorph;

/* 是否有该形态族的插图（懒加载一次）。 */
bool tex_morph_ready(int morph);

/* 把插图铺进 dst（等比拉伸，原文透明底）。没图返回 false。 */
bool tex_morph_draw(int morph, Rectangle dst, Color tint);

#endif /* TEX_H */
