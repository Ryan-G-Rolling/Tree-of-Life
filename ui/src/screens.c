/*
 * screens.c  ——  页面结构与绘制（Member C）
 *
 * 每个页面分两步：
 *   build_xxx()  只填矩形表（按钮在哪、是否可点）
 *   draw_xxx()   只画（用同一批矩形 + ui_adapter 提供的只读数据）
 *
 * 这样“命中判定”和“画在哪”永远一致，弹层天然压住底层。
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raylib.h"
#include "ui_internal.h"
#include "ui_adapter.h"
#include "environment.h"
#include "evolution.h"
#include "balance.h"
#include "atmosphere.h" /* 展台内部的 L1 悬液（atm_draw_live_in） */

/* ================= 通用小工具 ================= */

/* 上一帧的按钮表快照。**只为继承动画状态**（hover / press）：
 * screens_build 每帧重建矩形表，btn_add 里的 memset 会把它们清零，
 * 于是 ui_btn_draw 的插值永远从 0 开始、永远不会动。用 (id,target,option)
 * 而不是下标做键，弹层开关导致表顺序变化时才不会把动画串到别的按钮上。 */
static UiLayout s_prev;
static int codex_page = 0;
static int codex_last_tab = -1;
static int tree_page = 0;
void screens_debug_tree_page(int page) { tree_page = page<0?0:page>3?3:page; }

static UiButton *btn_add(UiLayout *L, UiButtonId id, Rectangle r, int target, int option, bool enabled)
{
    if (L->count >= MAX_UI_BUTTONS) return NULL;
    UiButton *b = &L->items[L->count++];
    float inherit_hover = 0.0f, inherit_press = 0.0f;
    for (int i = 0; i < s_prev.count; ++i) {
        const UiButton *p = &s_prev.items[i];
        /* 键 = (id, target, option) + 矩形左上角。
         * 必须带上矩形：同一个 id 可能同屏出现两次（底栏的“主动突变”与突变弹层里的
         * “生成候选”都是 UI_BTN_OPEN_MUTATION），只按三元组匹配会互相串动画。 */
        if (p->id == id && p->target_id == target && p->option == option &&
            p->rect.x == r.x && p->rect.y == r.y) {
            inherit_hover = p->hover;
            inherit_press = p->press;
            break;
        }
    }
    memset(b, 0, sizeof(*b));
    b->hover = inherit_hover;
    b->press = inherit_press;
    b->id = id;
    b->rect = r;
    b->target_id = target;
    b->option = option;
    b->enabled = enabled;
    return b;
}

/* 只读查找：返回非 const 指针是为了让按钮自己推进按下动画，不改任何游戏数据 */
static UiButton *btn_find(const UIState *S, UiButtonId id, int target)
{
    for (int i = 0; i < S->layout.count; ++i) {
        const UiButton *b = &S->layout.items[i];
        if (b->id == id && (target < 0 || b->target_id == target)) {
            return (UiButton *)&S->layout.items[i];
        }
    }
    return NULL;
}

static bool is_hot(const UIState *S, const UiButton *b)
{
    return b && S->hot >= 0 && (&S->layout.items[S->hot] == b);
}

/* 整卡即按钮的组件（升级卡 / Legacy 卡 / 弹层候选卡 / 点击区的圆）不走
 * ui_btn_draw，但必须走同一条 hover 曲线，否则一屏里两种 hover 手感。
 * 120ms 指数趋近（01 §8.4 例 2），返回值直接当 0..1 用。 */
static float btn_hover_step(UiButton *b, bool hot)
{
    if (!b) return 0.0f;
    float dt = GetFrameTime();
    if (dt > 0.12f) dt = 0.12f;
    float target = (hot && b->enabled) ? 1.0f : 0.0f;
    b->hover += (target - b->hover) * fminf(1.0f, dt / 0.12f);
    return b->hover;
}

/* hover 抬升量：0 → −1 逻辑 px，ease-out-cubic（01 §8.4 例 2）。
 * 与 ui_btn_draw 内部的曲线一致，保证“按钮上移”和“整卡上移”手感相同。 */
static float hover_lift(float hv)
{
    float u = hv < 0.0f ? 0.0f : (hv > 1.0f ? 1.0f : hv);
    float v = 1.0f - u;
    return -(1.0f - v * v * v);
}

static void fmt_mul(char *buf, int n, double v)
{
    snprintf(buf, (size_t)n, "×%.2f", v);
}

static void fmt_cost(char *buf, int n, double v)
{
    if (v < 10000.0) snprintf(buf, (size_t)n, "%.0f", v);
    else ui_fmt_num(v, buf, n);
}

/* 物种身份色：只做填充与色轨，永不做文字色（01 §2.4）。
 * 7 个色统一 L=0.340 / S≈0.50，强弱只由数值和形态表达，不用颜色暗示。 */
static Color ecology_color(int species_id)
{
    switch (species_id) {
    case SPECIES_PROTOCELL:            return COL_SP0;
    case SPECIES_CYANOBACTERIUM:       return COL_SP1;
    case SPECIES_CHEMOLITHOTROPH:      return COL_SP2;
    case SPECIES_ALGAL_EUKARYOTE:      return COL_SP3;
    case SPECIES_PROTOZOAN:            return COL_SP4;
    case SPECIES_MULTICELLULAR_COLONY: return COL_SP5;
    case SPECIES_EXTREMOPHILE_COLONY:  return COL_SP6;
    default: return COL_DIM;
    }
}

/* 带标题的卡片底板。全工程 40+ 处卡片都走这里，所以它的改动等于全局升级：
 *   色轨 4 → 3px（01 §9-2）、内边距 18 → 16（01 §6.1，唯一允许的坐标变更）、
 *   标题 COL_FAINT → COL_DIM + F_BODY 14（它是卡片的主语，不能是最低层级色）。 */
static void card_frame(Rectangle r, const char *title, const char *right_text, Color accent)
{
    ui_panel(r, R_CARD, COL_PANEL, COL_LINE);
    /* 色轨 4 → 3px，并在竖直方向内缩 8px：卡片圆角是 R_CARD=12，满高的色轨会在
     * 圆角处戳到卡片轮廓外面（4K 下能看到两个小凸起）。内缩后色轨的圆头正好落在
     * 左右两条直边的区间里。 */
    DrawRectangleRounded((Rectangle){ r.x, r.y + 8.0f, 3.0f, r.height - 16.0f }, 1.0f, 4, accent);

    if (title && title[0]) {
        ui_text(title, r.x + S_PAD, r.y + 12.0f, F_BODY, COL_DIM);
    }
    if (right_text && right_text[0]) {
        ui_text_right(right_text, r.x + r.width - S_PAD, r.y + 12.0f, F_BODY, COL_FAINT);
    }
}

/* ================= 主页面：顶部资源条 ================= */

static void draw_top_bar(const GameState *game)
{
    DrawRectangle(0, 0, UI_W, 88, COL_CHROME);
    DrawLineEx((Vector2){ 0, 87.5f }, (Vector2){ UI_W, 87.5f }, W_HAIRLINE, COL_LINE);

    double rates[3];
    ua_total_rates(game, rates);

    struct {
        const char *label;
        double value;
        double rate;
        Color color;
        bool has_rate;
    } pills[5] = {
        { "ATP",     game->run.atp,     rates[0], COL_ATP,     true  },
        { "生物量",  game->run.biomass, rates[1], COL_BIOMASS, true  },
        { "DNA",     game->run.dna,     rates[2], COL_DNA,     true  },
        { "理念", game->run.knowledge,evolution_knowledge_per_second(game),COL_RARE,true },
        { "EP",      (double)game->legacy.evolution_points, 0, COL_EP, false },
    };

    for (int i = 0; i < 5; ++i) {
        Rectangle p = { 20.0f + i * 165.0f, 16.0f, 155.0f, 56.0f };
        ui_panel(p, R_CARD, COL_PANEL, COL_LINE);
        DrawRectangleRounded((Rectangle){ p.x + 10.0f, p.y + 14.0f, 3.0f, 28.0f }, 1.0f, 4, pills[i].color);

        ui_text(pills[i].label, p.x + 24.0f, p.y + 10.0f, F_CAPTION, COL_FAINT);

        char num[32];
        if (!pills[i].has_rate) snprintf(num, sizeof(num), "%d", (int)pills[i].value);
        else ui_fmt_num(pills[i].value, num, (int)sizeof(num));
        ui_text(num, p.x + 24.0f, p.y + 26.0f, F_TITLE, COL_TEXT);

        if (pills[i].has_rate) {
            char rate[32];
            ui_fmt_rate(pills[i].rate, rate, (int)sizeof(rate));
            ui_text_right(rate, p.x + p.width - 14.0f, p.y + 34.0f, F_BODY,
                          pills[i].rate > 0.0 ? COL_DIM : COL_FAINT);
        } else {
            ui_text_right("永久", p.x + p.width - 14.0f, p.y + 34.0f, F_BODY, COL_FAINT);
        }
    }

    /* 右侧：纪元 / 阶段 */
    int main_species = game->run.active_species_count > 0 ? game->run.active_species[0].species_id : 0;
    int stage = ua_species_stage(main_species);
    char line[128];
    snprintf(line, sizeof(line), "第 %d 纪元", game->run.era);
    ui_text(line, 856.0f, 16.0f, F_LABEL, COL_TEXT);

    snprintf(line, sizeof(line), "阶段 %d · %s", stage, ua_stage_name(stage));
    ui_text(line, 856.0f, 38.0f, F_BODY, COL_DIM);

    char tbuf[32];
    ui_fmt_time(game->run.elapsed_time, tbuf, (int)sizeof(tbuf));
    ui_text_right(tbuf, 1260.0f, 18.0f, F_LABEL, COL_FAINT);

    /* 进化树 / 图鉴入口 */
    UIState *S = &g_ui;
    UiButton *tree = btn_find(S, UI_BTN_OPEN_TREE, -1);
    UiButton *codex = btn_find(S, UI_BTN_OPEN_CODEX, -1);
    if (tree) ui_btn_draw(tree, "进化树", NULL, is_hot(S, tree));
    if (codex) ui_btn_draw(codex, "图鉴", NULL, is_hot(S, codex));
}

/* ================= 主页面：点击区 ================= */

