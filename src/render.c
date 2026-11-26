/*
 * render.c  ——  绘制原语（Member C）
 *
 * 视觉方向 3.0「Primordial Cosmos」：
 *   深空藏青（近黑）作底，金色作品牌与货币，生物体用荧光色发光。
 *   面板是“深蓝玻璃 + 金线”，主体是“被照亮的标本”。
 *
 * 只做“怎么画”，不做“画什么”。页面结构在 screens.c，交互在 input.c/ui.c。
 * 所有坐标都在 1280×720 的逻辑画布内，缩放由 ui.c 的离屏纹理统一处理。
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "raylib.h"
#include "ui_internal.h"
#include "ui_adapter.h"
#include "ui_glyphs.h"
#include "embedded.h"

/* ================= 调色板 =================
 * 唯一允许出现颜色字面量的地方。界面代码只许引用 COL_* 名字。
 *
 * 分层：BG_DEEP < BG < CHROME ≈ STAGE < SURFACE_OFF < PANEL < BAR_TRACK < PANEL_HI
 *       < PANEL_HOT。相邻面之间的差值刻意拉开，4K 下边界不糊。 */

/* ===== 深空底色 ===== */
const Color COL_BG_DEEP       = {   3,   5,  11, 255 };
const Color COL_BG            = {   7,  11,  22, 255 };
const Color COL_BG_LIFT       = {  12,  18,  34, 255 };
const Color COL_CHROME        = {   6,   9,  20, 255 };
const Color COL_STAGE         = {   4,   8,  18, 255 };
const Color COL_CELL          = {  18,  30,  50, 255 };
const Color COL_VIGNETTE      = {   1,   2,   6, 255 };

/* ===== 面板 ===== */
const Color COL_PANEL         = {  11,  17,  32, 255 };
const Color COL_PANEL_HI      = {  18,  27,  47, 255 };
const Color COL_PANEL_HOT     = {  27,  41,  68, 255 };
const Color COL_SURFACE_OFF   = {   8,  12,  23, 255 };
const Color COL_BAR_TRACK     = {  22,  34,  56, 255 };
const Color COL_LINE          = {  33,  49,  78, 255 };
const Color COL_LINE_STRONG   = {  82, 116, 164, 255 };
const Color COL_GHOST         = {  42,  58,  86, 255 };

/* ===== 文字 ===== */
const Color COL_TEXT          = { 238, 246, 255, 255 };
const Color COL_DIM           = { 170, 192, 218, 255 };
const Color COL_FAINT         = { 122, 146, 178, 255 };

/* ===== 语义 ===== */
const Color COL_ACCENT        = { 255, 197, 102, 255 };  /* 品牌金 */
const Color COL_GOLD_DEEP     = { 176, 126,  52, 255 };
const Color COL_ATP           = { 255, 208, 112, 255 };
const Color COL_BIOMASS       = { 126, 232, 152, 255 };
const Color COL_DNA           = { 198, 162, 255, 255 };
const Color COL_EP            = { 255, 233, 168, 255 };
const Color COL_BAD           = { 255, 112, 112, 255 };
const Color COL_GOOD          = { 104, 232, 180, 255 };
const Color COL_RARE          = { 255, 160,  92, 255 };
const Color COL_TEAL          = {  96, 224, 214, 255 };
const Color COL_VIOLET        = { 150, 122, 255, 255 };

/* ===== 语义容器 ===== */
const Color COL_EP_SURFACE     = {  36,  31,  19, 255 };
const Color COL_BAD_SURFACE    = {  46,  16,  24, 255 };
const Color COL_ACCENT_SURFACE = {  42,  31,  16, 255 };

/* ================= 字体 ================= */

static Font g_font;
static bool g_font_ready = false;

/* 图集按逻辑最大字号的 SS 倍光栅化：baseSize 120 / SS 3 = 40 逻辑像素上限。
 * 美术字号阶梯最高 38（F_HERO），留 2px 余量。再抬一档会让图集从 4096²
 * 涨到 8192²（显存 ×4），而 F_HERO 已经是版式上限，不要再动。 */
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

/* 界面里所有写死的中文串必须在这里登记一次，否则字形不进图集、显示成 '?'。
 * 扩写文案时把新串塞进下面任意一条即可（重复登记会被 cp_push 去重）。 */
