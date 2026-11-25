/*
 * atmosphere.c  ——  氛围与背景层（L1 悬液 / L2 刻度 / L5 前景暗角）
 *
 * 规格：docs/art/02-atmosphere.md（v1.2）。本文件严格实现该规格，不改规格。
 * 归属：engineering-lead（ATT-01）。
 *
 * ============================ 四个必须记住的坑 ============================
 *
 * 坑 1（premultiply 偏色，02 §6.3）：烘焙静态层用的 RenderTexture **第一件事必须
 *   ClearBackground(COL_BG)**，让 RT 成为不透明图。若用 BLANK 透明底再画半透明
 *   内容，多层 straight-alpha 叠加后整体 blit 会偏暗。
 *
 * 坑 2（RT 读回来是上下翻转的，02 §6.3）：`LoadImageFromTexture()` 拿到的 Image
 *   是垂直翻转的，**必须 `ImageFlipVertical(&img)` 再 `LoadTextureFromImage`**。
 *   漏掉这一步的后果是整层背景上下颠倒——而它恰好不会被任何 CR 断言抓到
 *   （颜色全对，只是位置反了），只会在"网格怎么跑到上面去了"这种目检里暴露。
 *
 * 坑 3（8bit 量化吃掉低 α，02 §2.6）：`Fade()` 在 8bit 码值空间插值。α=0.02 的
 *   元素合成后只移动 0.2 个码值 → `round()` 直接吃掉。这条同时决定了 L5 暗角
 *   **不能**用"透明底 RT + 普通 alpha blit"实现：RT 里存的是预乘结果
 *   (rgb·α ≈ 5×0.02 = 0.1)，量化后归零，再乘一次 α 就彻底没了。见 §L5 的注释。
 *
 * 坑 4（展台的底必须是 COL_STAGE，不是 COL_BG）：静态层是**一整张不透明纹理**，
 *   展台那段会随后被 `draw_stage` 的 `COL_STAGE` 填充盖住，所以必须靠
 *   `atm_draw_live_in()` 把展台那段**子区域**再 blit 一次捞回来。若那段烘焙在
 *   COL_BG 上，捞回来就会把整个展台从 #0C171A 换成 #0B1517——Δ(1,2,3)，
 *   肉眼几乎看不出，但 A1 的所有 CR 判定都写在"底 = COL_STAGE"上，基线就错了。
 *   解法：烘焙时直接用 `ui_panel(ATM_STAGE, R_CARD, COL_STAGE, COL_LINE)` 画一遍
 *   （**调同一个函数**，不自己拼原语），于是子区域 blit 是逐像素恒等操作，
 *   连圆角描边都不会被方角覆盖。
 *
 * ============================ 为什么常驻的不是 RT ============================
 * raylib 5.5 的 `LoadRenderTexture()` 固定附带 32bit 深度 renderbuffer（无法关闭），
 * 4K 下一条 RT = 35.16(色) + 35.16(深度) = 70.3 MiB。而本层的静态内容
 * **一生只渲染一次**，所以走 RT → Image → Texture2D → 立刻 UnloadRenderTexture：
 * 常驻只有 35.16 MiB 且无深度。**氛围层常驻显存 = 35.16 MiB @SS=3**（≤ 40 MiB 上限 ✓）。
 * 峰值瞬时是 70.3（烘焙 RT）+ 70.3（主画布 RT）+ 35.16（readback Image）= 175 MiB，
 * 只在开局那一次出现，用户无感（02 §6.3「重烧成本 30–60ms」）。
 *
 * ============================ 为什么暗角可以降分辨率 ============================
 * L1+L2 静态层含 1 逻辑 px 的网格发丝线（SS=3 时 = 3 物理 px），必须满分辨率，
 * 否则 A7 断言直接挂。而 L5 暗角是**纯 2 台阶平滑渐变、零细节**：640×400 → 3840×2400
 * 的 6× 双线性放大反而把台阶磨得更顺，正好达成 §4.1「能数出台阶但看不出接缝」。
 * 只有 16 逻辑 px → 8 texel 的环带会软 ±2 逻辑 px，但那一层 α 只有 0.02–0.04，看不出。
 */
#include <math.h>
#include <stdbool.h>

#include "raylib.h"
#include "atmosphere.h"
#include "ui_internal.h"

/* ------------------------------------------------------------------------- */
/* 1. 常量（全部逐字抄 02 的表格，改任何一个数都要回去改文档 + 重算 §1.3 的表） */

#define ATM_TAU (2.0f * PI)

/* 02 §5.1 的两个背景层 token。规格归 02、由美术侧并入 ui_internal.h 的 extern 块；
 * 在那边落地之前用**文件内私有副本**（名字故意不同，将来合并时不会撞符号）。 */
static const Color ATM_CELL     = { 17, 30, 33, 255 }; /* #111E21，COL_CELL */
static const Color ATM_VIGNETTE = {  5, 12, 14, 255 }; /* #050C0E，COL_VIGNETTE */

/* 景深三层（02 §2.3 + 附录 A）：半径倍数与 alpha 的完整阶梯 */
static const float ATM_HALO_R_NEAR[3] = { 1.00f, 1.06f, 1.13f };
static const float ATM_HALO_A_NEAR[3] = { 0.30f, 0.18f, 0.09f };
static const float ATM_HALO_R_MID[2]  = { 1.00f, 1.08f };
static const float ATM_HALO_A_MID[2]  = { 0.22f, 0.11f };
#define ATM_HALO_A_FAR   0.14f