static void draw_click_panel(const GameState *game)
{
    UIState *S = &g_ui;
    Rectangle r = { 20.0f, 100.0f, 360.0f, 230.0f };
    card_frame(r, "生命体", NULL, COL_ACCENT);

    float cx = r.x + r.width * 0.5f;
    float cy = r.y + r.height * 0.5f + 12.0f;
    float pulse = 1.0f + 0.012f * sinf(S->time * 2.4f);

    /* 点击脉冲。必须用 DrawRing 而不是 DrawCircleLinesV：
     * 后者是 Z-UNSAFE 原语（线宽按设备像素算、不吃 zoom），SS=3 下这条涟漪会变成
     * 1/3 逻辑像素粗的发丝圈；DrawRing 是填充环，1 逻辑 px → SS 个物理 px。 */
    if (S->press_anim > 0.0f) {
        float k = 1.0f - S->press_anim;
        float rr = 58.0f + 46.0f * k;
        DrawRing((Vector2){ cx, cy }, rr - 0.5f, rr + 0.5f, 0.0f, 360.0f, 64,
                 Fade(COL_ACCENT, 0.45f * S->press_anim));
    }

    double click = ua_click_atp(game);
    char sub[64];
    if (click >= 0.0) {
        char num[32];
        ui_fmt_num(click, num, (int)sizeof(num));
        snprintf(sub, sizeof(sub), "+%s ATP", num);
    } else {
        snprintf(sub, sizeof(sub), "点击采集");
    }

    UiButton *b = btn_find(S, UI_BTN_CLICK, -1);
    bool hot = is_hot(S, b);

    /* 点击区画的是圆而不是矩形，用不了 ui_btn_draw，所以自己推进同一条 hover 曲线
     * （同样的 120ms 指数趋近）。b->hover 由 btn_add 跨帧继承，能真正插值起来。 */
    float hv = btn_hover_step(b, hot);

    /* 体色 = COL_ACCENT_SURFACE 打底，叠一层 COL_ACCENT 提亮（附录 A 行 188/189）：
     * 0.12 静息 → 0.26 hover。两个 token 之间插值，不再出现字面量。 */
    Color body = ui_color_mix(COL_ACCENT_SURFACE, COL_ACCENT, 0.12f + 0.14f * hv);
    if (S->flash > 0.02f) {
        body = ui_color_mix(body, COL_ACCENT, 0.35f * S->flash);
    }

    DrawCircleV((Vector2){ cx, cy }, 54.0f * pulse, body);
    DrawRing((Vector2){ cx, cy }, 54.0f * pulse, 58.0f * pulse, 0, 360, 64, Fade(COL_ACCENT, 0.85f));
    DrawCircleV((Vector2){ cx - 12.0f, cy - 14.0f }, 14.0f, Fade(COL_ACCENT, 0.35f));
    DrawCircleV((Vector2){ cx + 14.0f, cy + 10.0f }, 6.0f, Fade(COL_ACCENT, 0.25f));

    ui_text_center(sub, cx, cy + 74.0f, F_LABEL, COL_TEXT);
    ui_text_center("空格", cx, r.y + 40.0f, F_CAPTION, COL_FAINT);
}

/* ================= 主页面：单轮升级 ================= */

static void draw_upgrade_card(const GameState *game, int type)
{
    UIState *S = &g_ui;
    Rectangle r = { 20.0f, 342.0f + type * 114.0f, 360.0f, 104.0f };
    UiButton *b = btn_find(S, UI_BTN_UPGRADE, type);
    bool hot = is_hot(S, b);
    int level = game->run.run_upgrade_levels[type];
    float hv = btn_hover_step(b, hot);
    bool usable = b && b->enabled;

    /* 整卡即按钮：hover 走 COL_PANEL → COL_PANEL_HOT 插值 + COL_ACCENT 描边（01 §4.3） */
    Color fill = usable ? ui_color_mix(COL_PANEL, COL_PANEL_HOT, hv) : COL_PANEL;
    ui_panel(r, R_CARD, fill, (usable && hv > 0.02f) ? COL_ACCENT : COL_LINE);

    const char *title = ua_run_upgrade_title((RunUpgradeType)type);
    ui_text(title, r.x + S_PAD, r.y + 14.0f, F_LABEL, COL_TEXT);

    char lv[32];
    snprintf(lv, sizeof(lv), "Lv.%d", level);
    ui_text_right(lv, r.x + r.width - 100.0f, r.y + 16.0f, F_BODY, COL_DIM);

    char eff[96];
    ua_run_upgrade_effect(game, (RunUpgradeType)type, eff, (int)sizeof(eff));
    ui_text(eff, r.x + S_PAD, r.y + 40.0f, F_BODY, COL_DIM);

    /* 价格按钮 */
    char cost[48], num[32];
    double price = ua_run_upgrade_cost(game, (RunUpgradeType)type);
    ui_fmt_num(price, num, (int)sizeof(num));
    const char *unit = (type == UPGRADE_GENOME) ? "生物量" : "ATP";
    snprintf(cost, sizeof(cost), "%s %s", num, unit);
    ui_text(cost, r.x + S_PAD, r.y + 68.0f, F_BODY, usable ? COL_ACCENT : COL_FAINT);

    if (b) {
        char key[8];
        snprintf(key, sizeof(key), "%d", type + 1);
        ui_text_right(key, r.x + r.width - S_PAD, r.y + 70.0f, F_CAPTION, COL_FAINT);
    }
}

/* ================= 主页面：环境横幅 ================= */

static void draw_env_banner(const GameState *game)
{
    Rectangle r = { 396.0f, 100.0f, 524.0f, 54.0f };
    int env = game->run.current_environment_id;
    bool crisis = environment_is_crisis(env);
    Color accent = crisis ? COL_BAD : COL_ACCENT;

    ui_panel(r, R_CARD, crisis ? COL_BAD_SURFACE : COL_PANEL, crisis ? Fade(COL_BAD, 0.5f) : COL_LINE);

    char tbuf[32];
    ui_fmt_time(game->run.current_environment_id ? game->run.environment_time_left : game->run.event_interval_left, tbuf, (int)sizeof(tbuf));
    ui_text(tbuf, r.x + S_PAD, r.y + 16.0f, F_TITLE, accent);

    ui_text(ua_environment_name(env), r.x + 86.0f, r.y + 10.0f, F_LABEL, COL_TEXT);

    char desc[128];
    ua_environment_desc(env, desc, (int)sizeof(desc));
    ui_text(desc, r.x + 86.0f, r.y + 30.0f, F_CAPTION, COL_DIM);

    /* Adaptation Lv.3：下一事件预览 */
    if (environment_preview_available(game) && game->run.next_environment_id > 0) {
        char nb[64];
        snprintf(nb, sizeof(nb), "下一事件：%s", ua_environment_name(game->run.next_environment_id));
        ui_text_right(nb, r.x + r.width - S_PAD, r.y + 34.0f, F_CAPTION, COL_EP);
    }
}

/* ================= 主页面：生命体展示 ================= */

static void draw_creature(float cx, float cy, float radius, int species_id, float t, bool main_line)
{
    Color base = ecology_color(species_id);
    float breathe = 1.0f + 0.02f * sinf(t * 1.6f);

    DrawCircleV((Vector2){ cx, cy }, radius * 1.30f, Fade(base, 0.06f));
    DrawCircleV((Vector2){ cx, cy }, radius * 1.12f, Fade(base, 0.10f));

    /* 伪足：一圈随时间起伏的小圆，拼出不规则轮廓 */
    for (int i = 0; i < 9; ++i) {
        float a = t * 0.35f + (float)i * (2.0f * PI / 9.0f);
        float rr = radius * (0.86f + 0.10f * sinf(t * 1.3f + (float)i));
        DrawCircleV((Vector2){ cx + cosf(a) * rr * 0.72f, cy + sinf(a) * rr * 0.72f },
                    radius * 0.30f, Fade(base, 0.55f));
    }

    DrawCircleV((Vector2){ cx, cy }, radius * breathe, base);
    DrawCircleV((Vector2){ cx, cy }, radius * 0.94f, Fade(COL_TEXT, 0.08f));
    DrawRing((Vector2){ cx, cy }, radius * 0.97f, radius * 1.02f, 0, 360, 72, Fade(COL_TEXT, 0.35f));

    /* 细胞核 */
    float nx = cx + cosf(t * 0.6f) * radius * 0.12f;
    float ny = cy + sinf(t * 0.5f) * radius * 0.10f;
    DrawCircleV((Vector2){ nx, ny }, radius * 0.32f, Fade(COL_BG_DEEP, 0.55f));
    DrawCircleV((Vector2){ nx, ny }, radius * 0.22f, Fade(COL_TEXT, 0.30f));

    /* 细胞器 */
    for (int i = 0; i < 5; ++i) {
        float a = t * 0.7f + (float)i * 1.25f;
        float rr = radius * (0.50f + 0.10f * (float)(i % 3));
        DrawCircleV((Vector2){ cx + cosf(a) * rr, cy + sinf(a) * rr }, radius * 0.055f + 1.5f,
                    Fade(COL_TEXT, 0.55f));
    }

    if (main_line) {
        DrawRing((Vector2){ cx, cy }, radius * 1.06f, radius * 1.08f, 0, 360, 72, Fade(COL_ACCENT, 0.35f));
    }
}

static void draw_stage(const GameState *game)
{
    UIState *S = &g_ui;
    Rectangle r = { 396.0f, 166.0f, 524.0f, 300.0f };
    /* 展台 = COL_STAGE 凹陷面。它和 COL_PANEL 的 CR 只有 1.08（< 1.10 门槛），
     * 所以**必须**保留 COL_LINE 描边，否则展台会和卡片糊成一片（01 §2.5）。 */
    ui_panel(r, R_CARD, COL_STAGE, COL_LINE);

    /* L3：展台内部的悬液与刻度。必须在不透明的 COL_STAGE 填完之后调（否则被盖掉），
     * 且传**未经内缩**的 r —— 内部靠烘焙时调用同一个 ui_panel 保证子区域 blit 恒等，
     * 外面一内缩就会把展台圆角描边切掉几个像素（atmosphere.h 里 atm_draw_live_in 的契约）。
     * 主界面不调全屏版 atm_draw_live()：它会被这块不透明展台整块盖住，白烧 12 个调用。 */
    atm_draw_live_in(r);

    /* 【ENG-04 ①】这里原本是一段 18 次迭代的上浮气泡循环，y 坐标每帧按
     * 时间取模上移、颜色是 ACCENT 的 10% 透明度。§6.5 判据：悬浮物必须
     * "位置固定、只呼吸不位移"，上浮语义（暗示流体在流动）已整段删除。
     *
     * **6 粒固定微尘不在这里画**：它们由 `atm_draw_live_in(r)`（上一行）末尾的
     * `atm_dust(ATM_DUST_STAGE, 6, t)` 负责，坐标/半径/α 呼吸都在
     * src/atmosphere.c 的「6 粒微尘」一节。同一份东西写两遍就会出现 12 个点，
     * 所以本文件只保留"删掉"这一个动作，不再持有微尘数据。 */

    if (game->run.active_species_count <= 0) {
        ui_text_center("等待初始化", r.x + r.width * 0.5f, r.y + r.height * 0.5f, F_LABEL, COL_DIM);
        return;
    }

    bool two = game->run.active_species_count >= 2;
    float cx = two ? r.x + 200.0f : r.x + r.width * 0.5f;
    float cy = r.y + 158.0f;
    float radius = two ? 74.0f : 96.0f;

    draw_creature(cx, cy, radius, game->run.active_species[0].species_id, S->time, true);
    ui_text_center(ua_species_name(game->run.active_species[0].species_id), cx, cy + radius + 26.0f, F_LABEL, COL_TEXT);
    ui_text_center("主谱系", cx, cy + radius + 48.0f, F_CAPTION, COL_ACCENT);

    if (two) {
        float cx2 = r.x + 388.0f;
        draw_creature(cx2, cy, 58.0f, game->run.active_species[1].species_id, S->time + 1.7f, false);
        ui_text_center(ua_species_name(game->run.active_species[1].species_id), cx2, cy + 58.0f + 26.0f, F_LABEL, COL_TEXT);
        ui_text_center("额外分支", cx2, cy + 58.0f + 46.0f, F_CAPTION, COL_DIM);
    } else {
        /* 第二槽未解锁：画一个虚线空槽 */
        Rectangle slot = { r.x + 366.0f, r.y + 92.0f, 132.0f, 132.0f };
        ui_panel(slot, R_CARD, COL_SURFACE_OFF, Fade(COL_LINE, 0.8f));
        ui_text_center("第二物种槽", slot.x + slot.width * 0.5f, slot.y + 48.0f, F_BODY, COL_FAINT);
        ui_text_center("多样性 Lv.2", slot.x + slot.width * 0.5f, slot.y + 68.0f, F_CAPTION, COL_FAINT);
    }
}