static const char *const UI_STATIC_STRINGS[] = {
    "进化商店", "进化树", "生命图鉴", "生命体", "样本", "主谱系", "额外分支",
    "空置物种槽", "多样性解锁", "生物量里程碑", "下一档", "下一步", "主动突变",
    "物种分化", "结束本轮演化", "采集", "点击生命体或按空格采集", "每次",
    "稳定期", "危机中", "丰饶期", "距下一次扰动", "下一次扰动", "协同",
    "尚无突变", "环境适应", "无筛选", "常态海洋", "第", "纪元", "阶段",
    "突变", "物种", "危机", "结算", "永久", "当前", "确认", "取消", "关闭",
    "返回本轮", "继续成长", "结算并进入", "开始新轮演化", "资源与突变重置",
    "图鉴与强化保留", "未使用的 EP 会保留到以后", "已发现", "未发现", "稀有",
    "点击选择", "生成候选", "候选已锁定", "换页或读档都不会刷新", "选定后清除",
    "当前没有候选", "生成一次后固定保存", "需要", "前置不足", "生物量不足",
    "保留亲本为额外分支", "是", "否", "需要空的第二物种槽", "亲本离开活跃列表",
    "当前阶段没有可用的分化方向", "先通过突变满足前置条件",
    "最高纪元", "最高", "成就", "生态", "机能", "分化", "商店", "图鉴",
    "自动", "点击收益", "种群增长与生物量", "产出", "解锁突变", "发现物种",
    "度过危机", "主要进化阶段", "本轮持续", "确认后本轮冻结", "只入账一次",
    "本轮 Evolution Points", "现有", "未使用部分永久保留", "本轮奖励已领取",
    "只能开始新轮", "返回本轮继续成长不会损失任何进度",
    "把上一轮的成果投进永久强化", "新轮从原始生命重新演化",
    "点击卡片升级", "已满级", "尚未强化", "下一级", "满级",
    "主动变异", "基因纹章", "星辰", "已点亮", "未探索", "当前路径",
    "徽记亮起 = 该物种进过图鉴", "点空白处关闭", "已暂停模拟", "确认前可以随时返回",
    "已返回本轮", "进度没有任何变化", "结算完成", "新的纪元开始", "资源与突变已重置",
    "强化成功", "升级完成", "获得突变", "分化成功", "候选已生成",
    "本次选择的候选不会刷新", "条件不足", "资源不够或前置未满足", "当前阶段不能执行该操作",
    "目标无效", "前置条件未满足", "已经满级", "本轮奖励已经领取过", "操作未生效",
    "还没有可用的分化方向", "先通过突变满足前置条件", "点击生命体开始积累 ATP",
    "还没有可用的分化方向：先通过突变满足前置条件",
    "空格 采集 · 1/2/3 升级 · M 突变 · E 结算",
    "可立即分化", "条件已满足", "条件未满足", "先在突变里补前置",
    "本版已到最后阶段", "可以结束本轮，把积累换成 EP",
    "生物量已够，可分化为", "已抵达本版最后阶段，可以结算或继续积累",
    "常态海洋，", "后发生下一次扰动", "还有", "结束，低适应物种会掉种群",
    "个突变候选待选", "DNA 现有", "DNA 足够", "可以打开突变面板",
    "积累 DNA 以解锁主动突变", "图鉴已点亮", "物种",
    "距离下一次环境筛选", "环境信号", "选择你的下一段进化",
    "进化正在加速", "点击培养皿收集能量", "资源产出", "世界", "星系",
    "失败", "成功", "已解锁", "未解锁", "进度", "时间", "总计", "合计",
    "每轮", "首次", "最后", "继续", "退出", "返回", "打开", "查看", "收下",
    "生存", "适应", "进化", "遗产", "记忆", "宇宙", "生命", "能量",
    "进化商店 · 用 ATP 与生物量购买", "这里可以买到下一段进化",
    "生命体展台", "环境压力", "稳定", "波动", "剧烈", "轻度", "中度", "重度",
    "综合", "平均", "最大", "最小", "基础", "加成", "惩罚", "来源",
    "你已经走过", "尚未探索", "已收录", "未收录", "全部", "节点",
};

static void register_fixed_glyphs(void)
{
    for (int c = 32; c < 127; ++c) cp_push(c);
    for (int i = 0; i < UI_GLYPH_PART_COUNT; ++i) ui_font_register(UI_GLYPH_PARTS[i]);
}

