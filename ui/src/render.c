/*
 * render.c  ——  绘制原语（Member C）
 *
 * 只做“怎么画”，不做“画什么”。页面结构在 screens.c，交互在 input.c/ui.c。
 * 所有坐标都在 1280×800 的逻辑画布内，缩放由 ui.c 的离屏纹理统一处理。
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "raylib.h"
#include "ui_internal.h"
#include "ui_adapter.h"
#include "ui_glyphs.h"

/* ================= 调色板 =================
 * 唯一允许出现颜色字面量的地方（01 §2.2 附录 B 逐字对齐）。
 * 界面代码只许引用 COL_* 名字；任何括号形式的 Color 复合字面量出现在
 * screens.c / ui.c / render.c 的其他位置都是 bug（01 §0 R0-1）。
 * 验收：用 grep 搜 "Color)" 紧跟 "{" 的字面量写法，这三个文件里必须全为 0。
 *
 * 分层逻辑：BG_DEEP < BG < CHROME ≈ STAGE < SURFACE_OFF < PANEL < BAR_TRACK < PANEL_HI < PANEL_HOT
 * 相邻两面的 CR 必须 ≥ 1.10 才看得见（01 §2.5）；PANEL↔STAGE(1.08) 与
 * PANEL↔EP_SURFACE(1.02) 不达标，所以这两类容器**必须保留描边**。 */

/* ===== 基础面 ===== */
const Color COL_BG_DEEP         = {   7,  14,  16, 255 };  /* 遮罩下最底层 / 清屏色 */
const Color COL_BG              = {  11,  21,  23, 255 };  /* 常规页面底色 */
const Color COL_PANEL           = {  18,  31,  34, 255 };  /* 卡片 / 面板 / 弹层底板 */
const Color COL_PANEL_HI        = {  26,  42,  46, 255 };  /* 抬升面：弹层内卡片、未选中页签、按钮底 */
const Color COL_CHROME          = {  15,  28,  31, 255 };  /* 顶栏 0–88 / 底栏 736–800 */
const Color COL_STAGE           = {  12,  23,  26, 255 };  /* 展台凹陷面（必须配 COL_LINE 描边） */
const Color COL_SURFACE_OFF     = {  14,  25,  28, 255 };  /* 未解锁卡 / 空槽 / 禁用按钮 */
const Color COL_PANEL_HOT       = {  34,  59,  66, 255 };  /* hover / 按下 / 选中填充 */
const Color COL_BAR_TRACK       = {  23,  38,  42, 255 };  /* 所有进度条槽底 */
const Color COL_LINE            = {  43,  65,  69, 255 };  /* 静态描边 / 分割线（1.56，不可作可点证据） */
const Color COL_LINE_STRONG     = {  92, 128, 132, 255 };  /* 轴线 / 焦点框（3.92） */

/* ===== 文字（三层信息分级） ===== */
const Color COL_TEXT            = { 236, 244, 241, 255 };  /* 15.08:1 —— 必须读到的字 */
const Color COL_DIM             = { 163, 184, 182, 255 };  /*  8.10:1 —— 说明性副文本 */
const Color COL_FAINT           = { 122, 147, 146, 255 };  /*  5.15:1 —— 单位后缀 / 快捷键 / 公式 */

/* ===== 语义（高饱和 = 信号） ===== */
const Color COL_ACCENT          = {  53, 214, 198, 255 };  /* 品牌色：主谱系色轨 / 可交互描边 */
const Color COL_ATP             = { 255, 197,  61, 255 };
const Color COL_BIOMASS         = { 147, 209,  79, 255 };
const Color COL_DNA             = { 138, 160, 255, 255 };
const Color COL_EP              = { 209, 155, 255, 255 };
const Color COL_BAD             = { 255, 107, 122, 255 };
const Color COL_GOOD            = { 107, 239, 160, 255 };
const Color COL_RARE            = { 255, 159,  69, 255 };