/* ================= 主页面：纪元进度 ================= */

static void draw_era_progress(const GameState *game)
{
    Rectangle r = { 396.0f, 478.0f, 524.0f, 92.0f };
    card_frame(r, "生物量里程碑", NULL, COL_BIOMASS);

    int level = game->run.highest_biomass_milestone;
    double cur = game->run.max_biomass;
    double next = ua_milestone_threshold(level + 1);
    double floor_v = ua_milestone_threshold(level);

    float t = 0.0f;
    if(level >= BIOMASS_MILESTONE_COUNT) t=1.0f;
    else if (next > floor_v) t = (float)((cur - floor_v) / (next - floor_v));
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    char left[64], right[32], num[32];
    ui_fmt_num(cur, num, (int)sizeof(num));
    snprintf(left, sizeof(left), "B%d · %s", level, num);
    ui_fmt_num(next, right, (int)sizeof(right));

    ui_text(left, r.x + S_PAD, r.y + 38.0f, F_BODY, COL_TEXT);
    ui_text_right(right, r.x + r.width - S_PAD, r.y + 38.0f, F_BODY, COL_DIM);

    ui_bar((Rectangle){ r.x + S_PAD, r.y + 62.0f, r.width - S_PAD * 2.0f, 8.0f }, t, COL_BIOMASS, COL_BAR_TRACK);
}

/* ================= 主页面：物种卡 ================= */

static void draw_species_card(const GameState *game, int slot)
{
    int card_slot=slot;
    if(slot==1 && game->run.active_species_count>2) slot=1+(g_ui.branch_view%2);
    Rectangle r = { 936.0f, 100.0f + card_slot * 208.0f, 324.0f, 196.0f };
    bool exists = slot < game->run.active_species_count;

    if (!exists) {
        /* 未解锁卡 = COL_SURFACE_OFF（比 COL_PANEL 更暗一档），明确表示“这里没有活的东西” */
        ui_panel(r, R_CARD, COL_SURFACE_OFF, Fade(COL_LINE, 0.7f));
        ui_text_center("空置物种槽", r.x + r.width * 0.5f, r.y + 74.0f, F_LABEL, COL_FAINT);
        ui_text_center(slot == 0 ? "等待主谱系" : "多样性 Lv.2 解锁", r.x + r.width * 0.5f, r.y + 98.0f, F_CAPTION, COL_FAINT);
        return;
    }

    const SpeciesState *sp = &game->run.active_species[slot];
    int id = sp->species_id;
    Color accent = ecology_color(id);
    card_frame(r, ua_species_name(id), sp->is_main_lineage ? "主谱系" : "额外分支 · Tab 切换", accent);

    char line[128];
    snprintf(line, sizeof(line), "%s · 阶段 %d", ua_ecology_name(id), ua_species_stage(id));
    ui_text(line, r.x + S_PAD, r.y + 36.0f, F_CAPTION, COL_DIM);

    SpeciesRates rates;
    ua_species_rates(game, slot, &rates);

    /* 种群 */
    char num[32];
    ui_fmt_num(sp->population, num, (int)sizeof(num));
    snprintf(line, sizeof(line), "种群 %s / %.0f", num, rates.final_population_cap);
    ui_text(line, r.x + S_PAD, r.y + 58.0f, F_BODY, COL_TEXT);

    float t = rates.final_population_cap > 0.0 ? (float)(sp->population / rates.final_population_cap) : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    /* 槽底恒为 COL_BAR_TRACK（01 §2.2 使用禁忌：不可作面板色） */
    ui_bar((Rectangle){ r.x + S_PAD, r.y + 78.0f, r.width - S_PAD * 2.0f, 6.0f }, t, accent, COL_BAR_TRACK);

    /* 三资源产率。位置固定 ATP→生物量→DNA（冗余编码，见 01 §2.3 色盲注意），
     * 只在 12px 标签 / 14px 数值上收敛字号，不动这一行的绝对 x（D5 不在本轮范围）。 */
    char a[24], b[24], c[24];
    ui_fmt_num(rates.atp_per_second, a, (int)sizeof(a));
    ui_fmt_num(rates.biomass_per_second, b, (int)sizeof(b));
    ui_fmt_num(rates.dna_per_second, c, (int)sizeof(c));
    float y = r.y + 96.0f;
    ui_text("ATP", r.x + S_PAD, y, F_CAPTION, COL_FAINT);
    ui_text(a, r.x + 48.0f, y - 1.0f, F_BODY, COL_ATP);
    ui_text("生物量", r.x + 106.0f, y, F_CAPTION, COL_FAINT);
    ui_text(b, r.x + 152.0f, y - 1.0f, F_BODY, COL_BIOMASS);
    ui_text("DNA", r.x + 206.0f, y, F_CAPTION, COL_FAINT);
    ui_text(c, r.x + 240.0f, y - 1.0f, F_BODY, COL_DNA);

    /* 适应度徽标 */
    char mul[24];
    fmt_mul(mul, (int)sizeof(mul), rates.environment_multiplier);
    Color mul_color = rates.environment_multiplier > 1.001 ? COL_GOOD
                    : (rates.environment_multiplier < 0.999 ? COL_BAD : COL_DIM);
    ui_text("环境适应", r.x + S_PAD, r.y + 120.0f, F_CAPTION, COL_FAINT);
    ui_text(mul, r.x + 78.0f, r.y + 117.0f, F_LABEL, mul_color);
    if (rates.synergy_active) {
        ui_text("协同 +10%", r.x + 168.0f, r.y + 119.0f, F_CAPTION, COL_EP);
    }

    /* 突变图标：chip 高度统一 H_CHIP=20，文字 F_CAPTION(12) 垂直居中于 chip.y+4 */
    ui_text("突变", r.x + S_PAD, r.y + 146.0f, F_CAPTION, COL_FAINT);
    int shown = 0;
    float chip_x = r.x + 54.0f;
    for (int m = 0; m < MUTATION_COUNT && shown < 5; ++m) {
        if (!sp->mutations[m]) continue;
        const char *name = ua_mutation_name(m);
        float w = ui_text_w(name, F_CAPTION) + 14.0f;
        if (chip_x + w > r.x + r.width - S_SAFE) break;
        Rectangle chip = { chip_x, r.y + 142.0f, w, H_CHIP };
        ui_panel(chip, R_CHIP(chip.height), Fade(COL_DNA, 0.12f), BLANK);
        ui_text_center(name, chip.x + chip.width * 0.5f, chip.y + 4.0f, F_CAPTION, COL_DNA);
        chip_x += w + 6.0f;
        shown++;
    }
    if (shown == 0) {
        ui_text("尚无突变", r.x + 54.0f, r.y + 146.0f, F_CAPTION, COL_FAINT);
    }

    /* 适应度解释：M2 要求界面能说明“为什么是这个倍率” */
    char why[192];
    ua_fitness_breakdown(game, slot, why, (int)sizeof(why));
    ui_text(why, r.x + S_PAD, r.y + 170.0f, F_CAPTION, COL_DIM);
}

/* ================= 主页面：右下角统计 ================= */

static void draw_run_stats(const GameState *game)
{
    Rectangle r = { 936.0f, 516.0f, 324.0f, 208.0f };
    card_frame(r, "本轮统计", NULL, COL_DNA);

    int counts[5];
    ua_era_breakdown(game, counts);
    const char *labels[5] = { "阶段", "突变", "物种", "危机", "里程碑" };

    for (int i = 0; i < 5; ++i) {
        float y = r.y + 46.0f + i * 26.0f;
        ui_text(labels[i], r.x + S_PAD, y, F_CAPTION, COL_DIM);
        char v[16];
        if (i == 4) snprintf(v, sizeof(v), "B%d", counts[i]);
        else snprintf(v, sizeof(v), "%d", counts[i]);
        ui_text_right(v, r.x + 110.0f, y, F_BODY, COL_TEXT);
    }

    ui_divider(r.x + 130.0f, r.x + r.width - S_PAD, r.y + 40.0f, COL_LINE);

    /* 结算预览 */
    EraReward reward;
    ua_era_reward(game, &reward);
    char total[32];
    snprintf(total, sizeof(total), "%d EP", reward.total_points);
    ui_text("确认结算可得", r.x + 148.0f, r.y + 66.0f, F_CAPTION, COL_DIM);
    ui_text(total, r.x + 148.0f, r.y + 86.0f, F_DISPLAY, COL_EP);

    /* 公式：F_CAPTION 下限 12（01 §3.2 硬性结论 3：10 一律升 12）。
     * 原串 "EP = 2E + M/3 + S/2 + min(C,6) + B" 在 12px 下宽约 186 逻辑 px，
     * 而这一行只有 176px 可用（x 由 148 起、卡片内边距 16 收边），会越出卡片描边。
     * 运算符两侧的空格是排版装饰，去掉后 25 字符约 108px，语义一字不少。
     * 中文/拉丁之间仍然保留半角空格（§3.4 T-2 只约束中英混排）。 */
    char hint[96];
    snprintf(hint, sizeof(hint), "EP=2E+M/3+S/2+min(C,6)+B");
    ui_text(hint, r.x + 148.0f, r.y + 124.0f, F_CAPTION, COL_FAINT);
    ui_text("主动结束不会丢失图鉴", r.x + 148.0f, r.y + 142.0f, F_CAPTION, COL_FAINT);
}