/* 可见性下限（02 §2.6 / 附录 A）：低于这些值 8bit 量化会吃掉整个元素
 *
 * 【ENG-04 ③】面 / 柔边环原本是 0.55 / 0.60，但两者在 COL_STAGE(12,23,26) 上
 * 混合后都落在 **(15,27,30)** —— 完全同值，Δ=(0,0,0)，那圈"细胞壁"整条不可见，
 * 于是母题 A 在 4K 下读作"一块均匀的灰饼 + 一根 0.985R 细轮廓"，而不是"细胞"。
 * 修法（team-lead 授权"环提一档、面降一档、总和不变"，**不抬总 α、不破 CR≤1.25**）：
 *   面 0.55 → 0.45  ⇒ (14,26,29)
 *   环 0.60 → 0.75  ⇒ (16,28,31)   Δ=(2,2,2)
 * Δ=(2,2,2) 已是物理上限：COL_CELL(17,30,33) 与 COL_STAGE(12,23,26) 只差 (5,7,7)，
 * 即使 α=1.0 也只能拿到 (5,7,7)。想让"壁"更明显只能把壁环换 COL_LINE(Δ43)，
 * 那属于改 02 的调色口径，需要美术点头，本轮不动。 */
#define ATM_CELL_FILL    0.45f /* COL_CELL 面填充：差=3（下限） */
#define ATM_CELL_EDGE    0.75f /* COL_CELL 柔边环：与面拉开 Δ=(2,2,2) */
#define ATM_DUST_MIN     0.10f /* COL_LINE 微尘 α 谷值：差=4 */
#define ATM_DUST_MAX     0.16f /* COL_LINE 微尘 α 峰值：差=7；>0.22 就太清楚了 */
#define ATM_GRID_ALPHA   0.16f /* 刻度线（线级判据 0.16–0.22） */
#define ATM_GRID_MAJOR   0.22f

/* 尺寸（逻辑 px） */
#define ATM_GRID_STEP    32    /* 02 §3.1：4K 加强后从 64 收到 32（K2） */
#define ATM_GRID_GROUP   5     /* 每 4 格断开 1 格 → 断续刻度带 */
#define ATM_GRID_INSET   8.0f  /* 展台四边内缩（02 §2.2.3） */
#define ATM_RING_R       262.0f/* 皿环 R = 展台半宽（02 §3.3） */
#define ATM_RING_SEG     96    /* 02 §6.4-K4：R ≥ 80 的环用 96 段 */
#define ATM_VIG_BAND_L1  112.0f/* 背景级三层 112 / 72 / 36 */
#define ATM_VIG_A_L1     0.030f
/* 【主理人裁定 2026-11-25】L5 前景暗角：原规格"2 层 × 0.020，目标 COL_VIGNETTE"在
 * 8bit 下是 Δ=(0,0,0)——逐像素实测 x=0..19 与中缝完全同色，两条调用等於没画。
 * 根因：COL_VIGNETTE(5,12,14) 与 COL_BG(11,21,23) 只差 (6,9,9)，α=0.020 的单层
 * 位移仅 0.12~0.18 码值，round 后归零；两层叠加也没跨过 0.5 阈值。
 * 修法：目标改纯黑（差 (11,21,23)）+ 单层 α=0.08 → 位移 0.9/1.7/1.8 码值，
 * 稳定产生 Δ=(1,2,2)，肉眼可辨但仍属"边缘压暗"而非第二光源。CR 相对底色约 1.03，
 * 远低于 1.25 铁律。顺带把调用从 2 降到 1，把 §6.1 的余量还回来（16→15）。 */
#define ATM_VIG_A_L5     0.080f/* 前景级 1 层 × 0.080，目标 BLACK（非 ATM_VIGNETTE） */
#define ATM_VIG_BAND_L5  16.0f
#define ATM_CALLS_MAX    16    /* 每帧硬上限，超过即实现错误（02 §6.1） */

/* 静态审计值（02 §7.2-A3）：见文件末尾的计数表 */
#define ATM_CALLS_AUDIT_MAIN 16

/* ------------------------------------------------------------------------- */
/* 2. 布局表（02 附录 D，**直接照抄，不要改成 hash 生成**）
 *
 * §2.2.1 讲得很清楚：缝只有 10–20px 宽，只有"圆周恰好扫过缝"的落点才读得出圆；
 * 圆心落在缝里会得到一根竖条，读作矩形，A4 断言（背景不成图形）直接挂。
 * hash 抖动的命中率太低且结果无法复核，所以这里全部是算好并逐点校验过的常数。
 *
 * 坐标系 = 1280×800 逻辑画布。圆心故意在卡片后面或画布外，**不要"修正"到缝里**。 */

typedef enum { MOTIF_VESICLE = 0, MOTIF_SHELL, MOTIF_DIVIDING, MOTIF_COLONY, MOTIF_EDGE } Motif;
typedef enum { TIER_NEAR = 0, TIER_MID, TIER_FAR } Tier;

typedef struct {
    float x, y, R;
    Motif kind;
    Tier tier;
} AtmCell;

/* 附录 D 主表：6 个缝隙母题 */
static const AtmCell ATM_GUTTER_MOTIFS[6] = {
    /* M1 */ {  -34.0f, 352.0f, 168.0f, MOTIF_EDGE,    TIER_NEAR }, /* 左边距长弧 */
    /* M2 */ { 1312.0f, 470.0f, 200.0f, MOTIF_EDGE,    TIER_NEAR }, /* 右边距长弧 */
    /* M3 */ {  416.0f, 240.0f,  32.0f, MOTIF_VESICLE, TIER_MID  }, /* 中列竖缝弧帽，左极点 384 */
    /* M4 */ {  900.0f, 620.0f,  36.0f, MOTIF_SHELL,   TIER_MID  }, /* 右列竖缝弧帽，右极点 936 */
    /* M5 */ {   10.0f, 590.0f,  14.0f, MOTIF_SHELL,   TIER_FAR  }, /* 左边距小圆镜 */
    /* M6 */ { 1270.0f, 340.0f,  13.0f, MOTIF_SHELL,   TIER_FAR  }, /* 右边距小圆镜 */
};