/* ===== 语义容器（低对比面，靠描边立边界） ===== */
const Color COL_EP_SURFACE      = {  34,  27,  51, 255 };
const Color COL_BAD_SURFACE     = {  43,  22,  24, 255 };
const Color COL_ACCENT_SURFACE  = {  20,  46,  44, 255 };

/* ===== 物种身份（统一 L=0.340、S≈0.50，只用色相区分身份）=====
 * 低饱和 = 身份，高饱和 = 信号。物种色永不做文字色。 */
const Color COL_SP0 = {  74, 176, 125, 255 };  /* 原始细胞 150° */
const Color COL_SP1 = { 114, 173,  73, 255 };  /* 光合细菌  95° */
const Color COL_SP2 = { 189, 152,  79, 255 };  /* 化能细菌  40° */
const Color COL_SP3 = {  72, 173, 168, 255 };  /* 藻类真核 177° */
const Color COL_SP4 = { 254, 106, 163, 255 };  /* 原生动物 337° */
const Color COL_SP5 = { 170, 138, 255, 255 };  /* 多细胞群体 250° */
const Color COL_SP6 = { 240, 101, 240, 255 };  /* 极端环境群体 300° */

/* ===== 背景层（01 §2.2b；02-atmosphere 的氛围层专用，界面层不引用）=====
 * 与 COL_BG 的 CR 全部 < 1.10 —— 单独看几乎不可见，可见度来自多层叠加。
 * 界面层若拿它们当普通面色用，会直接违反 01 §2.5 的分层可见性门槛。 */
const Color COL_CELL     = {  17,  30,  33, 255 };  /* 背景细胞面 / 培养皿内壁（CR 1.086） */
const Color COL_VIGNETTE = {   5,  12,  14, 255 };  /* 四周暗角 */
const Color COL_GHOST    = {  30,  49,  52, 255 };  /* 图鉴未发现轮廓，仅配 COL_SURFACE_OFF 底 */

/* ================= Z-UNSAFE 原语 =================
 * ⚠ DrawRectangleRoundedLinesEx / DrawCircleLinesV / DrawEllipseLines 的线宽
 *   以设备像素计、不吃 Camera2D(zoom)。要画“1 逻辑像素”的环或边框，必须改用
 *   DrawLineEx / DrawRing / DrawRectangleRounded 的嵌套填充，否则 SS=2/3 时
 *   线会变成 1/2 或 1/3 物理像素（ENG-01 实测：SS=3 下恒为 1 设备像素）。
 *   跟 zoom 正常的：DrawLineEx / DrawRectangleLinesEx / DrawRing / DrawCircleV /
 *   DrawCircleSector / DrawEllipse / DrawTriangle / DrawRectangleRounded。
 *   ui_panel 的嵌套填充写法就是这条警告的产物；新增描边请先查这张表。 */

/* ================= Z-A1 临时妥协（待逐边反锯齿后改回 1.0f）=================
 * 21 处 1.0f 线宽里有 3 处（atmosphere.c 的 grid_seg_h / grid_seg_v、
 * 本文件的 ui_divider）走 `W_HAIRLINE_AA = 1.05f` 而不是 `W_HAIRLINE = 1.0f`。
 * 原因：线宽 1.0 在非整数落点上会被光栅化拆成相邻两行各 50%，4K 下看是"虚线感"；
 * 1.05 让 floor(1.05×SS) 在 SS=1/2/3 下稳定给 1/2/3 物理像素且有整行满覆盖。
 * **这是临时值**：等 §7.4 的逐边反锯齿（把线改成 4 边矩形 + 端点补偿）落地后，
 * 三处统一改回 W_HAIRLINE，W_HAIRLINE_AA 整个删掉。
 * 注意 W_HAIRLINE 本体**保持 1.0f** —— ui_panel 的嵌套描边靠它拿满强度整数像素，
 * 改成 1.05 会引入过渡行（详见 ui_internal.h 里的长注释）。 */