/* ================= 主页面：下一步提示 ================= */

static void draw_next_step(const GameState *game)
{
    Rectangle r = { 396.0f, 586.0f, 524.0f, 138.0f };
    card_frame(r, "下一步", NULL, COL_ACCENT);

    float y = r.y + 44.0f;

    /* 1. 分化瓶颈 */
    int children[4];
    int n = ua_speciation_children(game, children, 4);
    int ready = -1;
    for (int i = 0; i < n; ++i) {
        if (ua_speciation_check(game, children[i]) == ACTION_OK) { ready = children[i]; break; }
    }

    DrawCircleV((Vector2){ r.x + 22.0f, y + 7.0f }, 3.0f, COL_ACCENT);
    if (ready >= 0) {
        char line[160], num[32];
        fmt_cost(num, (int)sizeof(num), ua_speciation_cost(game, ready));
        snprintf(line, sizeof(line), "生物量已够，可分化为 %s（%s）", ua_species_name(ready), num);
        ui_text(line, r.x + 34.0f, y, F_BODY, COL_TEXT);
    } else if (n > 0) {
        char req[160];
        ua_speciation_requirement(children[0], req, (int)sizeof(req));
        ui_text(req, r.x + 34.0f, y, F_BODY, COL_DIM);
    } else {
        ui_text("已抵达本版最后阶段，可以结算或继续积累", r.x + 34.0f, y, F_BODY, COL_DIM);
    }

    /* 2. 环境状态 */
    y += 26.0f;
    DrawCircleV((Vector2){ r.x + 22.0f, y + 7.0f }, 3.0f, game->run.current_environment_id ? COL_BAD : COL_DIM);
    char tbuf[32];
    ui_fmt_time(game->run.current_environment_id ? game->run.environment_time_left : game->run.event_interval_left, tbuf, (int)sizeof(tbuf));
    if (game->run.current_environment_id) {
        char line[160];
        snprintf(line, sizeof(line), "%s 还剩 %s，低适应物种会掉种群", ua_environment_name(game->run.current_environment_id), tbuf);
        ui_text(line, r.x + 34.0f, y, F_BODY, COL_DIM);
    } else {
        char line[160];
        snprintf(line, sizeof(line), "常态环境，%s 后进入下一次事件", tbuf);
        ui_text(line, r.x + 34.0f, y, F_BODY, COL_DIM);
    }

    /* 3. 突变候选 */
    y += 26.0f;
    if (game->run.mutation_offer.active) {
        DrawCircleV((Vector2){ r.x + 22.0f, y + 7.0f }, 3.0f, COL_DNA);
        char line[160], num[32];
        ui_fmt_num(game->run.dna, num, (int)sizeof(num));
        snprintf(line, sizeof(line), "%d 个突变候选待选，DNA 现有 %s", game->run.mutation_offer.choice_count, num);
        ui_text(line, r.x + 34.0f, y, F_BODY, COL_DIM);
    } else {
        DrawCircleV((Vector2){ r.x + 22.0f, y + 7.0f }, 3.0f, COL_DNA);
        char line[160];
        if (game->run.dna >= 12.0) snprintf(line, sizeof(line), "DNA 足够，可以打开突变面板");
        else snprintf(line, sizeof(line), "积累 DNA 以解锁主动突变");
        ui_text(line, r.x + 34.0f, y, F_BODY, COL_DIM);
    }
}

/* ================= 主页面：底部动作条 ================= */

static void draw_bottom_bar(const GameState *game)
{
    UIState *S = &g_ui;
    DrawRectangle(0, 736, UI_W, 64, COL_CHROME);
    DrawLineEx((Vector2){ 0, 736.5f }, (Vector2){ 1280, 736.5f }, W_HAIRLINE, COL_LINE);

    UiButton *mut = btn_find(S, UI_BTN_OPEN_MUTATION, -1);
    UiButton *spe = btn_find(S, UI_BTN_OPEN_SPECIATION, -1);
    UiButton *end = btn_find(S, UI_BTN_OPEN_END_ERA, -1);

    if (mut) {
        char sub[48];
        if (game->run.mutation_offer.active) {
            snprintf(sub, sizeof(sub), "%d 个候选", game->run.mutation_offer.choice_count);
        } else {
            snprintf(sub, sizeof(sub), "消耗 DNA");
        }
        ui_btn_draw(mut, "主动突变", sub, is_hot(S, mut));
    }
    if (spe) ui_btn_draw(spe, "物种分化", "消耗生物量", is_hot(S, spe));
    if (end) ui_btn_draw(end, "结束本轮演化", "预览 Evolution Points", is_hot(S, end));

    UiButton *cap=btn_find(S,UI_BTN_CAPACITY,-1), *res=btn_find(S,UI_BTN_RESERVE,-1), *tech=btn_find(S,UI_BTN_TECH,-1);
    char a[80],b[80],c[80];
    snprintf(a,sizeof a,"%.0f ATP + %.0f 生物量",evolution_capacity_atp_cost(game),evolution_capacity_biomass_cost(game));
    snprintf(b,sizeof b,"%.0f ATP · Lv.%d",evolution_reserve_cost(game),game->run.reserve_levels);
    const SubUpgradeDef *next=evolution_sub_upgrade_next(game,0);
    snprintf(c,sizeof c,"%.0f 理念",next?evolution_sub_upgrade_cost(next):0.0);
    if(cap) ui_btn_draw(cap,"容量扩张",a,is_hot(S,cap));
    if(res) ui_btn_draw(res,"应急储备",b,is_hot(S,res));
    if(tech) ui_btn_draw(tech,next?next->name:"科技",next?c:"高阶物种解锁",is_hot(S,tech));
}

/* ================= 主页面：布局 ================= */

static void build_main(UiLayout *L, const GameState *game, const UIState *S)
{
    (void)S;
    /* 点击生命体 */
    btn_add(L, UI_BTN_CLICK, (Rectangle){ 116.0f, 216.0f, 168.0f, 168.0f }, 0, 0, true);

    /* 三类升级 */
    for (int i = 0; i < 3; ++i) {
        double price = ua_run_upgrade_cost(game, (RunUpgradeType)i);
        bool affordable = (i == UPGRADE_GENOME) ? (game->run.biomass >= price) : (game->run.atp >= price);
        btn_add(L, UI_BTN_UPGRADE, (Rectangle){ 20.0f, 342.0f + i * 114.0f, 360.0f, 104.0f }, i, 0, affordable);
    }

    /* 顶部入口：H_SECONDARY 34（原 30）。**下沿保持 82 不动**（原 52+30），
     * 只向上长 4px → y = 82 − 34 = 48。
     * 往下长的做法会顶到 87.5 的分割线（只剩 3.5px）；往上长则与“07:11”时间行
     * （右对齐 1260，占 18–34）之间仍留 14px，是唯一不压线的方向。 */
    btn_add(L, UI_BTN_OPEN_TREE, (Rectangle){ 1044.0f, 48.0f, 100.0f, H_SECONDARY }, 0, 0, true);
    btn_add(L, UI_BTN_OPEN_CODEX, (Rectangle){ 1152.0f, 48.0f, 108.0f, H_SECONDARY }, 0, 0, true);

    /* 底部动作：H_PRIMARY 44（原 40）。底栏 736–800 的中心是 768，
     * 所以 y = 768 − 44/2 = 746，上下各留 10。 */
    btn_add(L,UI_BTN_CAPACITY,(Rectangle){600,746,214,H_PRIMARY},0,0,game->run.biomass>=evolution_capacity_biomass_cost(game)&&game->run.atp>=evolution_capacity_atp_cost(game));
    btn_add(L,UI_BTN_RESERVE,(Rectangle){826,746,180,H_PRIMARY},0,0,game->run.reserve_levels<RESERVE_MAX_LEVEL&&game->run.atp>=evolution_reserve_cost(game));
    const SubUpgradeDef *tech=evolution_sub_upgrade_next(game,0);
    btn_add(L,UI_BTN_TECH,(Rectangle){1018,746,242,H_PRIMARY},0,0,tech&&game->run.knowledge>=evolution_sub_upgrade_cost(tech));
    int species_count = game->run.active_species_count;
    bool can_speciate = false;
    int children[4];
    int n = ua_speciation_children(game, children, 4);
    for (int i = 0; i < n; ++i) {
        if (ua_speciation_check(game, children[i]) == ACTION_OK) { can_speciate = true; break; }
    }
    (void)species_count;
    (void)can_speciate;

    btn_add(L, UI_BTN_OPEN_MUTATION, (Rectangle){ 20.0f, 746.0f, 180.0f, H_PRIMARY }, 0, 0, true);
    btn_add(L, UI_BTN_OPEN_SPECIATION, (Rectangle){ 212.0f, 746.0f, 180.0f, H_PRIMARY }, 0, 0, n > 0);
    btn_add(L, UI_BTN_OPEN_END_ERA, (Rectangle){ 404.0f, 746.0f, 180.0f, H_PRIMARY }, 0, 0, true);
}

static void draw_main(const GameState *game, const UIState *state, const ActionResult *last)
{
    (void)last;
    (void)state;

    draw_top_bar(game);
    draw_click_panel(game);
    for (int i = 0; i < 3; ++i) draw_upgrade_card(game, i);
    draw_env_banner(game);
    draw_stage(game);
    draw_era_progress(game);
    draw_species_card(game, 0);
    draw_species_card(game, 1);
    draw_run_stats(game);
    draw_next_step(game);
    draw_bottom_bar(game);
}

/* ================= End of Era 页面 ================= */

static void build_end_era(UiLayout *L, const GameState *game, const UIState *S)
{
    (void)S;
    /* H_PRIMARY 44（原 52）。中心由 638 保持不变 → y = 638 − 22 = 616 */
    btn_add(L, UI_BTN_CANCEL_END_ERA, (Rectangle){ 316.0f, 616.0f, 300.0f, H_PRIMARY }, 0, 0, true);
    btn_add(L, UI_BTN_CONFIRM_END_ERA, (Rectangle){ 664.0f, 616.0f, 300.0f, H_PRIMARY }, 0, 0, !game->run.reward_claimed);
}