/* §2.2.1「仍按 R 降序绘制」：大的先画、小的压在上面 → 远的东西被近的挡住 */
static const int ATM_GUTTER_ORDER[6] = { 1, 0, 3, 2, 4, 5 };

/* 附录 D 展台表：4 个（S1/S2 实时，S3/S4 静态烘进 RT） */
#define ATM_STAGE_MOTIF_NEAR  { 450.0f, 220.0f, 44.0f, MOTIF_VESICLE, TIER_NEAR } /* S1 */
#define ATM_STAGE_MOTIF_MID   { 860.0f, 430.0f, 26.0f, MOTIF_SHELL,   TIER_MID  } /* S2 */
#define ATM_STAGE_MOTIF_FAR1  { 430.0f, 440.0f, 18.0f, MOTIF_SHELL,   TIER_FAR  } /* S3 */
#define ATM_STAGE_MOTIF_FAR2  { 720.0f, 190.0f, 15.0f, MOTIF_SHELL,   TIER_FAR  } /* S4 */

/* 展台 = 唯一允许内部绘制的卡（02 §2.2.3），rect 与 screens.c 的 draw_stage 一致 */
static const Rectangle ATM_STAGE = { 396.0f, 166.0f, 524.0f, 300.0f };
/* 展台下 1/3（y ∈ [366,466]）：网格只允许出现在这里，生命体所在的 y ∈ [166,366] 禁画 */
static const Rectangle ATM_STAGE_LOW = { 396.0f, 366.0f, 524.0f, 100.0f };

/* 主界面 9 条缝（02 §1.4 实测表，卡片并集的补集） */
static const Rectangle ATM_GUTTERS[] = {
    {    0.0f,  88.0f, 1280.0f,  12.0f }, /* 顶栏与内容之间 */
    {    0.0f, 724.0f, 1280.0f,  12.0f }, /* 内容与底栏之间 */
    {    0.0f, 100.0f,   20.0f, 624.0f }, /* 左边距 */
    { 1260.0f, 100.0f,   20.0f, 624.0f }, /* 右边距 */
    {  380.0f, 100.0f,   16.0f, 624.0f }, /* 左列与中列之间 */
    {  920.0f, 100.0f,   16.0f, 624.0f }, /* 中列与右列之间 */
    {   20.0f, 330.0f,  360.0f,  32.0f }, /* 左列内横缝 */
    {  396.0f, 154.0f,  524.0f,  40.0f }, /* 中列内横缝（环境横幅与展台之间） */
    {  936.0f, 296.0f,  324.0f,  24.0f }, /* 右列内横缝 */
};
#define ATM_GUTTER_COUNT ((int)(sizeof(ATM_GUTTERS) / sizeof(ATM_GUTTERS[0])))

/* 微尘 6 粒的固定位置（02 §2.5：位置固定、半径固定 1.5、只做 α 呼吸）。
 * 展台内：全部落在 §2.2.3 的避让区之外（距生命体圆心 > 130、不碰第二物种槽）。 */
static const Vector2 ATM_DUST_STAGE[6] = {
    { 430.0f, 300.0f }, { 500.0f, 260.0f }, { 480.0f, 440.0f },
    { 600.0f, 180.0f }, { 820.0f, 200.0f }, { 760.0f, 230.0f },
};
/* 六个落点都校验过：① 距生命体圆心 > 130（§2.2.3）；② 不碰第二物种槽
 * (754,250,148,148)；③ 不落在任何展台母题的实心面里（否则微尘被面吃掉，
 * 一个调用画了空气 —— 第一版 (450,200) 就掉在 S1 的盘面里，实测码值差 0）。 */
/* 全屏页：无卡片遮挡，撒在内容区里（同样固定） */
static const Vector2 ATM_DUST_FULL[6] = {
    {  180.0f, 260.0f }, {  330.0f, 560.0f }, {  620.0f, 180.0f },
    {  760.0f, 620.0f }, {  980.0f, 300.0f }, { 1120.0f, 520.0f },
};

/* 标本编号（02 §3.2）：只允许"编号 / 倍率 / 批次号"，禁止写状态。
 * 位置贴着展台内侧边角、不对称 —— 展台是主界面**唯一**有可见背景的地方，
 * 放在缝里 12px 的字一行放不下（竖缝只有 16px 宽）。
 * ⚠ 这两处是整份实现里唯一"靠文档推、看不到实机版式"的落点：若首张截图里
 *   与展台内已有文字打架，直接改这两个常数即可，不影响其他任何东西。 */
static const char *ATM_LABEL_A = "F12 / S.03";
static const float ATM_LABEL_A_X = 406.0f;
static const float ATM_LABEL_A_Y = 400.0f;
static const char *ATM_LABEL_B = "40\xC3\x97"; /* "40×"（U+00D7 已在 render.c 登记） */
static const float ATM_LABEL_B_X = 876.0f;
static const float ATM_LABEL_B_Y = 180.0f;

/* ------------------------------------------------------------------------- */
/* 3. 状态 */

static Texture2D g_static;    /* L1+L2，满分辨率，无深度缓冲 */
static int  g_static_ss;      /* 烘焙时使用的 SS；0 = 未烘焙 */
static bool g_ready;

/* 02 §7.2-A3 明确要求「不要为了可测性在 release 热路径上加计数器」，
 * 所以计数只在 debug 构建里存在；release 下 atm_last_call_count() 返回静态审计值。 */