static void register_ui_strings(void)
{
    for (unsigned i = 0; i < sizeof(UI_STATIC_STRINGS) / sizeof(UI_STATIC_STRINGS[0]); ++i) {
        ui_font_register(UI_STATIC_STRINGS[i]);
    }
    /* 符号与标点。▲▼◆ 用于涨跌与等级标记。 */
    ui_font_register("·×÷≥≤→←↑↓▲▼◆●○★☆—…‰％／（）【】「」『』、，。：；！？“”‘’");

    /* 第 13 版新增（数字单位 + 新增页面文案）。漏一个就会显示成 '?'，
     * 所以这里宁多勿少；正式回归以启动日志的 "UI font: missing U+xxxx" 为准。 */
    ui_font_register("万亿");
    ui_font_register("点击培养皿或按空格采集");
    ui_font_register("点击培养皿开始积累 ATP");
    ui_font_register("多样性 Lv.2 解锁");
    ui_font_register("等待主谱系");
    ui_font_register("进化树 · 星图");
    ui_font_register("一圈一个阶段，越外越晚；亮线是走过的亲缘，暗线是尚未探索的分支");
    ui_font_register("尚未获得");
    ui_font_register("采集 +");
    ui_font_register("永久");
    ui_font_register("环境适应");
    ui_font_register("阶段");
    ui_font_register("阶");
    ui_font_register("保留亲本为额外分支");
    ui_font_register("生物量里程碑");

    /* 第 15 版：情报面板三页签 + 底栏新闻条 + 结算进度条。
     * 这些串都是动态拼出来的话，揉成几句一次性登记，省得逐字漏。 */
    ui_font_register("情报纪录产出下一步");
    ui_font_register("本轮峰值生物量当前总种群已点亮物种预计结算");
    ui_font_register("最近 18 秒的存量变化（每 0.25 秒采样）");
    ui_font_register("正处在阶段宇宙的尺度开始向你展开演化还在缓慢积累");
    ui_font_register("距还有一段路已满档生物量里程碑下一档");
    ui_font_register("环境警报耐性低的物种正在掉种群海水平静后下一次扰动会抵达");
    ui_font_register("每秒每次点击培养皿已获得项突变是唯一能改写形态的资源");
    ui_font_register("个物种进化树上环正在发光从一枚细胞到宇宙尽头这是第次演化");
    ui_font_register("可分化为下一分支图鉴已点亮已抵达本版最后阶段可以结算或继续积累");
    ui_font_register("个突变候选待选现有足够可以打开突变面板键积累以解锁主动突变");
    ui_font_register("常态海洋后发生下一次扰动结束低适应物种会掉种群");
    ui_font_register("空格采集升级突变结算图鉴进化树商店页签");
    ui_font_register("成就解锁还没有可用的分化方向先通过满足前置条件");
    ui_font_register("第纪元文明已到正在改造整颗行星");

    /* 第 15 版补：Legacy 生涯档案 + 进化树图例 */
    ui_font_register("生涯档案最高纪元历史最佳最高生物量图鉴收录已点亮未探索当前路径");
    ui_font_register("生涯统计已点亮物种已收录突变渡过危机本轮时长成就达成");
}