static void draw_end_era(const GameState *game, const UIState *S)
{
    /* 全屏页底色：End of Era 是"终止"，用最暗一档 COL_BG_DEEP（也是全屏清屏色）；
     * Legacy 是"新一轮起点"，用 COL_BG 稍亮一档。两个全屏页**故意差一档**（01 §2.2
     * + 附录 A 行 659/748 的裁定）。 */
    DrawRectangle(0, 0, UI_W, UI_H, COL_BG_DEEP);

    /* L1/L3 氛围层：全屏页唯一"该看得见背景"的地方（02 §1.4：EoE 可见背景面积约 30%）。
     * 必须在页底填充之后、任何面板之前 —— 这一页没有不透明的展台面，
     * 所以整屏可见；放在面板后面就是白烧。主界面反过来不要调它（会被展台整块盖住）。 */
    atm_draw_live();

    Rectangle panel = { 264.0f, 75.0f, 752.0f, 650.0f };
    ui_panel(panel, R_PANEL, COL_PANEL, COL_LINE);

    ui_text("结束本轮演化", panel.x + 40.0f, panel.y + 34.0f, F_DISPLAY, COL_TEXT);
    char sub[160];
    char tbuf[32];
    ui_fmt_time(game->run.elapsed_time, tbuf, (int)sizeof(tbuf));
    snprintf(sub, sizeof(sub), "本轮持续 %s · 第 %d 纪元 · 确认后本轮冻结，Evolution Points 只入账一次", tbuf, game->run.era);
    ui_text(sub, panel.x + 40.0f, panel.y + 72.0f, F_BODY, COL_DIM);

    ui_divider(panel.x + 40.0f, panel.x + panel.width - 40.0f, panel.y + 100.0f, COL_LINE);

    int c[5];
    ua_era_breakdown(game, c);
    EraReward reward;
    ua_era_reward(game, &reward);

    struct {
        const char *label;
        const char *formula;
        int count;
        int points;
        Color color;
    } rows[5] = {
        { "主要进化阶段", "2 × E",              c[0], reward.era_points,      COL_ACCENT },
        { "解锁突变",     "floor(M / 3)",       c[1], reward.mutation_points, COL_DNA },
        { "发现物种",     "floor(S / 2)",       c[2], reward.species_points,  COL_BIOMASS },
        { "度过危机",     "min(C, 3)",          c[3], reward.crisis_points,   COL_RARE },
        { "生物量里程碑", "B",                  c[4], reward.biomass_points,  COL_ATP },
    };

    for (int i = 0; i < 5; ++i) {
        float y = panel.y + 120.0f + i * 62.0f;
        /* 弹层内卡片用 COL_PANEL_HI 抬升面 + R_BTN 圆角 */
        ui_panel((Rectangle){ panel.x + 40.0f, y, panel.width - 80.0f, 52.0f }, R_BTN,
                 COL_PANEL_HI, Fade(COL_LINE, 0.6f));
        DrawCircleV((Vector2){ panel.x + 66.0f, y + 26.0f }, 4.0f, rows[i].color);
        ui_text(rows[i].label, panel.x + 82.0f, y + 17.0f, F_LABEL, COL_TEXT);

        /* D7：公式是“理解结算”的关键信息，不能放在最低层级色上。
         * 旧版 COL_FAINT 12px（层级判断错，不是对比度不达标）→ COL_DIM 14px。 */
        char mid[64];
        snprintf(mid, sizeof(mid), "%s = %d", rows[i].formula, rows[i].count);
        ui_text(mid, panel.x + 260.0f, y + 19.0f, F_BODY, COL_DIM);

        char pts[32];
        snprintf(pts, sizeof(pts), "+%d", rows[i].points);
        ui_text_right(pts, panel.x + panel.width - 60.0f, y + 14.0f, F_TITLE, rows[i].color);
    }

    /* 总计 */
    Rectangle total = { panel.x + 40.0f, panel.y + 438.0f, panel.width - 80.0f, 84.0f };
    /* EP 容器与 COL_PANEL 的 CR 只有 1.02 → 必须保留 Fade(COL_EP) 描边（01 §2.5） */
    ui_panel(total, R_CARD, COL_EP_SURFACE, Fade(COL_EP, 0.45f));
    ui_text("本轮 Evolution Points", total.x + 22.0f, total.y + 20.0f, F_LABEL, COL_TEXT);
    char note[96];
    snprintf(note, sizeof(note), "现有 %d EP · 未使用部分永久保留", game->legacy.evolution_points);
    ui_text(note, total.x + 22.0f, total.y + 46.0f, F_CAPTION, COL_DIM);

    char tot[32];
    snprintf(tot, sizeof(tot), "%d", reward.total_points);
    ui_text_right(tot, total.x + total.width - 26.0f, total.y + 18.0f, F_HERO, COL_EP);

    if (game->run.reward_claimed) {
        ui_text_center("本轮奖励已领取，只能开始新轮", panel.x + panel.width * 0.5f, 690.0f, F_BODY, COL_BAD);
    } else {
        ui_text_center("返回本轮继续成长不会损失任何进度", panel.x + panel.width * 0.5f, 690.0f, F_BODY, COL_FAINT);
    }

    UiButton *cancel = btn_find(S, UI_BTN_CANCEL_END_ERA, -1);
    UiButton *confirm = btn_find(S, UI_BTN_CONFIRM_END_ERA, -1);
    if (cancel) ui_btn_draw(cancel, "返回本轮", "继续成长", is_hot(S, cancel));
    if (confirm) ui_btn_draw(confirm, "确认结束", "结算并进入 Legacy", is_hot(S, confirm));
}

/* ================= Legacy 页面 ================= */

static void build_legacy(UiLayout *L, const GameState *game, const UIState *S)
{
    (void)S;
    for (int i = 0; i < 4; ++i) {
        Rectangle r = { 60.0f + (i % 2) * 600.0f, 132.0f + (i / 2) * 232.0f, 560.0f, 212.0f };
        bool maxed = game->legacy.levels[i] >= LEGACY_MAX_LEVEL;
        bool afford = game->legacy.evolution_points >= ua_legacy_cost(i, game->legacy.levels[i]);
        btn_add(L, UI_BTN_BUY_LEGACY, r, i, 0, !maxed && afford);
    }
    /* H_PRIMARY 44（原 58）。中心由 669 保持不变 → y = 669 − 22 = 647 */
    btn_add(L, UI_BTN_START_NEW_RUN, (Rectangle){ 480.0f, 647.0f, 320.0f, H_PRIMARY }, 0, 0, true);
}

static void draw_legacy(const GameState *game, const UIState *S)
{
    DrawRectangle(0, 0, UI_W, UI_H, COL_BG);

    /* L1/L3 氛围层（同 draw_end_era 的理由）。顺序必须在页底填充之后、卡片之前。 */
    atm_draw_live();

    ui_text("Evolution Legacy", 60.0f, 40.0f, F_DISPLAY, COL_TEXT);
    ui_text("把上一轮的成果投进永久强化，新轮从原始生命重新演化", 60.0f, 78.0f, F_BODY, COL_DIM);

    /* EP 余额 */
    Rectangle bal = { 960.0f, 34.0f, 260.0f, 62.0f };
    ui_panel(bal, R_CARD, COL_EP_SURFACE, Fade(COL_EP, 0.5f));
    ui_text("Evolution Points", bal.x + S_PAD, bal.y + 10.0f, F_CAPTION, COL_DIM);
    char ep[32];
    snprintf(ep, sizeof(ep), "%d", game->legacy.evolution_points);
    ui_text_right(ep, bal.x + bal.width - S_PAD, bal.y + 22.0f, F_DISPLAY, COL_EP);

    for (int i = 0; i < 4; ++i) {
        Rectangle r = { 60.0f + (i % 2) * 600.0f, 132.0f + (i / 2) * 232.0f, 560.0f, 212.0f };
        UiButton *b = btn_find(S, UI_BTN_BUY_LEGACY, i);
        bool hot = is_hot(S, b);
        int level = game->legacy.levels[i];
        bool maxed = level >= LEGACY_MAX_LEVEL;
        int cost = maxed ? 0 : ua_legacy_cost(i, level);
        float hv = btn_hover_step(b, hot);
        bool usable = b && b->enabled;

        /* 整卡即按钮（01 §4.3）：COL_PANEL → COL_PANEL_HOT 插值 + COL_EP 描边 + 上移 1px。
         * 圆角走 R_CARD 12 —— 它和右上角的 EP 余额块（bal）是同族卡片，不是页面级面板
         * （R_PANEL 16 只给 End of Era 主面板与四个弹层）。 */
        Color fill = usable ? ui_color_mix(COL_PANEL, COL_PANEL_HOT, hv) : COL_PANEL;
        Rectangle rr = r;
        rr.y += hover_lift(hv);
        ui_panel(rr, R_CARD, maxed ? COL_SURFACE_OFF : fill,
                 (usable && hv > 0.02f) ? COL_EP : COL_LINE);

        ui_text(ua_legacy_name(i), rr.x + 24.0f, rr.y + 22.0f, F_TITLE, COL_TEXT);

        char lv[32];
        snprintf(lv, sizeof(lv), "Lv.%d / %d", level, LEGACY_MAX_LEVEL);
        ui_text_right(lv, rr.x + rr.width - 24.0f, rr.y + 24.0f, F_LABEL, COL_DIM);

        /* 等级点 */
        for (int k = 0; k < LEGACY_MAX_LEVEL; ++k) {
            Rectangle pip = { rr.x + 24.0f + k * 26.0f, rr.y + 54.0f, 18.0f, 6.0f };
            DrawRectangleRounded(pip, 1.0f, 4, k < level ? COL_EP : COL_BAR_TRACK);
        }

        char eff[160];
        ua_legacy_effect(game, i, level < LEGACY_MAX_LEVEL ? level + 1 : LEGACY_MAX_LEVEL, eff, (int)sizeof(eff));
        ui_text(maxed ? "已满级" : "下一级", rr.x + 24.0f, rr.y + 74.0f, F_CAPTION, COL_FAINT);
        ui_text(eff, rr.x + 24.0f, rr.y + 94.0f, F_BODY, COL_TEXT);

        char desc[160];
        ua_legacy_effect(game, i, level, desc, (int)sizeof(desc));
        ui_text("当前", rr.x + 24.0f, rr.y + 128.0f, F_CAPTION, COL_FAINT);
        ui_text(level > 0 ? desc : "尚未强化", rr.x + 24.0f, rr.y + 146.0f, F_BODY, COL_DIM);

        if (maxed) {
            ui_text_right("MAX", rr.x + rr.width - 24.0f, rr.y + 166.0f, F_LABEL, COL_EP);
        } else {
            char price[48];
            snprintf(price, sizeof(price), "%d EP", cost);
            Color pc = usable ? COL_EP : COL_FAINT;
            ui_text_right(price, rr.x + rr.width - 24.0f, rr.y + 160.0f, F_TITLE, pc);
            ui_text_right("点击卡片升级", rr.x + rr.width - 24.0f, rr.y + 186.0f, F_CAPTION, COL_FAINT);
        }
    }

    UiButton *start = btn_find(S, UI_BTN_START_NEW_RUN, -1);
    if (start) ui_btn_draw(start, "开始新轮演化", "资源与突变重置 · 图鉴与强化保留", is_hot(S, start));

    ui_text_center("未使用的 EP 会保留到以后", 640.0f, 706.0f, F_CAPTION, COL_FAINT);
}