static Font g_font;
static bool g_font_ready = false;

/* 字体图集一次性按逻辑字号的 3 倍光栅化（3 = 画布最大超采样 SS）。
 * 绘制时 DrawTextEx 的 size 仍是逻辑像素（40/22/16/13…），raylib 会把 120px
 * 图集缩到目标尺寸；因为整层又被 Camera2D 放大了 SS 倍，SS=3 时正好 1:1 命中
 * 图集原始像素 —— 中文在 4K 下才是真清晰，而不是把 40px 位图拉大。
 * 图集约 21MB，换来 SS=1..3 全档位都够用，不必随 SS 重新光栅化。
 *
 * 【硬约束 · 字号上限】baseSize=120 决定全局最大可用字号 = 120 / UI_SS_MAX(3) = 40。
 * 美术字号阶梯最高 38，余量仅 2px。要加 40 以上的 hero 字（如 44），必须先把这个
 * 值抬到 160（44*3=132 → 160 够，图集会从 4096×2048 涨到约 37MB），
 * 否则字形超出图集原始分辨率、被插值放大，**反而比 38 更糊**。
 * 见 docs/art/01-visual-identity.md §7.3-C2。 */
#define UI_FONT_BASE_SIZE 120

/* ---------------- UTF-8 → 码点，并累积字形集 ---------------- */

static int *g_cp = NULL;
static int g_cp_count = 0;
static int g_cp_cap = 0;

static void cp_push(int cp)
{
    if (cp <= 0) return;
    for (int i = 0; i < g_cp_count; ++i) {
        if (g_cp[i] == cp) return;
    }
    if (g_cp_count == g_cp_cap) {
        int next = g_cp_cap ? g_cp_cap * 2 : 512;
        int *grown = (int *)realloc(g_cp, sizeof(int) * (size_t)next);
        if (!grown) return;
        g_cp = grown;
        g_cp_cap = next;
    }
    g_cp[g_cp_count++] = cp;
}