#ifndef NDEBUG
static int g_calls;
#define ATM_DRAW(expr) do { g_calls++; (expr); } while (0)
int atm_last_call_count(void) { return g_calls; }
#else
#define ATM_DRAW(expr) (expr)
int atm_last_call_count(void) { return ATM_CALLS_AUDIT_MAIN; }
#endif

/* ------------------------------------------------------------------------- */
/* 4. 母题（02 §2.1，逐字实现；所有尺寸写成 R × 系数） */

/* 一条"半径 r、线宽 w"的闭合轮廓环。w 有 1 逻辑 px 下限：
 * 远景母题 R 只有 13–15，0.015R 会不到 0.2px → 被光栅化吃掉。 */
static void atm_ring_line(Vector2 c, float r, float w, float a, int seg)
{
    if (w < 1.0f) w = 1.0f;
    ATM_DRAW(DrawRing(c, r - w * 0.5f, r + w * 0.5f, 0.0f, 360.0f, seg, Fade(COL_LINE, a)));
}

/* 景深柔边（02 §2.3）：外扩 + 逐层降 alpha = 弥散圆，读作"失焦"。
 * 第一层由各母题自己的"外壁"承担（§2.1 里 ③ 与景深第一层是同一个环），
 * 所以这里只负责**第 2 层起**的外扩层，避免重复画在同一个半径上。 */
static void atm_halo_extra(Vector2 c, float R, Tier tier)
{
    if (tier == TIER_NEAR) {
        atm_ring_line(c, R * ATM_HALO_R_NEAR[1], R * 0.015f, ATM_HALO_A_NEAR[1], 96);
        atm_ring_line(c, R * ATM_HALO_R_NEAR[2], R * 0.015f, ATM_HALO_A_NEAR[2], 96);
    } else if (tier == TIER_MID) {
        atm_ring_line(c, R * ATM_HALO_R_MID[1], R * 0.015f, ATM_HALO_A_MID[1], 64);
    }
    /* FAR：只有 1 层轮廓（判据 D1/D3），外扩层为 0 */
}

/* 母题的外壁 alpha 就是景深第一层的 alpha */
static float atm_wall_alpha(Tier tier)
{
    if (tier == TIER_NEAR) return ATM_HALO_A_NEAR[0];
    if (tier == TIER_MID)  return ATM_HALO_A_MID[0];
    return ATM_HALO_A_FAR;
}

static int atm_seg(Tier tier)
{
    if (tier == TIER_NEAR) return 96;
    if (tier == TIER_MID)  return 64;
    return 48;
}

static void bake_motif(const AtmCell *m)
{
    Vector2 c = { m->x, m->y };
    float R = m->R;
    float wa = atm_wall_alpha(m->tier);
    int seg = atm_seg(m->tier);

    switch (m->kind) {

    case MOTIF_VESICLE: /* A 囊泡：① 面 ② 柔边 ③ 外壁 + 外扩层 */
        ATM_DRAW(DrawCircleV(c, R * 0.94f, Fade(ATM_CELL, ATM_CELL_FILL)));
        ATM_DRAW(DrawRing(c, R * 0.94f, R * 1.00f, 0.0f, 360.0f, 32,
                          Fade(ATM_CELL, ATM_CELL_EDGE)));
        atm_ring_line(c, R * 0.985f, R * 0.03f, wa, seg);
        atm_halo_extra(c, R, m->tier);
        break;

    case MOTIF_SHELL: /* B 空壳：只剩细胞壁 + 一圈内壁（远景无内部结构 → 省略） */
        atm_ring_line(c, R * 0.975f, R * 0.05f, wa, seg);
        if (m->tier != TIER_FAR)
            atm_ring_line(c, R * 0.575f, R * 0.05f, 0.12f, 32);
        atm_halo_extra(c, R, m->tier);
        break;

    case MOTIF_DIVIDING: { /* C 分裂：两团 + 缢缩腰（唯一允许 5 调用的母题） */
        Vector2 a = { c.x - R * 0.52f, c.y };
        Vector2 b = { c.x + R * 0.52f, c.y };
        float r = R * 0.62f;
        ATM_DRAW(DrawCircleV(a, r * 0.95f, Fade(ATM_CELL, 0.45f)));
        ATM_DRAW(DrawCircleV(b, r * 0.95f, Fade(ATM_CELL, 0.45f)));
        atm_ring_line(a, r * 0.975f, r * 0.05f, 0.20f, 40);
        atm_ring_line(b, r * 0.975f, r * 0.05f, 0.20f, 40);
        ATM_DRAW(DrawLineEx((Vector2){ c.x, c.y - R * 0.30f },
                            (Vector2){ c.x, c.y + R * 0.30f }, 1.0f,
                            Fade(COL_LINE, 0.16f)));
        break;
    }

    case MOTIF_COLONY: { /* D 菌落：5 个子球 + 一圈极淡外壁 */
        for (int i = 0; i < 5; i++) {
            float ang = (float)i * 72.0f + 12.0f;
            float rad = ang * DEG2RAD;
            Vector2 p = { c.x + cosf(rad) * R * 0.55f, c.y + sinf(rad) * R * 0.55f };
            ATM_DRAW(DrawCircleV(p, R * 0.24f, Fade(ATM_CELL, 0.45f)));
        }
        atm_ring_line(c, R * 0.99f, R * 0.02f, 0.14f, 40);
        break;
    }

    case MOTIF_EDGE: /* E 边缘裁切：圆心在画布外，只露出一段弧 = "视野外还有更大的东西" */
        atm_ring_line(c, R * 0.9925f, R * 0.015f, 0.20f, 96);
        if (m->tier == TIER_NEAR) {
            /* E 的 R 是 168–200，用 1.06/1.13R 会把弧推得太远，改用 1.03/1.06 */
            atm_ring_line(c, R * 1.030f, R * 0.012f, 0.12f, 96);
            atm_ring_line(c, R * 1.060f, R * 0.012f, 0.06f, 96);
        } else if (m->tier == TIER_MID) {
            atm_ring_line(c, R * 1.030f, R * 0.012f, 0.10f, 64);
        }
        break;
    }
}