/* ================= 弹层 ================= */

/* 面板高度随候选数量变化，避免固定高度留下大片空白 */
static Rectangle mutation_panel_rect(const GameState *game)
{
    int n = game->run.mutation_offer.active ? game->run.mutation_offer.choice_count : 0;
    if (n < 1) n = 1;
    float h = 96.0f + n * 122.0f + 84.0f;
    if (h < 320.0f) h = 320.0f;
    return (Rectangle){ 330.0f, (UI_H - h) * 0.5f, 620.0f, h };
}

static Rectangle codex_panel_rect(int tab, int achievement_count)
{
    float h = 424.0f;
    if (tab == 0) {
        h = 624.0f;
    } else if (tab == 1) {
        h = 624.0f;
    } else if (tab == 2) {
        /* 记录页：成就 2 列排布，首行在 panel.y + 282，行距 68，卡片高 56。
           原先写死 452：8 条成就（4 行）时最后一行底边到 282+3*68+56 = 542，
           溢出 90px。mock 只有 3 条成就，所以截图全绿、真实存档才破 —— 按实际
           数量算高度，杜绝"测试数据比真实数据温柔"这类隐藏 bug。 */
        int rows = achievement_count > 0 ? (achievement_count + 1) / 2 : 0;
        h = 282.0f + (rows > 0 ? (float)(rows - 1) * 68.0f + 56.0f : 0.0f) + 32.0f;
        if (h < 424.0f) h = 424.0f;
    }
    return (Rectangle){ 90.0f, (UI_H - h) * 0.5f, 1100.0f, h };
}

static Rectangle speciation_panel_rect(const GameState *game)
{
    int children[4];
    int n = ua_speciation_children(game, children, 4);
    if (n < 1) n = 1;
    float h = 92.0f + n * 110.0f + 84.0f;
    if (h < 330.0f) h = 330.0f;
    return (Rectangle){ 300.0f, (UI_H - h) * 0.5f, 680.0f, h };
}

static void build_overlay(UiLayout *L, const GameState *game, const UIState *S)
{
    /* 遮罩本身也是一个命中区：点空白处关闭弹层。
       它先入表，所以后续面板按钮会覆盖它。 */
    btn_add(L, UI_BTN_BACKDROP, (Rectangle){ 0, 0, UI_W, UI_H }, 0, 0, true);

    switch (S->overlay) {
    case UI_OVL_MUTATION: {
        Rectangle panel = mutation_panel_rect(game);
        for (int i = 0; i < game->run.mutation_offer.choice_count && i < MAX_MUTATION_CHOICES; ++i) {
            bool afford = game->run.dna >= game->run.mutation_offer.dna_costs[i];
            btn_add(L, UI_BTN_CHOOSE_MUTATION,
                    (Rectangle){ panel.x + 28.0f, panel.y + 96.0f + i * 122.0f, panel.width - 56.0f, 108.0f },
                    game->run.mutation_offer.mutation_ids[i], i, afford);
        }
        /* 底部两行：关闭 H_SECONDARY 34（下沿仍距面板 22）；生成候选带副标题 → H_PRIMARY 44 */
        btn_add(L, UI_BTN_CLOSE_OVERLAY, (Rectangle){ panel.x + (panel.width - 200.0f) * 0.5f, panel.y + panel.height - 56.0f, 200.0f, H_SECONDARY }, 0, 0, true);
        if (!game->run.mutation_offer.active) {
            btn_add(L, UI_BTN_OPEN_MUTATION, (Rectangle){ panel.x + (panel.width - 240.0f) * 0.5f, panel.y + panel.height - 106.0f, 240.0f, H_PRIMARY }, 0, 0, true);
        }
        break;
    }
    case UI_OVL_SPECIATION: {
        Rectangle panel = speciation_panel_rect(game);
        int children[4];
        int n = ua_speciation_children(game, children, 4);
        for (int i = 0; i < n; ++i) {
            bool ok = ua_speciation_check(game, children[i]) == ACTION_OK
                   && game->run.biomass >= ua_speciation_cost(game, children[i]);
            btn_add(L, UI_BTN_SPECIATE,
                    (Rectangle){ panel.x + 26.0f, panel.y + 92.0f + i * 110.0f, panel.width - 52.0f, 96.0f },
                    children[i], S->keep_parent, ok);
        }
        /* 底部一行两按钮：垂直中心都落在 panel.height − 49（原 50 高按钮的中心），
         * 高度不同但中心对齐，视觉上才是一条基线。 */
        btn_add(L, UI_BTN_TOGGLE_KEEP_PARENT,
                (Rectangle){ panel.x + 26.0f, panel.y + panel.height - 71.0f, 340.0f, H_PRIMARY }, 0, 0, true);
        btn_add(L, UI_BTN_CLOSE_OVERLAY,
                (Rectangle){ panel.x + panel.width - 156.0f, panel.y + panel.height - 66.0f, 130.0f, H_SECONDARY }, 0, 0, true);
        break;
    }
    case UI_OVL_CODEX: {
        Rectangle panel = codex_panel_rect(S->codex_tab, ua_achievement_count());
        for (int i = 0; i < 3; ++i) {
            btn_add(L, UI_BTN_CODEX_TAB,
                    (Rectangle){ panel.x + 26.0f + i * 116.0f, panel.y + 66.0f, 108.0f, H_SECONDARY }, i, i, true);
        }
        btn_add(L, UI_BTN_CLOSE_OVERLAY,
                (Rectangle){ panel.x + panel.width - 130.0f, panel.y + 16.0f, 104.0f, H_SECONDARY }, 0, 0, true);
        break;
    }
    case UI_OVL_TREE: {
        Rectangle panel = { 190.0f, 90.0f, 900.0f, 620.0f };
        btn_add(L, UI_BTN_CLOSE_OVERLAY,
                (Rectangle){ panel.x + panel.width - 130.0f, panel.y + 16.0f, 104.0f, H_SECONDARY }, 0, 0, true);
        break;
    }
    default:
        break;
    }
}

static void draw_overlay_backdrop(float alpha)
{
    /* 遮罩底 = COL_BG_DEEP（01 §2.2 它的第一用途就是“弹层遮罩下的最底层”）。
     * D12：alpha 0.62 → 0.72。0.62 在 4K 下底层文字仍清晰可读，和弹层内容抢读。 */
    DrawRectangle(0, 0, UI_W, UI_H, Fade(COL_BG_DEEP, alpha));
}

static void draw_mutation_overlay(const GameState *game, const UIState *S)
{
    Rectangle panel = mutation_panel_rect(game);
    ui_panel(panel, R_PANEL, COL_PANEL, COL_LINE);

    ui_text("主动突变", panel.x + 28.0f, panel.y + 26.0f, F_TITLE, COL_TEXT);
    char dna[64], num[32];
    ui_fmt_num(game->run.dna, num, (int)sizeof(num));
    snprintf(dna, sizeof(dna), "DNA %s", num);
    ui_text_right(dna, panel.x + panel.width - 28.0f, panel.y + 32.0f, F_LABEL, COL_DNA);

    const char *hint = game->run.mutation_offer.active
        ? "候选已锁定：换页或读档都不会刷新，选定后清除"
        : "当前没有候选，生成一次后固定保存";
    ui_text(hint, panel.x + 28.0f, panel.y + 58.0f, F_CAPTION, COL_FAINT);

    if (!game->run.mutation_offer.active) {
        UiButton *gen = btn_find(S, UI_BTN_OPEN_MUTATION, -1);
        if (gen) ui_btn_draw(gen, "生成候选", "每个物种独立计算", is_hot(S, gen));
        UiButton *close = btn_find(S, UI_BTN_CLOSE_OVERLAY, -1);
        if (close) ui_btn_draw(close, "关闭", NULL, is_hot(S, close));
        return;
    }

    for (int i = 0; i < game->run.mutation_offer.choice_count; ++i) {
        Rectangle r = { panel.x + 28.0f, panel.y + 96.0f + i * 122.0f, panel.width - 56.0f, 108.0f };
        UiButton *b = btn_find(S, UI_BTN_CHOOSE_MUTATION, game->run.mutation_offer.mutation_ids[i]);
        bool hot = is_hot(S, b);
        int mid = game->run.mutation_offer.mutation_ids[i];
        bool rare = ua_mutation_is_rare(mid);
        float hv = btn_hover_step(b, hot);
        bool usable = b && b->enabled;

        /* 弹层内候选卡 = 抬升面 COL_PANEL_HI，hover 插值到 COL_PANEL_HOT */
        Color fill = usable ? ui_color_mix(COL_PANEL_HI, COL_PANEL_HOT, hv) : COL_PANEL_HI;
        ui_panel(r, R_CARD, fill, rare ? Fade(COL_RARE, 0.6f) : COL_LINE);

        ui_text(ua_mutation_name(mid), r.x + S_PAD, r.y + 14.0f, F_TITLE, COL_TEXT);
        if (rare) {
            /* 稀有徽标：1px 描边 + 16% 透明填充 chip（01 §2.2 使用禁忌） */
            float nw = ui_text_w(ua_mutation_name(mid), F_TITLE);
            Rectangle tag = { r.x + S_PAD + nw + 10.0f, r.y + 18.0f,
                              ui_text_w("稀有", F_CAPTION) + 16.0f, H_CHIP };
            ui_panel(tag, R_CHIP(tag.height), Fade(COL_RARE, 0.16f), Fade(COL_RARE, 0.45f));
            ui_text_center("稀有", tag.x + tag.width * 0.5f, tag.y + 4.0f, F_CAPTION, COL_RARE);
        }

        char desc[160];
        ua_mutation_desc(mid, desc, (int)sizeof(desc));
        ui_text(desc, r.x + S_PAD, r.y + 44.0f, F_BODY, COL_DIM);

        char cost[48];
        ui_fmt_num(game->run.mutation_offer.dna_costs[i], num, (int)sizeof(num));
        snprintf(cost, sizeof(cost), "%s DNA", num);
        Color cc = usable ? COL_DNA : COL_BAD;
        ui_text(cost, r.x + S_PAD, r.y + 74.0f, F_BODY, cc);
        ui_text_right("点击选择", r.x + r.width - S_PAD, r.y + 76.0f, F_CAPTION, COL_FAINT);
    }

    UiButton *close = btn_find(S, UI_BTN_CLOSE_OVERLAY, -1);
    if (close) ui_btn_draw(close, "关闭", NULL, is_hot(S, close));
}