void ui_font_register(const char *utf8)
{
    if (!utf8) return;
    const unsigned char *p = (const unsigned char *)utf8;
    while (*p) {
        int cp = 0;
        if (*p < 0x80) {
            cp = *p++;
        } else if ((*p & 0xE0) == 0xC0 && p[1]) {
            cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
            p += 2;
        } else if ((*p & 0xF0) == 0xE0 && p[1] && p[2]) {
            cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
            p += 3;
        } else if ((*p & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) {
            cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
            p += 4;
        } else {
            p++;
            continue;
        }
        cp_push(cp);
    }
}

static void register_fixed_glyphs(void)
{
    for (int c = 32; c < 127; ++c) cp_push(c);
    for (int i = 0; i < UI_GLYPH_PART_COUNT; ++i) ui_font_register(UI_GLYPH_PARTS[i]);
}

void ui_font_init(void)
{
    register_fixed_glyphs();
    ua_register_font_strings();

    /* 常用符号与标点，避免模板里出现问号 */
    ui_font_register("·×÷≥≤→↑↓—…‰％￥／（）【】「」『』、，。：；！？“”‘’");

    g_font = LoadFontEx("assets/fonts/ui_zh.ttf", UI_FONT_BASE_SIZE, g_cp, g_cp_count);
    if (g_font.texture.id == 0) {
        g_font = GetFontDefault();
        g_font_ready = false;
    } else {
        /* 必须 BILINEAR，**禁止 TEXTURE_FILTER_POINT**：图集是 120px、实际绘制
         * 字号低到 12px（缩 10 倍），点采样会直接丢笔画/出锯齿；BILINEAR 才是
         * 这里唯一正确的选择。画布纹理同理（见 ui.c canvas_ensure）。 */
        SetTextureFilter(g_font.texture, TEXTURE_FILTER_BILINEAR);
        g_font_ready = true;

        /* 自检：登记过但字体里没有的字形，说明文案漏登记或字体缺字。
         * 跳过 U+0000..U+0020（控制符与空格）：raylib 对空格返回 index 0，
         * 会稳定误报一条 "missing U+0020"，把真正的缺字淹掉。 */
        int missing = 0;
        for (int i = 0; i < g_cp_count; ++i) {
            if (g_cp[i] <= 32) continue;
            if (GetGlyphIndex(g_font, g_cp[i]) == 0) {
                if (missing < 8) TraceLog(LOG_WARNING, "UI font: missing U+%04X", g_cp[i]);
                missing++;
            }
        }
        if (missing > 0) {
            TraceLog(LOG_WARNING, "UI font: %d glyphs requested, %d missing", g_cp_count, missing);
        } else {
            TraceLog(LOG_INFO, "UI font: %d glyphs loaded", g_font.glyphCount);
        }
    }
}

void ui_font_shutdown(void)
{
    if (g_font_ready) UnloadFont(g_font);
    free(g_cp);
    g_cp = NULL;
    g_cp_count = g_cp_cap = 0;
}

/* ---------------- 文本 ---------------- */

float ui_text(const char *text, float x, float y, float size, Color color)
{
    if (!text || !text[0]) return 0.0f;
    float spacing = size * 0.02f;
    Vector2 pos = { x, y };
    DrawTextEx(g_font, text, pos, size, spacing, color);
    return MeasureTextEx(g_font, text, size, spacing).x;
}

float ui_text_w(const char *text, float size)
{
    if (!text || !text[0]) return 0.0f;
    return MeasureTextEx(g_font, text, size, size * 0.02f).x;
}

void ui_text_center(const char *text, float cx, float y, float size, Color color)
{
    float w = ui_text_w(text, size);
    ui_text(text, cx - w * 0.5f, y, size, color);
}

void ui_text_right(const char *text, float rx, float y, float size, Color color)
{
    float w = ui_text_w(text, size);
    ui_text(text, rx - w, y, size, color);
}

/* ---------------- 颜色与缓动小工具 ---------------- */

/* 两色线性插值。用来在同一 token 家族的相邻两面之间过渡（hover / 选中），
 * 避免为了一个中间态再硬编码一个字面量。 */
Color ui_color_mix(Color a, Color b, float t)
{
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    Color c;
    c.r = (unsigned char)((float)a.r + ((float)b.r - (float)a.r) * t);
    c.g = (unsigned char)((float)a.g + ((float)b.g - (float)a.g) * t);
    c.b = (unsigned char)((float)a.b + ((float)b.b - (float)a.b) * t);
    c.a = (unsigned char)((float)a.a + ((float)b.a - (float)a.a) * t);
    return c;
}

static float ease_out_cubic(float u)
{
    if (u <= 0.0f) return 0.0f;
    if (u >= 1.0f) return 1.0f;
    float v = 1.0f - u;
    return 1.0f - v * v * v;
}

/* ---------------- 面板与控件 ---------------- */

/* 圆角矩形的 roundness ∈ [0,1]，1 = 胶囊。radius 是逻辑半径，换算时要除以短边的
 * 一半 —— 这样同一个 radius 在任意矩形上都保持物理半径不变（R_PANEL 16 在
 * 752×650 面板和 200×216 卡片上一样圆）。 */
static float roundness_of(float radius, Rectangle r)
{
    float m = (r.width < r.height ? r.width : r.height);
    if (m <= 0.0f) return 0.0f;
    float k = radius / m * 2.0f;
    return k > 1.0f ? 1.0f : k;
}

void ui_panel(Rectangle r, float radius, Color fill, Color border)
{
    if (radius <= 0.0f) {
        /* 直角分支：DrawRectangleLinesEx 实测跟随 zoom（3 逻辑 px → 3×SS 设备 px），
         * 所以这里保持原样，不需要走嵌套填充。 */
        DrawRectangleRec(r, fill);
        if (border.a > 0) DrawRectangleLinesEx(r, W_HAIRLINE, border);
        return;
    }

    float k_out = roundness_of(radius, r);

    if (border.a == 0) {
        DrawRectangleRounded(r, k_out, 8, fill);
        return;
    }

    /* 半透明填充无法用"外圈描边色 + 内缩填充"盖出来（内圈会透出外圈色），
     * 回退旧路径。代价：SS>1 时描边退化为 1 设备像素，仅用于确实需要半透明的面。 */
    if (fill.a < 255) {
        DrawRectangleRounded(r, k_out, 8, fill);
        DrawRectangleRoundedLinesEx(r, k_out, 8, W_HAIRLINE, border);
        return;
    }

    /* 方案 A：外圈整块铺描边色 + 内缩 1 逻辑 px 铺填充。
     * 为什么不用 DrawRectangleRoundedLinesEx —— 它是 Z-UNSAFE 原语，线宽按设备像素
     * 算、不吃 Camera2D(zoom)，SS=3 时描边只有 1/3 逻辑像素粗，且与填充之间会露出
     * 1–2px 背景缝（见文件顶部 Z-UNSAFE 说明）。改用两次填充后描边是四边形，
     * 正确放大 SS 倍：SS=1/2/3 → 1/2/3 物理像素，与 DrawLineEx 同口径。 */
    DrawRectangleRounded(r, k_out, 8, border);

    Rectangle in = { r.x + W_HAIRLINE, r.y + W_HAIRLINE, r.width - 2.0f * W_HAIRLINE, r.height - 2.0f * W_HAIRLINE };
    if (in.width < 1.0f || in.height < 1.0f) return;

    /* 内圈半径必须**按内圈自己的短边重算**，不能复用 k_out：
     * k_out 套到内圈上得到的实际半径是 radius × minin/minout。44 高按钮（radius 10）
     * 会变成 9.545，216 高的树卡（radius 12）变成 11.888 —— 表现为"角部描边比直边粗"。
     * 误差随 radius/短边 比值放大，所以这里重算一次。 */
    float r_in = radius - W_HAIRLINE;
    if (r_in < 0.0f) r_in = 0.0f;
    DrawRectangleRounded(in, roundness_of(r_in, in), 8, fill);
}

void ui_panel_soft(Rectangle r, float radius, Color fill)
{
    Color c = fill;
    ui_panel(r, radius, c, BLANK);
}

void ui_bar(Rectangle r, float t, Color fill, Color back)
{
    /* 槽底恒为 COL_BAR_TRACK（01 §2.2）。传 BLANK 表示“用默认槽底”，
     * 这样新增进度条不会因为忘记传色而画出参差的底色。 */
    if (back.a == 0) back = COL_BAR_TRACK;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    DrawRectangleRounded(r, 1.0f, 6, back);
    if (t > 0.001f) {
        Rectangle f = r;
        f.width = r.width * t;
        if (f.width < r.height) f.width = r.height;
        DrawRectangleRounded(f, 1.0f, 6, fill);
    }
}

void ui_divider(float x1, float x2, float y, Color color)
{
    /* 走 W_HAIRLINE_AA（=1.05）而不是 W_HAIRLINE：分割线是典型"非整数坐标上的
     * 1 逻辑像素横线"，1.0 在 SS=2/3 下会被光栅化拆成两行各半强度。
     * 这是 Z-A1 的临时妥协，理由与"为什么不能改 W_HAIRLINE 本体"见 ui_internal.h。 */
    DrawLineEx((Vector2){ x1, y + 0.5f }, (Vector2){ x2, y + 0.5f }, W_HAIRLINE_AA, color);
}

/* ---------------- 数字格式 ---------------- */

void ui_fmt_num(double v, char *out, int n)
{
    if (n <= 0) return;
    double a = fabs(v);
    if (a < 1000.0) {
        if (a < 10.0) snprintf(out, (size_t)n, "%.1f", v);
        else snprintf(out, (size_t)n, "%.0f", v);
        return;
    }
    const char *suffix[] = { "k", "M", "B", "T", "P" };
    int s = -1;
    while (a >= 1000.0 && s < 4) {
        a /= 1000.0;
        v /= 1000.0;
        s++;
    }
    if (a < 10.0) snprintf(out, (size_t)n, "%.2f%s", v, suffix[s]);
    else if (a < 100.0) snprintf(out, (size_t)n, "%.1f%s", v, suffix[s]);
    else snprintf(out, (size_t)n, "%.0f%s", v, suffix[s]);
}

void ui_fmt_rate(double v, char *out, int n)
{
    char tmp[32];
    ui_fmt_num(v, tmp, (int)sizeof(tmp));
    snprintf(out, (size_t)n, "%s/s", tmp);
}

void ui_fmt_time(double seconds, char *out, int n)
{
    if (seconds < 0.0) seconds = 0.0;
    int total = (int)(seconds + 0.5);
    int m = total / 60;
    int s = total % 60;
    snprintf(out, (size_t)n, "%02d:%02d", m, s);
}

/* ---------------- 按钮 ----------------
 * 高度由调用方按 44 / 34 / 28 三档给（01 §6.3）。**两行文字（title+sub）只在
 * 44 档成立**：44 时 title 占 10–26、sub 占 26–40；34 档放两行会溢出 1px，
 * 所以带 sub 的按钮必须给 H_PRIMARY。
 * hover 从旧的“瞬移 1px”改成插值（01 §8.4 例 2）：指数趋近 ≈ ease-out-cubic，
 * 不需要记起始时间，掉帧也不会跳。 */
void ui_btn_draw(UiButton *b, const char *title, const char *sub, bool hot)
{
    float dt = GetFrameTime();
    if (dt > 0.12f) dt = 0.12f;   /* 卡顿/切回窗口时不要一步跳完 */

    float target = (hot && b->enabled) ? 1.0f : 0.0f;
    b->hover += (target - b->hover) * fminf(1.0f, dt / 0.12f);

    Color fill, border, text;
    if (!b->enabled) {
        fill   = COL_SURFACE_OFF;      /* 禁用态：比 COL_PANEL 更暗一档 */
        border = COL_LINE;
        text   = COL_FAINT;
    } else {
        fill   = ui_color_mix(COL_PANEL_HI, COL_PANEL_HOT, b->hover);
        border = b->hover > 0.02f ? COL_ACCENT : COL_LINE;
        text   = COL_TEXT;
    }

    Rectangle r = b->rect;
    r.y -= ease_out_cubic(b->hover);   /* 抬升 1 逻辑 px */

    ui_panel(r, R_BTN, fill, border);

    float cx = r.x + r.width * 0.5f;
    float cy = r.y + r.height * 0.5f;
    if (sub && sub[0]) {
        ui_text_center(title, cx, cy - 12.0f, F_LABEL, text);
        ui_text_center(sub, cx, cy + 4.0f, F_BODY, b->enabled ? COL_DIM : COL_FAINT);
    } else {
        ui_text_center(title, cx, cy - 8.0f, F_LABEL, text);
    }
}

/* 供 screens.c 复用的小工具：在按钮右侧画一个“不可用原因”角标 */
void ui_tag(Rectangle r, const char *text, Color color)
{
    float w = ui_text_w(text, F_CAPTION) + 14.0f;
    Rectangle tag = { r.x + r.width - w - S_GAP, r.y + S_GAP, w, H_CHIP };
    Color bg = color;
    bg.a = 50;
    ui_panel(tag, R_CHIP(tag.height), bg, BLANK);
    ui_text_center(text, tag.x + tag.width * 0.5f, tag.y + 4.0f, F_CAPTION, color);
}

void ui_dot(float cx, float cy, float radius, Color color)
{
    DrawCircleV((Vector2){ cx, cy }, radius, color);
}