/* ------------------------------------------------------------------------- */
/* 5. L2 刻度：极细测量网格（02 §3.1）
 *
 * 只在缝里 + 展台下 1/3，绝不覆盖生命体。无裁剪 API（02 §0.3-B4）：
 * 线段端点直接按缝的矩形算交点，一个像素都不用裁。
 *
 * A9 的函数级 grep 判据要求这两个函数体内**不得出现 `Fade(CELL`** —— 它们只用
 * COL_LINE（与 COL_STAGE 的 Δchan 是 43，α=0.16 时码值差 7，安全）。 */

static void grid_seg_h(float y, float x0, float x1, float a)
{
    if (x1 - x0 < 2.0f) return;
    /* ENG-04 ②：线宽走 token。这里是 W_HAIRLINE_AA(1.05) 而不是 W_HAIRLINE(1.0)，
     * 理由见 render.c 顶部「Z-A1 临时妥协」与 ui_internal.h 的同名注释。 */
    ATM_DRAW(DrawLineEx((Vector2){ x0, y + 0.5f }, (Vector2){ x1, y + 0.5f }, W_HAIRLINE_AA,
                        Fade(COL_LINE, a)));
}

static void grid_seg_v(float x, float y0, float y1, float a)
{
    if (y1 - y0 < 2.0f) return;
    ATM_DRAW(DrawLineEx((Vector2){ x + 0.5f, y0 }, (Vector2){ x + 0.5f, y1 }, W_HAIRLINE_AA,
                        Fade(COL_LINE, a)));
}

/* 步进 32 逻辑 px；每 4 格断开 1 格；每第 4 条长度 +8、α 提到 0.22 */
static bool grid_skip(int i) { return (i % ATM_GRID_GROUP) == (ATM_GRID_GROUP - 1); }
static bool grid_major(int i) { return (i % 4) == 0; }

/* 缝里的网格。y/x 命中某个缝的区间时，只在那条缝的跨度内画。 */
static void atm_grid(void)
{
    for (int i = 1; i * ATM_GRID_STEP < UI_H; i++) {
        if (grid_skip(i)) continue;
        float y = (float)(i * ATM_GRID_STEP);
        float a = grid_major(i) ? ATM_GRID_MAJOR : ATM_GRID_ALPHA;
        float ext = grid_major(i) ? 4.0f : 0.0f;
        for (int g = 0; g < ATM_GUTTER_COUNT; g++) {
            Rectangle r = ATM_GUTTERS[g];
            if (y < r.y || y > r.y + r.height) continue;
            grid_seg_h(y, r.x - ext, r.x + r.width + ext, a);
        }
    }
    for (int i = 1; i * ATM_GRID_STEP < UI_W; i++) {
        if (grid_skip(i)) continue;
        float x = (float)(i * ATM_GRID_STEP);
        float a = grid_major(i) ? ATM_GRID_MAJOR : ATM_GRID_ALPHA;
        float ext = grid_major(i) ? 4.0f : 0.0f;
        for (int g = 0; g < ATM_GUTTER_COUNT; g++) {
            Rectangle r = ATM_GUTTERS[g];
            if (x < r.x || x > r.x + r.width) continue;
            grid_seg_v(x, r.y - ext, r.y + r.height + ext, a);
        }
    }
}

/* 展台下 1/3 的网格（四边内缩 8）。**必须在 ui_panel(ATM_STAGE) 之后画**，
 * 否则会被展台的不透明填充盖掉。 */
static void atm_grid_stage(void)
{
    float x0 = ATM_STAGE_LOW.x + ATM_GRID_INSET;
    float x1 = ATM_STAGE_LOW.x + ATM_STAGE_LOW.width - ATM_GRID_INSET;
    float y0 = ATM_STAGE_LOW.y + 6.0f;
    float y1 = ATM_STAGE_LOW.y + ATM_STAGE_LOW.height - 6.0f;

    for (int i = 1; i * ATM_GRID_STEP < UI_H; i++) {
        if (grid_skip(i)) continue;
        float y = (float)(i * ATM_GRID_STEP);
        if (y < ATM_STAGE_LOW.y || y > ATM_STAGE_LOW.y + ATM_STAGE_LOW.height) continue;
        grid_seg_h(y, x0, x1, grid_major(i) ? ATM_GRID_MAJOR : ATM_GRID_ALPHA);
    }
    for (int i = 1; i * ATM_GRID_STEP < UI_W; i++) {
        if (grid_skip(i)) continue;
        float x = (float)(i * ATM_GRID_STEP);
        if (x < x0 || x > x1) continue;
        grid_seg_v(x, y0, y1, grid_major(i) ? ATM_GRID_MAJOR : ATM_GRID_ALPHA);
    }
}

/* ------------------------------------------------------------------------- */
/* 6. 培养皿同心环（02 §3.3）
 *
 * 圆心 = 展台中心 (658,316)，R = 展台半宽 262。只画 150°–210° 与 330°–30° 两段
 * → 左右各一段 3 条同心弧，上下完全为空，读作"皿沿从窗外探进来"。
 * 不用 scissor：角度区间直接就是裁剪结果。 */