static void draw_speciation_overlay(const GameState *game, const UIState *S)
{
    Rectangle panel = speciation_panel_rect(game);
    ui_panel(panel, R_PANEL, COL_PANEL, COL_LINE);

    ui_text("物种分化", panel.x + 26.0f, panel.y + 24.0f, F_TITLE, COL_TEXT);
    char bio[64], num[32];
    ui_fmt_num(game->run.biomass, num, (int)sizeof(num));
    snprintf(bio, sizeof(bio), "生物量 %s", num);
    ui_text_right(bio, panel.x + panel.width - 26.0f, panel.y + 30.0f, F_LABEL, COL_BIOMASS);

    int children[4];
    int n = ua_speciation_children(game, children, 4);

    if (n <= 0) {
        ui_text_center("当前阶段没有可用的分化方向", panel.x + panel.width * 0.5f, panel.y + 200.0f, F_LABEL, COL_DIM);
        ui_text_center("先通过突变满足前置条件", panel.x + panel.width * 0.5f, panel.y + 226.0f, F_CAPTION, COL_FAINT);
    }

    for (int i = 0; i < n; ++i) {
        Rectangle r = { panel.x + 26.0f, panel.y + 92.0f + i * 110.0f, panel.width - 52.0f, 96.0f };
        UiButton *b = btn_find(S, UI_BTN_SPECIATE, children[i]);
        bool hot = is_hot(S, b);
        int sid = children[i];
        ActionCode check = ua_speciation_check(game, sid);
        double cost = ua_speciation_cost(game, sid);
        bool afford = game->run.biomass >= cost;
        float hv = btn_hover_step(b, hot);
        bool usable = b && b->enabled;

        ui_panel(r, R_CARD, usable ? ui_color_mix(COL_PANEL_HI, COL_PANEL_HOT, hv) : COL_PANEL_HI,
                 usable ? Fade(COL_ACCENT, 0.55f) : COL_LINE);

        DrawCircleV((Vector2){ r.x + 38.0f, r.y + 48.0f }, 18.0f, Fade(ecology_color(sid), 0.9f));

        ui_text(ua_species_name(sid), r.x + 68.0f, r.y + 16.0f, F_LABEL, COL_TEXT);
        char meta[96];
        snprintf(meta, sizeof(meta), "%s · 阶段 %d", ua_ecology_name(sid), ua_species_stage(sid));
        ui_text(meta, r.x + 68.0f, r.y + 40.0f, F_CAPTION, COL_DIM);

        char req[160];
        ua_speciation_requirement(sid, req, (int)sizeof(req));
        ui_text(req, r.x + 68.0f, r.y + 62.0f, F_CAPTION,
               check == ACTION_OK ? COL_FAINT : COL_BAD);

        char price[64];
        fmt_cost(num, (int)sizeof(num), cost);
        snprintf(price, sizeof(price), "%s 生物量", num);
        ui_text_right(price, r.x + r.width - 20.0f, r.y + 20.0f, F_LABEL, afford ? COL_BIOMASS : COL_BAD);

        if (check != ACTION_OK) {
            ui_text_right("前置不足", r.x + r.width - 20.0f, r.y + 46.0f, F_CAPTION, COL_BAD);
        } else if (!afford) {
            ui_text_right("生物量不足", r.x + r.width - 20.0f, r.y + 46.0f, F_CAPTION, COL_BAD);
        }
    }

    UiButton *keep = btn_find(S, UI_BTN_TOGGLE_KEEP_PARENT, -1);
    if (keep) {
        char label[96];
        snprintf(label, sizeof(label), "保留亲本为额外分支：%s", S->keep_parent ? "是" : "否");
        ui_btn_draw(keep, label, S->keep_parent ? "需要多样性 Lv.2，可替换旧分支" : "亲本离开活跃列表", is_hot(S, keep));
    }

    UiButton *close = btn_find(S, UI_BTN_CLOSE_OVERLAY, -1);
    if (close) ui_btn_draw(close, "关闭", NULL, is_hot(S, close));
}

static void draw_codex_overlay(const GameState *game, const UIState *S)
{
    Rectangle panel = codex_panel_rect(S->codex_tab, ua_achievement_count());
    ui_panel(panel, R_PANEL, COL_PANEL, COL_LINE);

    ui_text("生命图鉴", panel.x + 26.0f, panel.y + 22.0f, F_TITLE, COL_TEXT);

    UiButton *close = btn_find(S, UI_BTN_CLOSE_OVERLAY, -1);
    if (close) ui_btn_draw(close, "关闭", NULL, is_hot(S, close));

    const char *tabs[3] = { "物种", "突变", "记录" };
    for (int i = 0; i < 3; ++i) {
        UiButton *b = btn_find(S, UI_BTN_CODEX_TAB, i);
        if (!b) continue;
        bool active = (S->codex_tab == i);
        ui_panel(b->rect, R_BTN, active ? COL_PANEL_HOT : COL_PANEL_HI, active ? COL_ACCENT : COL_LINE);
        ui_text_center(tabs[i], b->rect.x + b->rect.width * 0.5f, b->rect.y + 8.0f, F_LABEL,
                       active ? COL_TEXT : COL_DIM);
    }

    ui_divider(panel.x + 26.0f, panel.x + panel.width - 26.0f, panel.y + 112.0f, COL_LINE);

    if (codex_last_tab != S->codex_tab) { codex_page=0; codex_last_tab=S->codex_tab; }
    int page_size = S->codex_tab == 0 ? 12 : 12;
    int total = S->codex_tab == 0 ? SPECIES_COUNT : MUTATION_COUNT;
    int pages = (total+page_size-1)/page_size;
    float wheel = GetMouseWheelMove();
    if(wheel<0 && codex_page<pages-1) codex_page++;
    if(wheel>0 && codex_page>0) codex_page--;
    if(S->codex_tab<2) { char page[80];snprintf(page,sizeof page,"滚轮切换 · %d / %d 页",codex_page+1,pages);ui_text(page,panel.x+26,panel.y+panel.height-28,F_CAPTION,COL_DIM); }
    if (S->codex_tab == 0) {
        int discovered = 0;
        for (int i = 0; i < SPECIES_COUNT; ++i) {
            bool visible = i / page_size == codex_page;
            int j=i%page_size;
            Rectangle r = { panel.x + 26.0f + (j % 4) * 262.0f, panel.y + 132.0f + (j / 4) * 132.0f, 244.0f, 116.0f };
            bool found = game->legacy.codex_species[i];
            if (found) discovered++;
            if (!visible) continue;
            ui_panel(r, R_CARD, found ? COL_PANEL_HI : COL_SURFACE_OFF, COL_LINE);
            if (found) {
                DrawCircleV((Vector2){ r.x + 40.0f, r.y + 52.0f }, 22.0f, Fade(ecology_color(i), 0.9f));
                ui_text(ua_species_name(i), r.x + 76.0f, r.y + 30.0f, F_LABEL, COL_TEXT);
                char meta[64];
                snprintf(meta, sizeof(meta), "%s · 阶段 %d", ua_ecology_name(i), ua_species_stage(i));
                ui_text(meta, r.x + 76.0f, r.y + 54.0f, F_CAPTION, COL_DIM);
            } else {
                /* 未发现节点：卡片比圆再暗一档，圆才会「浮」出来（附录 A 行 1065 把两者
                 * 都映射到 COL_SURFACE_OFF，那样圆与卡同色会直接消失，问号落空） */
                DrawCircleV((Vector2){ r.x + 40.0f, r.y + 52.0f }, 22.0f, COL_PANEL_HI);
                ui_text_center("?", r.x + 40.0f, r.y + 40.0f, F_TITLE, COL_FAINT);
                ui_text("未发现", r.x + 76.0f, r.y + 42.0f, F_BODY, COL_FAINT);
            }
        }
        char cnt[64];
        snprintf(cnt, sizeof(cnt), "已发现 %d / %d", discovered, SPECIES_COUNT);
        ui_text_right(cnt, panel.x + panel.width - 26.0f, panel.y + 76.0f, F_BODY, COL_DIM);
    } else if (S->codex_tab == 1) {
        int discovered = 0;
        for (int i = 0; i < MUTATION_COUNT; ++i) {
            bool visible = i / page_size == codex_page;
            int j=i%page_size;
            Rectangle r = { panel.x + 26.0f + (j % 3) * 350.0f, panel.y + 132.0f + (j / 3) * 116.0f, 332.0f, 100.0f };
            bool found = game->legacy.codex_mutations[i];
            if (found) discovered++;
            if (!visible) continue;
            ui_panel(r, R_CARD, found ? COL_PANEL_HI : COL_SURFACE_OFF, COL_LINE);
            if (found) {
                ui_text(ua_mutation_name(i), r.x + S_PAD, r.y + 14.0f, F_LABEL, COL_TEXT);
                char desc[160];
                ua_mutation_desc(i, desc, (int)sizeof(desc));
                ui_text(desc, r.x + S_PAD, r.y + 38.0f, F_CAPTION, COL_DIM);
            } else {
                ui_text("? ? ?", r.x + S_PAD, r.y + 14.0f, F_LABEL, COL_FAINT);
                ui_text("尚未获得", r.x + S_PAD, r.y + 38.0f, F_CAPTION, COL_FAINT);
            }
        }
        char cnt[64];
        snprintf(cnt, sizeof(cnt), "已发现 %d / %d", discovered, MUTATION_COUNT);
        ui_text_right(cnt, panel.x + panel.width - 26.0f, panel.y + 76.0f, F_BODY, COL_DIM);
    } else {
        const char *labels[3] = { "最高阶段", "最高 EP", "最高生物量" };
        double values[3] = { (double)game->legacy.best_era, (double)game->legacy.best_evolution_points, game->legacy.best_biomass };
        Color colors[3] = { COL_ACCENT, COL_EP, COL_BIOMASS };
        for (int i = 0; i < 3; ++i) {
            Rectangle r = { panel.x + 26.0f + i * 350.0f, panel.y + 132.0f, 332.0f, 96.0f };
            ui_panel(r, R_CARD, COL_PANEL_HI, COL_LINE);
            ui_text(labels[i], r.x + S_PAD, r.y + 16.0f, F_CAPTION, COL_DIM);
            char v[32];
            if (i < 2) snprintf(v, sizeof(v), "%.0f", values[i]);
            else ui_fmt_num(values[i], v, (int)sizeof(v));
            ui_text(v, r.x + S_PAD, r.y + 40.0f, F_DISPLAY, colors[i]);
        }

        ui_text("成就", panel.x + 26.0f, panel.y + 250.0f, F_LABEL, COL_TEXT);
        int n = ua_achievement_count();
        for (int i = 0; i < n && i < 8; ++i) {
            Rectangle r = { panel.x + 26.0f + (i % 2) * 522.0f, panel.y + 282.0f + (i / 2) * 68.0f, 504.0f, 56.0f };
            bool got = false;
            ui_panel(r, R_BTN, got ? COL_PANEL_HI : COL_SURFACE_OFF,
                     got ? Fade(COL_ACCENT, 0.5f) : COL_LINE);
            ui_text(ua_achievement_name(i), r.x + S_PAD, r.y + 10.0f, F_BODY, got ? COL_TEXT : COL_FAINT);
            ui_text(ua_achievement_desc(i), r.x + S_PAD, r.y + 30.0f, F_CAPTION, got ? COL_DIM : COL_FAINT);
        }
    }
}