void ui_font_init(void)
{
    register_fixed_glyphs();
    ua_register_font_strings();
    register_ui_strings();

    /* 单文件版把字体编进 exe：LoadFontFromMemory 拿到的是一块只读数据，
     * 码点表仍然由 ui_font_register 动态攒（与文件版完全一致）。
     * 普通构建 emb_get() 恒 false，走原来的 LoadFontEx(路径)。 */
    const unsigned char *emb = NULL;
    int emb_size = 0;
    if (emb_get(EMB_FONT, &emb, &emb_size)) {
        g_font = LoadFontFromMemory(".ttf", emb, emb_size, UI_FONT_BASE_SIZE, g_cp, g_cp_count);
    } else {
        g_font = LoadFontEx("assets/fonts/ui_zh.ttf", UI_FONT_BASE_SIZE, g_cp, g_cp_count);
    }
    if (g_font.texture.id == 0) {
        g_font = GetFontDefault();
        g_font_ready = false;
    } else {
        SetTextureFilter(g_font.texture, TEXTURE_FILTER_BILINEAR);
        g_font_ready = true;

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
    DrawTextEx(g_font, text, (Vector2){ x, y }, size, spacing, color);
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

void ui_text_shadow(const char *text, float x, float y, float size, Color color, Color shadow)
{
    ui_text(text, x + size * 0.05f, y + size * 0.06f, size, shadow);
    ui_text(text, x, y, size, color);
}

void ui_text_strong(const char *text, float x, float y, float size, Color color)
{
    ui_text(text, x, y, size, color);
    if (size >= 20.0f) {
        /* 半像素重画两次：在 4K(SS=3) 下相当于 1.5 设备像素的描边，笔画更厚 */
        ui_text(text, x + 0.55f, y, size, color);
        ui_text(text, x, y + 0.55f, size, color);
    }
}

void ui_text_center_strong(const char *text, float cx, float y, float size, Color color)
{
    float w = ui_text_w(text, size);
    ui_text_strong(text, cx - w * 0.5f, y, size, color);
}

/* ---------------- 颜色与缓动 ---------------- */

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

/* ---------------- 面板与控件 ---------------- */

static float roundness_of(float radius, Rectangle r)
{
    float m = (r.width < r.height ? r.width : r.height);
    if (m <= 0.0f) return 0.0f;
    float k = radius / m * 2.0f;
    return k > 1.0f ? 1.0f : k;
}

/* 面板 = 落影 + 外描边 + 内填充 + 顶部玻璃高光。
 * 落影只在足够大的面上画：给 20px 的 chip 也加影子会把界面糊成一团。 */
void ui_panel(Rectangle r, float radius, Color fill, Color border)
{
    if (radius <= 0.0f) {
        DrawRectangleRec(r, fill);
        if (border.a > 0) DrawRectangleLinesEx(r, W_HAIRLINE, border);
        return;
    }

    float k = roundness_of(radius, r);

    if (r.height >= 34.0f && fill.a >= 250) {
        DrawRectangleRounded((Rectangle){ r.x + 1.0f, r.y + 6.0f, r.width, r.height }, k, 10,
                             Fade(BLACK, 0.28f));
        DrawRectangleRounded((Rectangle){ r.x + 3.0f, r.y + 12.0f, r.width, r.height }, k, 10,
                             Fade(BLACK, 0.14f));
    }

    if (border.a == 0) {
        DrawRectangleRounded(r, k, 10, fill);
        return;
    }

    if (fill.a < 255) {
        /* 半透明面：外圈和内圈只能分开画，描边在 SS>1 时会细一档，仅用于玻璃层 */
        DrawRectangleRounded(r, k, 10, fill);
        DrawRectangleRoundedLinesEx(r, k, 10, W_HAIRLINE, border);
        return;
    }

    DrawRectangleRounded(r, k, 10, border);
    Rectangle in = { r.x + W_HAIRLINE, r.y + W_HAIRLINE,
                     r.width - 2.0f * W_HAIRLINE, r.height - 2.0f * W_HAIRLINE };
    if (in.width < 1.0f || in.height < 1.0f) return;

    float r_in = radius - W_HAIRLINE;
    if (r_in < 0.0f) r_in = 0.0f;
    DrawRectangleRounded(in, roundness_of(r_in, in), 10, fill);

    /* 玻璃高光：贴着上边缘的一条极淡亮线。只在 ≥34 的面上画。 */
    if (r.height >= 34.0f) {
        float inset = radius + 2.0f;
        if (r.width > inset * 2.0f + 8.0f) {
            DrawLineEx((Vector2){ r.x + inset, r.y + 1.5f },
                       (Vector2){ r.x + r.width - inset, r.y + 1.5f },
                       W_HAIRLINE, Fade(WHITE, 0.10f));
        }
    }
}

void ui_panel_soft(Rectangle r, float radius, Color fill)
{
    ui_panel(r, radius, fill, BLANK);
}

void ui_halo(Rectangle r, float radius, Color col, float alpha)
{
    for (int i = 3; i >= 1; --i) {
        float e = (float)i * 2.2f;
        Rectangle g = { r.x - e, r.y - e, r.width + e * 2.0f, r.height + e * 2.0f };
        DrawRectangleRounded(g, roundness_of(radius + e, g), 10, Fade(col, alpha / (float)(i * 3)));
    }
}

void ui_gradient(Rectangle r, Color top, Color bottom)
{
    DrawRectangleGradientV((int)r.x, (int)r.y, (int)r.width, (int)r.height, top, bottom);
}

void ui_glow(Vector2 c, float radius, Color col, float alpha)
{
    /* 叠加混合的柔光球：4 层同心，中心约 alpha、边缘归零。
     * 用加法而不是普通 alpha，暗底上才有“自己发光”的物理感。 */
    if (alpha <= 0.0f || radius <= 0.0f) return;
    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 4; i >= 1; --i) {
        float k = (float)i / 4.0f;
        DrawCircleV(c, radius * k, Fade(col, alpha * 0.10f));
    }
    EndBlendMode();
}

void ui_bar(Rectangle r, float t, Color fill, Color back)
{
    if (back.a == 0) back = COL_BAR_TRACK;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    DrawRectangleRounded(r, 1.0f, 8, back);
    if (t > 0.001f) {
        Rectangle f = r;
        f.width = r.width * t;
        if (f.width < r.height) f.width = r.height;
        DrawRectangleRounded(f, 1.0f, 8, fill);
        /* 高光：条子上沿一条亮线，像充能液面 */
        DrawRectangleRounded((Rectangle){ f.x + 1.0f, f.y + 1.0f, f.width - 2.0f, 1.5f },
                             0.5f, 4, Fade(WHITE, 0.35f));
    }
}

void ui_divider(float x1, float x2, float y, Color color)
{
    DrawLineEx((Vector2){ x1, y + 0.5f }, (Vector2){ x2, y + 0.5f }, W_HAIRLINE_AA, color);
}

/* ---------------- 数字格式 ----------------
 * 中文单位：万 / 亿 / 万亿。前两位有效数字，数值再大也不换行。 */

void ui_fmt_num(double v, char *out, int n)
{
    if (n <= 0) return;
    double a = fabs(v);
    double s = (v < 0.0) ? -1.0 : 1.0;

    if (a < 10.0) {
        snprintf(out, (size_t)n, "%.1f", v);
        return;
    }
    if (a < 10000.0) {
        snprintf(out, (size_t)n, "%.0f", v);
        return;
    }
    if (a < 1.0e8) {
        double x = a / 1.0e4;
        if (x < 10.0) snprintf(out, (size_t)n, "%.2f万", s * x);
        else snprintf(out, (size_t)n, "%.1f万", s * x);
        return;
    }
    if (a < 1.0e12) {
        double x = a / 1.0e8;
        if (x < 10.0) snprintf(out, (size_t)n, "%.2f亿", s * x);
        else snprintf(out, (size_t)n, "%.2f亿", s * x);
        return;
    }
    {
        double x = a / 1.0e12;
        if (x < 100.0) snprintf(out, (size_t)n, "%.1f万亿", s * x);
        else snprintf(out, (size_t)n, "%.0f万亿", s * x);
    }
}

void ui_fmt_rate(double v, char *out, int n)
{
    char tmp[40];
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

/* ---------------- 按钮 ---------------- */

void ui_btn_draw(UiButton *b, const char *title, const char *sub, bool hot)
{
    float dt = GetFrameTime();
    if (dt > 0.12f) dt = 0.12f;

    float target = (hot && b->enabled) ? 1.0f : 0.0f;
    b->hover += (target - b->hover) * fminf(1.0f, dt / 0.12f);

    Color fill, border;
    if (!b->enabled) {
        fill = COL_SURFACE_OFF;
        border = Fade(COL_LINE, 0.9f);
    } else {
        fill = ui_color_mix(COL_PANEL_HI, COL_PANEL_HOT, b->hover);
        border = ui_color_mix(Fade(COL_ACCENT, 0.30f), COL_ACCENT, b->hover);
    }

    Rectangle r = b->rect;
    r.y -= b->hover * 1.5f + b->press * 1.0f;

    if (hot && b->enabled) ui_halo(r, R_BTN, COL_ACCENT, 0.10f);
    ui_panel(r, R_BTN, fill, border);

    float cx = r.x + r.width * 0.5f;
    float cy = r.y + r.height * 0.5f;
    if (sub && sub[0]) {
        ui_text_center_strong(title, cx, cy - 13.0f, F_LABEL,
                              b->enabled ? COL_TEXT : COL_FAINT);
        ui_text_center(sub, cx, cy + 4.0f, F_BODY, b->enabled ? COL_DIM : COL_FAINT);
    } else {
        ui_text_center_strong(title, cx, cy - 9.0f, F_LABEL,
                              b->enabled ? COL_TEXT : COL_FAINT);
    }
}

void ui_tag(Rectangle r, const char *text, Color color)
{
    float w = ui_text_w(text, F_CAPTION) + 14.0f;
    Rectangle tag = { r.x + r.width - w - S_GAP, r.y + S_GAP, w, H_CHIP };
    Color bg = color;
    bg.a = 40;
    ui_panel(tag, R_CHIP(tag.height), bg, BLANK);
    ui_text_center(text, tag.x + tag.width * 0.5f, tag.y + 4.0f, F_CAPTION, color);
}

void ui_dot(float cx, float cy, float radius, Color color)
{
    DrawCircleV((Vector2){ cx, cy }, radius, color);
}