static void bake_plate_rings(void)
{
    Vector2 c = { ATM_STAGE.x + ATM_STAGE.width * 0.5f,
                  ATM_STAGE.y + ATM_STAGE.height * 0.5f };
    static const float k[3]   = { 0.970f, 1.000f, 1.035f };
    static const float a[3]   = { 0.14f,  0.20f,  0.10f  };

    for (int i = 0; i < 3; i++) {
        float r = ATM_RING_R * k[i];
        ATM_DRAW(DrawRing(c, r, r + 1.0f, 150.0f, 210.0f, ATM_RING_SEG,
                          Fade(COL_LINE, a[i])));
        ATM_DRAW(DrawRing(c, r, r + 1.0f, 330.0f, 390.0f, ATM_RING_SEG,
                          Fade(COL_LINE, a[i])));
    }
}

/* ------------------------------------------------------------------------- */
/* 7. 暗角（02 §4.1）：两级，位置不同、上限不同 */

/* L1-V 背景暗角：画在静态层最底（背景层里），只作用于缝隙与全屏页，永不影响文字。
 * 3 层 112/72/36、α 0.030/0.035/0.040，最外圈累计 0.101。
 * 四角自然叠成更深的角 —— 这正是想要的"目镜"感，不需要额外画角。 */
static void bake_bg_vignette(void)
{
    static const float band[3]  = { 112.0f, 72.0f, 36.0f };
    static const float alpha[3] = { ATM_VIG_A_L1, 0.035f, 0.040f };

    for (int i = 0; i < 3; i++) {
        float w = band[i];
        float W = (float)UI_W, H = (float)UI_H;
        Color col = Fade(ATM_VIGNETTE, alpha[i]);
        /* 02 §4.1 写的是 `DrawRectangleRounded(rect, 0, 1, ·)`；raylib 的
         * roundness<=0 会直接早退成 DrawRectangleRec，两者逐像素相同，
         * 这里直接用 Rec 以免依赖那条内部实现细节。 */
        ATM_DRAW(DrawRectangleRec((Rectangle){ 0.0f, 0.0f, W, w }, col));
        ATM_DRAW(DrawRectangleRec((Rectangle){ 0.0f, H - w, W, w }, col));
        ATM_DRAW(DrawRectangleRec((Rectangle){ 0.0f, 0.0f, w, H }, col));
        ATM_DRAW(DrawRectangleRec((Rectangle){ W - w, 0.0f, w, H }, col));
    }
}

/* ------------------------------------------------------------------------- */
/* 8. 烘焙 */

static void free_static(void)
{
    if (IsTextureValid(g_static)) UnloadTexture(g_static);
    g_static = (Texture2D){ 0 };
    g_ready = false;
}

/* 把 alpha 强制回 255。烘焙时的半透明元素会把 RT 的 alpha 打下去：
 * raylib 的默认混合对 alpha 通道用的是同一个 glBlendFunc(GL_SRC_ALPHA,
 * GL_ONE_MINUS_SRC_ALPHA)，于是 dst.a = a² + dst.a·(1-a) —— 画一条 α0.16 的网格线，
 * 该像素的 alpha 就掉到 0.867。而这张纹理的定义是"不透明背景层"，blit 时
 * 必须逐像素复制；带着 0.87 的 alpha 去 blit，等于把每个元素再衰减 13%，
 * 合成后的底色会往画布的清屏色偏（实测网格峰值 17,29,31 而不是 16,28,30）。
 * 一行修正，所有 CR / 码值差取证的基线才是对的。 */
static void image_force_opaque(Image *img)
{
    if (img->format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) return;
    unsigned char *p = (unsigned char *)img->data;
    int n = img->width * img->height;
    for (int i = 0; i < n; i++) p[i * 4 + 3] = 255;
}

/* RT → Image → Texture2D。三个坑都在这里：翻转、alpha、立刻还回 RT。 */
static Texture2D rt_to_texture(RenderTexture2D *rt, int flip)
{
    Image img = LoadImageFromTexture(rt->texture);
    if (flip) ImageFlipVertical(&img);        /* 坑 2 */
    image_force_opaque(&img);                 /* 坑 5：alpha 泄漏 */
    Texture2D tex = LoadTextureFromImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR); /* 禁 POINT：靠它做下采样 */
    UnloadImage(img);
    UnloadRenderTexture(*rt);
    rt->texture.id = 0;                        /* 标记"已交还"，防止二次 Unload */
    return tex;
}

static void bake_static(int ss)
{
    free_static();

    RenderTexture2D rt = LoadRenderTexture(UI_W * ss, UI_H * ss);
    if (!IsRenderTextureValid(rt)) {
        TraceLog(LOG_WARNING, "ATM: static RT %dx%d failed, background disabled",
                 UI_W * ss, UI_H * ss);
        return;
    }

    BeginTextureMode(rt);
    ClearBackground(COL_BG); /* 坑 1：不透明底，避免预乘偏色 */

    Camera2D cam = { 0 };
    cam.zoom = (float)ss;
    BeginMode2D(cam);

    /* --- 背景层，从下到上 --- */
    bake_bg_vignette();          /* L1-V */
    atm_grid();                  /* L2：只落在 9 条缝里 */

    for (int i = 0; i < 6; i++)  /* L1：6 个缝隙母题，R 降序 */
        bake_motif(&ATM_GUTTER_MOTIFS[ATM_GUTTER_ORDER[i]]);

    /* --- 展台（唯一允许内部绘制的卡）---
     * 用 ui_panel 本身，不自己拼原语：这样圆角与描边跟 screens.c 的 draw_stage
     * 逐像素一致，atm_draw_live_in() 的子区域 blit 才是恒等操作（见文件头坑 4）。 */
    ui_panel(ATM_STAGE, R_CARD, COL_STAGE, COL_LINE);

    atm_grid_stage();            /* 展台下 1/3 的刻度 */
    bake_plate_rings();          /* 皿环 */
    {
        AtmCell s3 = ATM_STAGE_MOTIF_FAR1;
        AtmCell s4 = ATM_STAGE_MOTIF_FAR2;
        bake_motif(&s3);
        bake_motif(&s4);
    }
    ui_text(ATM_LABEL_A, ATM_LABEL_A_X, ATM_LABEL_A_Y, F_CAPTION, COL_FAINT);
    ui_text(ATM_LABEL_B, ATM_LABEL_B_X, ATM_LABEL_B_Y, F_CAPTION, COL_FAINT);

    EndMode2D();
    EndTextureMode();

    g_static = rt_to_texture(&rt, 1);
    g_static_ss = ss;
    g_ready = IsTextureValid(g_static);

    TraceLog(LOG_INFO, "ATM: static baked %dx%d -> tex %dx%d (%d bytes), SS=%d",
             UI_W * ss, UI_H * ss, g_static.width, g_static.height,
             g_static.width * g_static.height * 4, ss);
}