/* 进化树的亲缘连线：三段折线，竖直段落在两列之间的空隙里，不进卡片、不压字。
 * 只用 DrawLineEx / DrawCircleV（三角形填充路径），线宽与圆半径都跟随 Camera2D.zoom；
 * DrawCircleLines / DrawRectangleRoundedLinesEx 是设备像素原语，SS>1 会退化成发丝（Z-UNSAFE）。 */
static void tree_link(Vector2 a, Vector2 b, Color c, float w)
{
    float mx = (a.x + b.x) * 0.5f;
    DrawLineEx(a, (Vector2){ mx, a.y }, w, c);
    DrawLineEx((Vector2){ mx, a.y }, (Vector2){ mx, b.y }, w, c);
    DrawLineEx((Vector2){ mx, b.y }, b, w, c);
    DrawCircleV(a, w * 1.5f, c);
    DrawCircleV(b, w * 1.5f, c);
}

static void draw_tree_overlay(const GameState *game, const UIState *S)
{
    Rectangle panel = { 190.0f, 90.0f, 900.0f, 620.0f };
    ui_panel(panel, R_PANEL, COL_PANEL, COL_LINE);

    ui_text("进化树", panel.x + 26.0f, panel.y + 22.0f, F_TITLE, COL_TEXT);
    ui_text("连线为分化亲缘：亮色是已走过的谱系，暗色是尚未探索的分支", panel.x + 26.0f, panel.y + 54.0f, F_CAPTION, COL_DIM);

    UiButton *close = btn_find(S, UI_BTN_CLOSE_OVERLAY, -1);
    if (close) ui_btn_draw(close, "关闭", NULL, is_hot(S, close));

    int main_id = game->run.active_species_count > 0 ? game->run.active_species[0].species_id : 0;
    int main_stage = ua_species_stage(main_id);

    /* 卡片按阶段分列，而不是按 species_id 摆 2×4 网格。
     * ID 顺序的阶段分布是 0,1,1,2 / 2,3,3，按 ID 连线会交叉成网；
     * 按阶段分列后亲本永远在左侧，连线只剩分叉，没有回边。 */
    int max_stage = 0;
    for (int i = 0; i < SPECIES_COUNT; ++i) {
        int st = ua_species_stage(i);
        if (st > max_stage) max_stage = st;
    }
    float wheel = GetMouseWheelMove();
    if(wheel<0 && tree_page<(max_stage/3)) tree_page++;
    if(wheel>0 && tree_page>0) tree_page--;
    int first_stage=tree_page*3;
    int cols = max_stage-first_stage+1;
    if(cols>3) cols=3;
    if (cols < 1) cols = 1;
    if (cols > 6) cols = 6;             /* 数据表扩容时的布局护栏 */

    int col_count[6] = { 0, 0, 0, 0, 0, 0 };
    int stage_of[SPECIES_COUNT];
    int row_of[SPECIES_COUNT];
    int max_rows = 1;
    for (int i = 0; i < SPECIES_COUNT; ++i) {
        int st = ua_species_stage(i);
        st-=first_stage;
        stage_of[i] = st;
        if(st<0 || st>=cols) continue;
        row_of[i] = col_count[st]++;
        if (col_count[st] > max_rows) max_rows = col_count[st];
    }

    const float inner_x = panel.x + 26.0f;
    const float inner_w = panel.width - 52.0f;
    const float col_gap = 56.0f;
    const float card_w = (inner_w - col_gap * (float)(cols - 1)) / (float)cols;
    const float top = panel.y + 116.0f;
    const float avail_h = 424.0f;       /* 底沿 panel.y+540，图例留在 panel.y+586 */
    const float row_gap = 16.0f;
    const float card_h = fminf(196.0f,
                               (avail_h - row_gap * (float)(max_rows - 1)) / (float)max_rows);
    const float cell_r = fminf(22.0f, card_w * 0.16f);
    const bool compact = card_h < 174.0f;
    const float cell_cy = compact ? card_h*0.5f : 62.0f;

    Rectangle card[SPECIES_COUNT];
    for (int i = 0; i < SPECIES_COUNT; ++i) {
        int col = stage_of[i], row = row_of[i];
        if(col<0 || col>=cols) continue;
        float stack_h = card_h * (float)col_count[col] + row_gap * (float)(col_count[col] - 1);
        card[i] = (Rectangle){ inner_x + (float)col * (card_w + col_gap),
                               top + (avail_h - stack_h) * 0.5f + (float)row * (card_h + row_gap),
                               card_w, card_h };
    }

    /* 列头先画，连线再画，卡片最后画：卡片不透明，会自然盖住跨阶段连线
     * 穿过中间列的那一小段（如阶段1→阶段3），读起来是"从节点背后绕过去"。 */
    for (int c = 0; c < cols; ++c) {
        char head[48];
        snprintf(head, sizeof(head), "%d · %s", c+first_stage, ua_stage_name(c+first_stage));
        ui_text_center(head, inner_x + (float)c * (card_w + col_gap) + card_w * 0.5f,
                       panel.y + 86.0f, F_CAPTION, COL_DIM);
    }

    for (int child = 0; child < SPECIES_COUNT; ++child) {
        if(stage_of[child]<0 || stage_of[child]>=cols) continue;
        int parents[4];
        int n = ua_species_parents(child, parents, 4);
        for (int k = 0; k < n; ++k) {
            int p = parents[k];
            if (p < 0 || p >= SPECIES_COUNT || stage_of[p]<0 || stage_of[p]>=cols || stage_of[p] >= stage_of[child]) continue;
            bool lit = game->legacy.codex_species[p] && game->legacy.codex_species[child];
            /* 当前物种的"入边"单独提亮：一眼看出这一轮走到树上的哪个位置。
               其余已点亮分支用子代物种色，未探索分支压到 COL_LINE_STRONG 半透明。 */
            Color c = child == main_id ? Fade(COL_ACCENT, 0.92f)
                    : lit             ? Fade(ecology_color(child), 0.72f)
                                      : Fade(COL_LINE_STRONG, 0.34f);
            /* A4：描边宽度只允许 {1,2}。层级靠颜色表达，不靠粗细，
               否则 4K 下 2.6px 落到 7.8 物理像素，和 2px 卡片描边不成套。 */
            float w = 2.0f;
            Vector2 a = { card[p].x + card_w * 0.5f, card[p].y + cell_cy };
            Vector2 b = { card[child].x + card_w * 0.5f, card[child].y + cell_cy };
            a.x += cell_r;
            b.x -= cell_r;
            tree_link(a, b, c, w);
        }
    }

    for (int i = 0; i < SPECIES_COUNT; ++i) {
        if(stage_of[i]<0 || stage_of[i]>=cols) continue;
        Rectangle r = card[i];
        int stage = stage_of[i]+first_stage;
        bool found = game->legacy.codex_species[i];
        bool current = (i == main_id);

        ui_panel(r, R_CARD, current ? COL_ACCENT_SURFACE : COL_PANEL_HI,
                 current ? COL_ACCENT : COL_LINE);

        char st[32];
        snprintf(st, sizeof(st), "阶段 %d", stage);
        ui_text(st, r.x + 10.0f, r.y + 10.0f, F_CAPTION, COL_FAINT);

        float cx = r.x + r.width * 0.5f;
        bool reveal=found || stage<=main_stage;
        if(compact) {
            ui_text_center(reveal?ua_species_name(i):"未发现",cx,r.y+30,F_LABEL,reveal?COL_TEXT:COL_FAINT);
            ui_text_center(reveal?ua_ecology_name(i):"?",cx,r.y+52,F_CAPTION,COL_DIM);
        } else if(reveal) {
            DrawCircleV((Vector2){cx,r.y+cell_cy},cell_r,Fade(ecology_color(i),0.85f));
            ui_text_center(ua_species_name(i),cx,r.y+96,F_LABEL,COL_TEXT);
            ui_text_center(ua_ecology_name(i),cx,r.y+118,F_CAPTION,COL_DIM);
        } else {
            DrawCircleV((Vector2){cx,r.y+cell_cy},cell_r,COL_SURFACE_OFF);
            ui_text_center("?",cx,r.y+cell_cy-10,F_TITLE,COL_FAINT);
            ui_text_center("未发现",cx,r.y+96,F_BODY,COL_FAINT);
        }
        char cost[48],num[32];
        double c=ua_speciation_cost(game,i);
        fmt_cost(num,sizeof num,c);
        if(c<=0)snprintf(cost,sizeof cost,"初始物种");
        else snprintf(cost,sizeof cost,"分化 %s",num);
        ui_text_center(cost,cx,r.y+(compact?74:146),F_BODY,found?COL_BIOMASS:COL_FAINT);
        if (current) ui_tag(r, "当前", COL_ACCENT);
    }

    ui_text("滚轮翻页查看全部阶段 · 灰色卡片为尚未探索的分支", panel.x + 26.0f, panel.y + panel.height - 34.0f, F_CAPTION, COL_FAINT);
}

static void draw_overlay(const GameState *game, const UIState *S)
{
    if (S->overlay == UI_OVL_NONE) return;
    draw_overlay_backdrop(0.72f * S->ovl_t);
    switch (S->overlay) {
    case UI_OVL_MUTATION:   draw_mutation_overlay(game, S); break;
    case UI_OVL_SPECIATION: draw_speciation_overlay(game, S); break;
    case UI_OVL_CODEX:      draw_codex_overlay(game, S); break;
    case UI_OVL_TREE:       draw_tree_overlay(game, S); break;
    default: break;
    }
}

/* ================= 对外入口 ================= */

void screens_build(UiLayout *out, const GameState *game, const UIState *state)
{
    out->count = 0;
    UiScreen screen = state->screen;

    if (screen == UI_SCREEN_MAIN)       build_main(out, game, state);
    else if (screen == UI_SCREEN_END_ERA) build_end_era(out, game, state);
    else                                 build_legacy(out, game, state);

    if (state->overlay != UI_OVL_NONE) build_overlay(out, game, state);

    /* 本帧快照 → 下一帧 btn_add 用它继承 hover/press（见文件顶部的 s_prev 说明） */
    s_prev = *out;
}

void screens_draw(const GameState *game, const UIState *state, const ActionResult *last_result)
{
    switch (state->screen) {
    case UI_SCREEN_MAIN:     draw_main(game, state, last_result); break;
    case UI_SCREEN_END_ERA:  draw_end_era(game, state); break;
    case UI_SCREEN_LEGACY:   draw_legacy(game, state); break;
    default: break;
    }
    draw_overlay(game, state);
}
