/*
 * atmosphere.h  ——  氛围与背景层（L1 悬液 + L2 刻度 + L5 前景暗角）
 *
 * 规格：docs/art/02-atmosphere.md（v1.2）。本文件只是实现，不改任何规格。
 * 实现：src/atmosphere.c。归属：engineering-lead（ATT-01）。
 *
 * ============================ 硬约定（改代码前必读） ============================
 *
 * 1. **所有 atm_draw_* 必须在 `ui.c` 的 `BeginMode2D(zoom=SS)` 内部调用，
 *    一律使用 1280×800 逻辑像素坐标。** 本模块内部**从不**读取
 *    `ui_canvas_ss()` 参与任何布局/尺寸计算（02 §0.3-B2 / 01 §7.3-C1）——
 *    SS 只在 `atm_ensure()` 里读一次，用于分配烘焙用的 RenderTexture 尺寸。
 *
 * 2. **禁止 `BeginScissorMode` / `EndScissorMode` / 任何 rlgl 调用**
 *    （02 §0.3-B4）。SS>1 时裁剪矩形会被 Camera2D 的 zoom 再放大一次而错位。
 *    本模块的裁剪全部靠：① 角度区间（皿环）；② 线段端点（网格）；
 *    ③ 帧缓冲边界（画布外的圆自然被切掉）；④ 卡片本身是不透明的（后画即遮挡）。
 *
 * 3. **背景层零状态**（01 §1.2-P3）：删掉全部动效后截图必须一模一样。
 *    动效只允许 scale / alpha / 角度（01 §8.3）——**任何 x/y 位置变化都是 bug**。
 *
 * 4. **z 序**（02 §1.1）：
 *        ClearBackground → atm_draw_static → atm_draw_live
 *        → screens_draw（其内部 draw_stage 调一次 atm_draw_live_in）
 *        → atm_draw_vignette
 *
 * 5. `atm_draw_live()` 只给全屏页（END_ERA / LEGACY）用；主界面不要调它，
 *    否则会被不透明的展台整块盖住，白烧 12 个调用。
 *
 * 6. 锚定值（写给后来人，别"优化"回去）：UI_FONT_BASE_SIZE=120、
 *    画布 BILINEAR、SS 为整数 —— 见 ui.c / render.c 顶部契约块。
 * ============================================================================
 */
#ifndef ATMOSPHERE_H
#define ATMOSPHERE_H

#include "raylib.h"

/* 初始化。必须在 GL 上下文就绪、且 ui_font_init() 之后调用（标本编号要用已加载的
 * 字体图集）。注意：**这里不烘焙**——烘焙发生在第一次 atm_ensure()，那时 ui.c 的
 * canvas_ensure() 已经算出真实 SS；否则会先按 SS=1 白烧一张再重烧一张。 */
void atm_init(void);

/* 释放纹理。 */
void atm_shutdown(void);

/* 与 ui.c 的 canvas_ensure() 成对调用：SS 变了就按新分辨率重烧一次。
 * 窗口拉伸 / F11 全屏都会走到这里（每帧调用开销 = 一次 int 比较）。 */
void atm_ensure(void);

/* L1+L2：1 次 DrawTexturePro。画在 ClearBackground 之后、所有卡片之前。 */
void atm_draw_static(void);

/* L1 实时层（仅全屏页）：3 个呼吸细胞 + 6 粒微尘。主界面不要调。 */
void atm_draw_live(void);

/* 展台内部用：把 area 处的静态内容重绘一遍（1 次子区域 blit），再画固定坐标上的
 * 实时细胞与微尘。**area 只用于子区域取样，不参与任何落点计算、不做裁剪。**
 * 调用方必须传**未经内缩**的展台 rect（内部靠烘焙时调用同一个 ui_panel 保证恒等，
 * 一旦在外面内缩，展台圆角描边会被切掉几个像素）。
 * 必须在 draw_stage() 填完不透明的 COL_STAGE 之后、画生命体之前调用。 */
void atm_draw_live_in(Rectangle area);

/* L5：前景暗角，2 次 DrawRectangleLinesEx（最外 16 逻辑 px，2 层 × α0.020）。
 * **不是纹理 blit** —— 8bit 预乘量化与 BLEND_MULTIPLIED 两条路实测都不成立，
 * 详见 atmosphere.c 里该函数的注释。必须在 screens_draw() 之后调用。 */
void atm_draw_vignette(void);

/* 上一帧背景层的绘制调用数。**只在 debug 构建里真的计数**（02 §7.2-A3 明确要求
 * 不要为可测性在 release 热路径加计数器）；release 构建返回静态审计值 15。 */
int atm_last_call_count(void);

#endif /* ATMOSPHERE_H */