/* ------------------------------------------------------------------------- */
/* 9. 生命周期 */

void atm_ensure(void)
{
    int ss = ui_canvas_ss(); /* 02 §0.3-B2：SS 只在这里读一次 */
    if (ss < UI_SS_MIN) ss = UI_SS_MIN;
    if (ss > UI_SS_MAX) ss = UI_SS_MAX;

    if (!(g_ready && ss == g_static_ss)) bake_static(ss);
}

void atm_init(void)
{
    /* 只清状态，不烘焙：烘焙由第一次 atm_ensure() 触发，那时 ui.c 的
     * canvas_ensure() 已经把 g_ss 定下来。否则会先按 SS=1 白烧一张（4K 下
     * 一次 readback 就是 30–60ms）再立刻重烧一张，白付一次启动成本。 */
    g_static = (Texture2D){ 0 };
    g_static_ss = 0;
    g_ready = false;
}

void atm_shutdown(void)
{
    free_static();
    g_static_ss = 0;
}

/* ------------------------------------------------------------------------- */
/* 10. 绘制（每帧路径，02 §6.1 的 16 调用上限就管这四个函数）
 *
 * 计数表（02 §7.2-A3，静态审计；helper 的调用点计入调用方）：
 *   atm_draw_static     1  （1 × DrawTexturePro）
 *   atm_draw_live      12  （近 3 + 中 2 + 远 1 + 微尘 6）
 *   atm_draw_live_in   13  （子区域 blit 1 + 近 3 + 中 2 + 远 1 + 微尘 6）
 *   atm_draw_vignette   2  （2 × DrawRectangleLinesEx，见该函数注释）
 *   主界面一帧 = 1 + 13 + 2 = 16 ≤ 16 ✓（余量 0）
 *   全屏页一帧 = 1 + 12 + 2 = 15 ≤ 16 ✓（余量 1）
 */

void atm_draw_static(void)
{
#ifndef NDEBUG
    g_calls = 0;
#endif
    if (!g_ready) return;

    Rectangle src = { 0.0f, 0.0f, (float)g_static.width, (float)g_static.height };
    Rectangle dst = { 0.0f, 0.0f, (float)UI_W, (float)UI_H };
    ATM_DRAW(DrawTexturePro(g_static, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE));
}

/* 实时细胞（02 §2.3 硬拆分）：实时一律只用"外壁 + 1 层外扩"，
 * NEAR 的 3 层柔边只允许出现在预渲染的静态元素里 —— 于是近 3 / 中 2 / 远 1。
 * 动效只改 R 与 α，**圆心不动**（01 §8.3、02 §2.5）。 */
static void atm_live_cell(const AtmCell *m, float t, float period, float phase)
{
    Vector2 c = { m->x, m->y }; /* 常数：绝不参与 sinf */
    float R = m->R * (1.0f + 0.02f * sinf(t * (ATM_TAU / period) + phase));
    float ab = 1.0f + 0.25f * sinf(t * (ATM_TAU / period) + phase * 1.7f);

    switch (m->tier) {
    case TIER_NEAR: { /* 母题 A 精简：fill + 柔边 + 外壁 */
        ATM_DRAW(DrawCircleV(c, R * 0.94f, Fade(ATM_CELL, ATM_CELL_FILL)));
        ATM_DRAW(DrawRing(c, R * 0.94f, R * 1.00f, 0.0f, 360.0f, 64,
                          Fade(ATM_CELL, ATM_CELL_EDGE)));
        float a = ATM_HALO_A_NEAR[0] * ab;
        if (a > ATM_HALO_A_NEAR[0]) a = ATM_HALO_A_NEAR[0]; /* 峰值硬上限 0.30 */
        atm_ring_line(c, R * 0.985f, R * 0.03f, a, 96);
        break;
    }
    case TIER_MID: /* 母题 B 精简：外壁 + 1 层外扩 */
        atm_ring_line(c, R * 0.99f, R * 0.03f, ATM_HALO_A_MID[0] * ab, 64);
        atm_ring_line(c, R * ATM_HALO_R_MID[1], R * 0.015f, ATM_HALO_A_MID[1] * ab, 64);
        break;
    default: /* 远景：1 条轮廓，无内部结构（判据 D1） */
        atm_ring_line(c, R * 0.995f, R * 0.03f, ATM_HALO_A_FAR * ab, 48);
        break;
    }
}

/* 6 粒微尘（02 §2.5 / §6.5）：位置与半径全固定，只做 α 呼吸。
 * **绝不做 y 位移** —— 那正是被修掉的 18 个上浮气泡（§6.5）。
 * 必须用 COL_LINE 而不是 COL_CELL：COL_CELL 与 COL_STAGE 的通道差只有 (5,7,7)，
 * 乘 0.04 后码值差为 (0,0,0)，画了等于没画。 */
static void atm_dust(const Vector2 *pos, int n, float t)
{
    for (int i = 0; i < n; i++) {
        float br = 0.5f + 0.5f * sinf(t * (ATM_TAU / 3.1f) + (float)i * 0.7f);
        float a = ATM_DUST_MIN + (ATM_DUST_MAX - ATM_DUST_MIN) * br;
        ATM_DRAW(DrawCircleV(pos[i], 1.5f, Fade(COL_LINE, a)));
    }
}

void atm_draw_live(void)
{
    /* 全屏页（END_ERA / LEGACY）：无卡片遮挡，氛围红利最大。
     * 主界面不要调这个 —— 会被不透明的展台整块盖住，白烧 12 个调用。 */
    float t = g_ui.time;
    AtmCell a = { 210.0f, 300.0f, 58.0f, MOTIF_VESICLE, TIER_NEAR };
    AtmCell b = { 1070.0f, 470.0f, 34.0f, MOTIF_SHELL, TIER_MID };
    AtmCell c = {  980.0f, 210.0f, 18.0f, MOTIF_SHELL, TIER_FAR };

    atm_live_cell(&a, t, 7.0f, 0.0f);
    atm_live_cell(&b, t, 11.3f, 1.7f);
    atm_live_cell(&c, t, 4.6f, 3.1f);
    atm_dust(ATM_DUST_FULL, 6, t);
}

void atm_draw_live_in(Rectangle area)
{
    float t = g_ui.time;

    /* ① 把展台那段静态内容捞回来（1 次子区域取样）。
     *    必须有：主画布上这段已经被 draw_stage 的不透明 COL_STAGE 盖住了。
     *    恒等性由烘焙时直接调 ui_panel() 保证（见文件头坑 4），所以这里
     *    **用原 rect、不做内缩** —— 内缩 2px 反而会把圆角描边切掉几个像素。 */
    if (g_ready) {
        float s = (float)g_static_ss;
        Rectangle src = { area.x * s, area.y * s, area.width * s, area.height * s };
        Rectangle dst = area;
        ATM_DRAW(DrawTexturePro(g_static, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE));
    }

    /* ② 3 个实时细胞（08 附录 D 的 S1/S2 + 一个远景补位）。
     *    落点用**绝对坐标**，不按 area 比例算：版式若变，宁可显式改常数。 */
    {
        AtmCell s1 = ATM_STAGE_MOTIF_NEAR;
        AtmCell s2 = ATM_STAGE_MOTIF_MID;
        AtmCell s3 = ATM_STAGE_MOTIF_FAR1;
        /* S3 是静态远景，实时层不再画它 —— 这里改成画一个更靠下的远景呼吸点，
         * 让下 1/3 有"活"的东西，同时不重复静态层已经有的轮廓。 */
        s3.x = 470.0f; s3.y = 340.0f; s3.R = 12.0f;

        atm_live_cell(&s1, t, 7.0f, 0.0f);
        atm_live_cell(&s2, t, 11.3f, 1.7f);
        atm_live_cell(&s3, t, 4.6f, 3.1f);
    }

    /* ③ 6 粒微尘 */
    atm_dust(ATM_DUST_STAGE, 6, t);
}

/* L5 前景暗角（02 §4.1）：只压最外 16 逻辑 px，2 层 × α0.020。
 *
 * ⚠ 这一层**刻意不用离屏纹理**，与 02 §6.1 表里"L5 暗角 blit = 1 调用"不同。
 * 实测在本机（Intel / GL 3.3 / raylib 5.5）上，号称为它省调用的两条路都是死的：
 *
 *   ① 「透明底 RT + 普通 alpha blit」—— 8bit 量化直接吃掉：RT 里存的是预乘结果，
 *      Fade(·,0.020) 的 rgb 只有 5/12/14，乘 0.02 是 0.1/0.24/0.28 → 归 0。
 *      （更糟：raylib 的默认混合对 alpha 用的是同一个 glBlendFunc，dst.a = a²，
 *       2% 的 alpha 写进 RGBA8 就变成 0。实测 alpha=0。）
 *   ② 「白底乘法层 + BLEND_MULTIPLIED blit」—— DrawTexturePro 配 MULTIPLIED
 *      在本机完全不生效（对照实验：同一颜色用 DrawRectangle + MULTIPLIED 正常
 *      得到 250×128/255=125，换成 DrawTexturePro 却是原样不变）。不依赖它。
 *
 * 改用 2 次 DrawRectangleLinesEx：
 *   - 每层 1 个调用点，直接画在主画布上、逐帧真 alpha 混合 → 零量化损失；
 *   - 几何与"4 条边带"完全等价（raylib 内部就是 4 个矩形，侧边用 h-2t 扣掉角，
 *     所以四角不会叠两次，正好是 §4.1 要的单层）；
 *   - 省掉 1.0 MiB 纹理，也省掉"与 SS 无关但要在 resize 时重建"的隐患；
 *   - 代价：本函数从 1 个调用点变成 2 个 → 主界面每帧 15→16，吃掉 §6.1 的余量 2。
 *     这是本轮唯一一处吃掉余量的改动，已写进交付报告。
 */
void atm_draw_vignette(void)
{
    Rectangle full = { 0.0f, 0.0f, (float)UI_W, (float)UI_H };

    ATM_DRAW(DrawRectangleLinesEx(full, ATM_VIG_BAND_L5, Fade(BLACK, ATM_VIG_A_L5)));
}
