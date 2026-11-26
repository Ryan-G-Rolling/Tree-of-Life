/*
 * ui.c  ——  UI 状态机与输入→动作转换（Member C）
 *
 * 全文件唯一允许写游戏数据的地方：把玩家意图打包成 GameAction 交给 A。
 * 这里没有一处 game->run.xxx = ...，也没有一处资源公式。
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raylib.h"
#include "ui.h"
#include "ui_internal.h"
#include "ui_adapter.h"
#include "atmosphere.h" /* 氛围层（02-atmosphere）：L1+L2 静态 / L5 暗角 */

UIState g_ui;

static RenderTexture2D g_canvas;
static Sound g_snd_click = { 0 };
static Sound g_snd_buy = { 0 };
static Sound g_snd_evolve = { 0 };
static bool g_audio = false;

/* ================= 超采样画布 =================
 * 逻辑坐标固定 1280×800，画布按 SS 放大：画布 = UI_W*SS × UI_H*SS。
 * 窗口 1280×800 → SS=1；1920×1200 → SS=2；3840×2400 → SS=3。
 * 非整数倍窗口（例如 1600×1000）也会向上取整到 2，多出的采样在合成时被
 * 下采样吸收，等于免费的 SSAA。
 *
 * 契约（见 docs/art/01-visual-identity.md §7.3）：
 *   1. 界面绘制一律在 1280×800 里画，禁止手动乘 SS、禁止拿 ui_canvas_ss()
 *      参与任何尺寸计算 —— SS 只在这里被消费。
 *   2. SS 只能是整数（UI_SS_MAX=3），所以 1px 描边乘 SS 后仍是整数设备像素。
 *   3. 字库与画布纹理一律 TEXTURE_FILTER_BILINEAR，**禁止** TEXTURE_FILTER_POINT：
 *      画布在非整数倍窗口下要下采样，点采样会直接丢像素出锯齿。 */

static int g_ss = 0;        /* 当前画布实际 SS；0 表示还没创建 */
static int g_ss_forced = 0; /* 0 = 自动，UI_SS_MIN..UI_SS_MAX = 强制（命令行指定） */

void ui_set_forced_ss(int ss)
{
    if (ss < 0) ss = 0;
    if (ss > UI_SS_MAX) ss = UI_SS_MAX;
    g_ss_forced = ss;
}

int ui_canvas_ss(void) { return (g_ss >= UI_SS_MIN && g_ss <= UI_SS_MAX) ? g_ss : UI_SS_MIN; }

/* SS = clamp(ceil(min(winW/UI_W, winH/UI_H)), UI_SS_MIN, UI_SS_MAX)
 * 减一个极小量再取整：避免 1280*2=2560 这类恰好整数倍的档位被浮点误差抬高一档。 */
static int compute_ss(void)
{
    if (g_ss_forced >= UI_SS_MIN && g_ss_forced <= UI_SS_MAX) return g_ss_forced;

    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    if (sw < 1.0f || sh < 1.0f) return UI_SS_MIN;

    float fx = sw / (float)UI_W;
    float fy = sh / (float)UI_H;
    float f = (fx < fy) ? fx : fy;

    int ss = (int)ceilf(f - 0.0001f);
    if (ss < UI_SS_MIN) ss = UI_SS_MIN;
    if (ss > UI_SS_MAX) ss = UI_SS_MAX; /* 再大就纯烧显存：4K 以上已由 letterbox 下采样覆盖 */
    return ss;
}

static void canvas_ensure(int ss)
{
    if (ss < UI_SS_MIN) ss = UI_SS_MIN;
    if (ss > UI_SS_MAX) ss = UI_SS_MAX;
    if (ss == g_ss && IsRenderTextureValid(g_canvas)) return;

    if (IsRenderTextureValid(g_canvas)) UnloadRenderTexture(g_canvas);

    g_canvas = LoadRenderTexture(UI_W * ss, UI_H * ss);
    /* 老旧显卡可能建不出 3840×2400 的 FBO（GL_MAX_RENDERBUFFER_SIZE 不够）：
     * 宁可退回 SS=1 也不能黑屏。GL 3.3 设备一般 ≥ 4096，属于兜底。 */
    if (!IsRenderTextureValid(g_canvas) && ss > UI_SS_MIN) {
        TraceLog(LOG_WARNING, "UI canvas: SS=%d (%dx%d) failed, falling back to SS=%d",
                 ss, UI_W * ss, UI_H * ss, UI_SS_MIN);
        ss = UI_SS_MIN;
        g_canvas = LoadRenderTexture(UI_W * ss, UI_H * ss);
    }
    /* 画布必须 BILINEAR（禁止 POINT）：非整数倍窗口下要靠它做下采样 */
    SetTextureFilter(g_canvas.texture, TEXTURE_FILTER_BILINEAR);
    g_ss = ss;

    /* 氛围层按新 SS 重烧（02 §6.3 坑 3：SS 变了不重烧 → DrawTexturePro 拉伸，
     * 网格线宽不再是 SS 物理像素，且不报任何错）。必须在 g_ss 赋值之后调用：
     * atm_ensure() 内部读 ui_canvas_ss()。SS 未变时它只做一次 int 比较。 */
    atm_ensure();

    TraceLog(LOG_INFO, "UI canvas: %dx%d (SS=%d) for window %dx%d",
             UI_W * ss, UI_H * ss, ss, GetScreenWidth(), GetScreenHeight());
}

/* 逻辑坐标 → 画布像素：x*SS。绘制代码仍在 1280×800 里画。 */
static Camera2D ui_camera(void)
{
    Camera2D cam = { 0 };
    cam.offset = (Vector2){ 0.0f, 0.0f };
    cam.target = (Vector2){ 0.0f, 0.0f };
    cam.rotation = 0.0f;
    cam.zoom = (float)ui_canvas_ss();
    return cam;
}

/* ================= 辅助 ================= */

static UiScreen screen_from_phase(GamePhase phase)
{
    switch (phase) {
    case PHASE_END_ERA_PREVIEW: return UI_SCREEN_END_ERA;
    case PHASE_LEGACY:          return UI_SCREEN_LEGACY;
    default:                    return UI_SCREEN_MAIN;
    }
}

static void toast(const char *text, bool bad, float seconds)
{
    snprintf(g_ui.toast, sizeof(g_ui.toast), "%s", text ? text : "");
    g_ui.toast_t = seconds;
    g_ui.toast_bad = bad;
}

static void play(Sound s)
{
    if (g_audio && IsSoundValid(s)) PlaySound(s);
}

static bool action_needs_game(UiButtonId id)
{
    switch (id) {
    case UI_BTN_CLICK:
    case UI_BTN_CAPACITY:
    case UI_BTN_RESERVE:
    case UI_BTN_TECH:
    case UI_BTN_UPGRADE:
    case UI_BTN_OPEN_MUTATION:
    case UI_BTN_CHOOSE_MUTATION:
    case UI_BTN_SPECIATE:
    case UI_BTN_OPEN_END_ERA:
    case UI_BTN_CANCEL_END_ERA:
    case UI_BTN_CONFIRM_END_ERA:
    case UI_BTN_BUY_LEGACY:
    case UI_BTN_START_NEW_RUN:
        return true;
    default:
        return false;
    }
}

static void fill_action(const UiButton *b, GameAction *out)
{
    memset(out, 0, sizeof(*out));
    switch (b->id) {
    case UI_BTN_CLICK:            out->type = ACTION_CLICK_ATP; break;
    case UI_BTN_CAPACITY: out->type=ACTION_BUY_CAPACITY; break;
    case UI_BTN_RESERVE: out->type=ACTION_BUY_RESERVE; break;
    case UI_BTN_TECH: out->type=ACTION_BUY_SUB_UPGRADE; break;
    case UI_BTN_UPGRADE:          out->type = ACTION_BUY_RUN_UPGRADE; out->target_id = b->target_id; break;
    case UI_BTN_OPEN_MUTATION:    out->type = ACTION_GENERATE_MUTATION_OFFER; out->target_species_slot = b->target_id; break;
    case UI_BTN_CHOOSE_MUTATION:  out->type = ACTION_CHOOSE_MUTATION; out->target_id = b->target_id; out->option = b->option; break;
    case UI_BTN_SPECIATE:         out->type = ACTION_SPECIATE; out->target_species_slot = 0; out->target_id = b->target_id; out->option = b->option; break;
    case UI_BTN_OPEN_END_ERA:     out->type = ACTION_OPEN_END_ERA; break;
    case UI_BTN_CANCEL_END_ERA:   out->type = ACTION_CANCEL_END_ERA; break;
    case UI_BTN_CONFIRM_END_ERA:  out->type = ACTION_CONFIRM_END_ERA; break;
    case UI_BTN_BUY_LEGACY:       out->type = ACTION_BUY_LEGACY; out->target_id = b->target_id; break;
    case UI_BTN_START_NEW_RUN:    out->type = ACTION_START_NEW_RUN; break;
    default: return;
    }
}

/* 纯 UI 行为：只改 g_ui，不碰 GameState */
static void handle_local_button(const UiButton *b, const GameState *game)
{
    switch (b->id) {
    case UI_BTN_OPEN_SPECIATION: {
        if (!b->enabled) {
            toast("还没有可用的分化方向：先通过突变满足前置条件", true, 3.0f);
            break;
        }
        g_ui.overlay = UI_OVL_SPECIATION;
        g_ui.ovl_t = 0.0f;
        /* 有第二槽才可能“保留亲本” */
        g_ui.keep_parent = (game->legacy.levels[LEGACY_DIVERSIFICATION] >= 2) ? 1 : 0;
        break;
    }
    case UI_BTN_TOGGLE_KEEP_PARENT:
        g_ui.keep_parent = g_ui.keep_parent ? 0 : 1;
        break;
    case UI_BTN_OPEN_CODEX:
        g_ui.overlay = UI_OVL_CODEX;
        g_ui.ovl_t = 0.0f;
        break;
    case UI_BTN_OPEN_TREE:
        g_ui.overlay = UI_OVL_TREE;
        g_ui.ovl_t = 0.0f;
        break;
    case UI_BTN_CODEX_TAB:
        g_ui.codex_tab = b->option;
        break;
    case UI_BTN_CLOSE_OVERLAY:
    case UI_BTN_BACKDROP:
        g_ui.overlay = UI_OVL_NONE;
        break;
    default:
        break;
    }
}

static void close_overlay_now(void)
{
    g_ui.overlay = UI_OVL_NONE;
    g_ui.ovl_t = 0.0f;
}

/* ================= 生命周期 ================= */

void ui_init(void)
{
    memset(&g_ui, 0, sizeof(g_ui));
    g_ui.screen = UI_SCREEN_MAIN;
    g_ui.overlay = UI_OVL_NONE;
    g_ui.hot = -1;
    g_ui.pressed = -1;
    g_ui.last_phase = -1;

    ui_font_init();

    g_ss = 0;
    canvas_ensure(compute_ss());

    if (IsAudioDeviceReady()) {
        if (FileExists("assets/sfx/click.wav"))  g_snd_click  = LoadSound("assets/sfx/click.wav");
        if (FileExists("assets/sfx/buy.wav"))    g_snd_buy    = LoadSound("assets/sfx/buy.wav");
        if (FileExists("assets/sfx/evolve.wav")) g_snd_evolve = LoadSound("assets/sfx/evolve.wav");
        g_audio = IsSoundValid(g_snd_click);
    }

    toast("点击生命体开始积累 ATP", false, 3.5f);
}

void ui_shutdown(void)
{
    if (g_audio) {
        if (IsSoundValid(g_snd_click)) UnloadSound(g_snd_click);
        if (IsSoundValid(g_snd_buy)) UnloadSound(g_snd_buy);
        if (IsSoundValid(g_snd_evolve)) UnloadSound(g_snd_evolve);
    }
    /* 氛围层纹理（静态满分辨率 + 暗角）必须在 GL 上下文还活着时释放 */
    atm_shutdown();
    if (IsRenderTextureValid(g_canvas)) UnloadRenderTexture(g_canvas);
    g_ss = 0;
    ui_font_shutdown();
}

int ui_should_close(void)
{
    return WindowShouldClose() ? 1 : 0;
}

/* ================= 每帧输入 ================= */

static void fire(int index, const GameState *game, GameAction *out, bool *has_action)
{
    UiButton *b = &g_ui.layout.items[index];
    g_ui.pressed = index;

    if (action_needs_game(b->id)) {
        fill_action(b, out);
        *has_action = true;
        if (b->id == UI_BTN_OPEN_MUTATION) {
            g_ui.overlay = UI_OVL_MUTATION;
            g_ui.ovl_t = 0.0f;
        }
    } else {
        handle_local_button(b, game);
    }

    if (b->id == UI_BTN_CLICK) {
        g_ui.press_anim = 1.0f;
        play(g_snd_click);
    } else {
        play(g_snd_buy);
    }
}

bool ui_update(const GameState *game, GameAction *pending_action)
{
    bool has_action = false;
    if (pending_action) memset(pending_action, 0, sizeof(*pending_action));

    input_begin_frame();

    /* 阶段变化 → 切页面并关弹层（phase 是唯一权威，UI 不自己记状态） */
    UiScreen want = screen_from_phase(game->phase);
    if (want != g_ui.screen) {
        g_ui.screen = want;
        close_overlay_now();
    }
    if (g_ui.last_phase != (int)game->phase) {
        g_ui.last_phase = (int)game->phase;
    }

    /* 候选被清除（选择成功 / 分化 / 新轮）→ 自动收起突变弹层 */
    static bool prev_offer_active = false;
    if (prev_offer_active && !game->run.mutation_offer.active && g_ui.overlay == UI_OVL_MUTATION) {
        close_overlay_now();
    }
    prev_offer_active = game->run.mutation_offer.active;

    screens_build(&g_ui.layout, game, &g_ui);
    g_ui.hot = input_hit_topmost(&g_ui.layout, &g_ui);

    /* 鼠标 */
    if (input_click_pressed()) {
        if (g_ui.hot >= 0) {
            UiButton *b = &g_ui.layout.items[g_ui.hot];
            if (b->enabled) {
                fire(g_ui.hot, game, pending_action, &has_action);
            } else {
                toast("条件不足：资源不够或前置未满足", true, 2.0f);
            }
        }
    }

    /* 键盘快捷键 */
    if (g_ui.overlay != UI_OVL_NONE) {
        if (input_key_pressed(KEY_ESCAPE)) {
            close_overlay_now();
        }
    } else if (g_ui.screen == UI_SCREEN_MAIN) {
        if (input_key_pressed(KEY_SPACE)) {
            if (pending_action) { pending_action->type = ACTION_CLICK_ATP; has_action = true; }
            g_ui.press_anim = 1.0f;
            play(g_snd_click);
        }
        if (input_key_pressed(KEY_ONE) || input_key_pressed(KEY_KP_1)) {
            UiButton *b = NULL;
            for (int i = 0; i < g_ui.layout.count; ++i)
                if (g_ui.layout.items[i].id == UI_BTN_UPGRADE && g_ui.layout.items[i].target_id == 0) b = &g_ui.layout.items[i];
            if (b && b->enabled && pending_action) { fill_action(b, pending_action); has_action = true; play(g_snd_buy); }
        }
        if (input_key_pressed(KEY_TWO) || input_key_pressed(KEY_KP_2)) {
            for (int i = 0; i < g_ui.layout.count; ++i)
                if (g_ui.layout.items[i].id == UI_BTN_UPGRADE && g_ui.layout.items[i].target_id == 1 && g_ui.layout.items[i].enabled && pending_action) {
                    fill_action(&g_ui.layout.items[i], pending_action); has_action = true; play(g_snd_buy);
                }
        }
        if (input_key_pressed(KEY_THREE) || input_key_pressed(KEY_KP_3)) {
            for (int i = 0; i < g_ui.layout.count; ++i)
                if (g_ui.layout.items[i].id == UI_BTN_UPGRADE && g_ui.layout.items[i].target_id == 2 && g_ui.layout.items[i].enabled && pending_action) {
                    fill_action(&g_ui.layout.items[i], pending_action); has_action = true; play(g_snd_buy);
                }
        }
        if (input_key_pressed(KEY_M)) {
            g_ui.overlay = UI_OVL_MUTATION;
            g_ui.ovl_t = 0.0f;
            if (pending_action) { pending_action->type = ACTION_GENERATE_MUTATION_OFFER; has_action = true; }
        }
        if (input_key_pressed(KEY_E)) {
            if (pending_action) { pending_action->type = ACTION_OPEN_END_ERA; has_action = true; }
        }
        if (input_key_pressed(KEY_T)) {
            g_ui.overlay = UI_OVL_TREE;
            g_ui.ovl_t = 0.0f;
        }
        if (input_key_pressed(KEY_C)) {
            g_ui.overlay = UI_OVL_CODEX;
            g_ui.ovl_t = 0.0f;
        }
    }

    if (g_ui.screen == UI_SCREEN_MAIN && g_ui.overlay == UI_OVL_NONE) {
        if(IsKeyPressed(KEY_TAB)) g_ui.branch_view++;
    }
    /* 鼠标指针 */
    if (g_ui.hot >= 0 && g_ui.layout.items[g_ui.hot].enabled) SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    else SetMouseCursor(MOUSE_CURSOR_DEFAULT);

    g_ui.pressed = -1;
    return has_action;
}

/* ================= 结果反馈 ================= */

void ui_notify_result(const GameState *game, GameAction action, ActionResult result)
{
    char msg[192];
    double a = result.amount_spent;

    if (result.code == ACTION_OK) {
        switch (action.type) {
        case ACTION_CLICK_ATP: {
            char num[32];
            ui_fmt_num(a, num, (int)sizeof(num));
            snprintf(msg, sizeof(msg), "+%s ATP", num);
            toast(msg, false, 0.9f);
            g_ui.flash = 1.0f;
            break;
        }
        case ACTION_BUY_RUN_UPGRADE:
            toast("升级完成", false, 1.4f);
            break;
        case ACTION_GENERATE_MUTATION_OFFER:
            toast("候选已生成：本次选择的候选不会刷新", false, 2.4f);
            break;
        case ACTION_CHOOSE_MUTATION: {
            const char *name = ua_mutation_name(result.affected_id);
            snprintf(msg, sizeof(msg), "获得突变：%s", name);
            toast(msg, false, 2.6f);
            play(g_snd_evolve);
            break;
        }
        case ACTION_SPECIATE: {
            const char *name = ua_species_name(result.affected_id);
            snprintf(msg, sizeof(msg), "分化成功：%s", name);
            toast(msg, false, 2.6f);
            play(g_snd_evolve);
            break;
        }
        case ACTION_OPEN_END_ERA:
            toast("已暂停模拟，确认前可以随时返回", false, 2.2f);
            break;
        case ACTION_CANCEL_END_ERA:
            toast("已返回本轮，进度没有任何变化", false, 1.8f);
            break;
        case ACTION_CONFIRM_END_ERA: {
            char num[32];
            ui_fmt_num(a, num, (int)sizeof(num));
            snprintf(msg, sizeof(msg), "结算完成：+%s EP", num);
            toast(msg, false, 3.0f);
            play(g_snd_evolve);
            break;
        }
        case ACTION_BUY_LEGACY: {
            const char *name = ua_legacy_name(result.affected_id);
            snprintf(msg, sizeof(msg), "%s 强化成功", name);
            toast(msg, false, 2.0f);
            break;
        }
        case ACTION_START_NEW_RUN:
            toast("新的纪元开始：资源与突变已重置", false, 3.0f);
            break;
        default:
            break;
        }
        return;
    }

    switch (result.code) {
    case ACTION_INVALID_PHASE:      toast("当前阶段不能执行该操作", true, 2.0f); break;
    case ACTION_INVALID_TARGET:     toast("目标无效", true, 2.0f); break;
    case ACTION_NOT_ENOUGH_ATP:     toast("ATP 不足", true, 1.8f); break;
    case ACTION_NOT_ENOUGH_BIOMASS: toast("生物量不足", true, 1.8f); break;
    case ACTION_NOT_ENOUGH_DNA:     toast("DNA 不足", true, 1.8f); break;
    case ACTION_NOT_ENOUGH_KNOWLEDGE: toast("理念不足", true, 1.8f); break;
    case ACTION_NOT_ENOUGH_EP:      toast("Evolution Points 不足", true, 1.8f); break;
    case ACTION_REQUIREMENT_NOT_MET: toast("前置条件未满足", true, 2.2f); break;
    case ACTION_LEVEL_MAXED:        toast("已经满级", true, 1.8f); break;
    case ACTION_ALREADY_CLAIMED:    toast("本轮奖励已经领取过", true, 2.2f); break;
    default:                        toast("操作未生效", true, 1.8f); break;
    }
    (void)game;
}

/* ================= 绘制 ================= */

static void draw_toast(void)
{
    if (g_ui.toast_t <= 0.0f || !g_ui.toast[0]) return;

    float alpha = 1.0f;
    if (g_ui.toast_t < 0.35f) alpha = g_ui.toast_t / 0.35f;

    float size = F_LABEL;
    float w = ui_text_w(g_ui.toast, size) + 44.0f;
    Rectangle r = { (UI_W - w) * 0.5f, 686.0f, w, 38.0f };
    /* 语义容器底：负向用 COL_BAD_SURFACE，正向用 COL_ACCENT_SURFACE（01 §2.2） */
    Color bg = g_ui.toast_bad ? COL_BAD_SURFACE : COL_ACCENT_SURFACE;
    Color bd = g_ui.toast_bad ? COL_BAD : COL_ACCENT;
    bg.a = (unsigned char)(235 * alpha);
    DrawRectangleRounded(r, 0.6f, 8, bg);
    DrawRectangleRoundedLinesEx(r, 0.6f, 8, W_HAIRLINE, Fade(bd, 0.6f * alpha));
    ui_text_center(g_ui.toast, UI_W * 0.5f, r.y + 11.0f, size, Fade(COL_TEXT, alpha));
}

void ui_render(const GameState *game, const ActionResult *last_result)
{
    float dt = GetFrameTime();
    if (dt > 0.12f) dt = 0.12f;

    g_ui.time += dt;
    if (g_ui.toast_t > 0.0f) g_ui.toast_t -= dt;
    if (g_ui.press_anim > 0.0f) g_ui.press_anim -= dt * 2.2f;
    if (g_ui.press_anim < 0.0f) g_ui.press_anim = 0.0f;
    if (g_ui.flash > 0.0f) g_ui.flash -= dt * 3.0f;
    if (g_ui.flash < 0.0f) g_ui.flash = 0.0f;

    if (g_ui.overlay != UI_OVL_NONE) {
        g_ui.ovl_t += dt * 7.0f;
        if (g_ui.ovl_t > 1.0f) g_ui.ovl_t = 1.0f;
    } else {
        g_ui.ovl_t = 0.0f;
    }

    for (int i = 0; i < g_ui.layout.count; ++i) {
        UiButton *b = &g_ui.layout.items[i];
        float target = (&g_ui.layout.items[i] == (g_ui.pressed >= 0 ? &g_ui.layout.items[g_ui.pressed] : NULL)) ? 1.0f : 0.0f;
        b->press += (target - b->press) * (dt * 14.0f);
    }

    input_begin_frame();

    /* 窗口尺寸变了（拉伸/全屏切换）→ 按新 SS 重建画布 */
    canvas_ensure(compute_ss());

    BeginTextureMode(g_canvas);
    ClearBackground(COL_BG_DEEP); /* 必须在 BeginMode2D 之前：清的是整张 SS 画布 */
    /* 逻辑 1280×800 → 画布像素 x*SS，界面代码不用改。
     * zoom = 整数 SS ⇒ **吃 zoom 的**描边（DrawLineEx / DrawRectangleLinesEx /
     * DrawRing）传 1.0f 落到画布上就是 SS 个设备像素，不会出现 1.5px 半覆盖。
     * 但 DrawRectangleRoundedLinesEx 与 DrawCircleLinesV 是 Z-UNSAFE 原语：线宽按
     * 设备像素算、不吃 zoom，SS=3 时只有 1/3 逻辑像素粗 —— 要用它们画“1 逻辑像素”
     * 的边框或环，请改用 ui_panel 的嵌套填充 / DrawRing（详见 render.c 顶部警告）。 */
    BeginMode2D(ui_camera());
    /* z 序（02 §1.1）：ClearBackground(COL_BG 系) → 静态氛围层 → 内容 → 前景暗角。
     * 三个 atm_* 一律在 BeginMode2D 内、用 1280×800 逻辑坐标（01 §7.3-C1 硬约定 1）：
     * 手动乘 SS 会让静态层的 1 逻辑 px 网格在 4K 下错位。 */
    atm_draw_static(); /* L1 悬液 + L2 刻度：1 次 DrawTexturePro，整张不透明纹理 */
    screens_draw(game, &g_ui, last_result);
    draw_toast();
    atm_draw_vignette(); /* L5 前景暗角：最外 16 逻辑 px，2 层 × α0.020 */
    EndMode2D();
    EndTextureMode();

    float scale = input_ui_scale();
    float ox = input_ui_off_x();
    float oy = input_ui_off_y();

    BeginDrawing();
    ClearBackground(BLACK);
    /* src 取整张画布（SS 倍），dst 仍是 letterbox 矩形 → 由 GPU 下采样。
       鼠标映射（input_mouse_virtual）不受影响，它工作在逻辑坐标系。 */
    Rectangle src = { 0.0f, 0.0f, (float)(UI_W * g_ss), -(float)(UI_H * g_ss) };
    Rectangle dst = { ox, oy, (float)UI_W * scale, (float)UI_H * scale };
    DrawTexturePro(g_canvas.texture, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
    EndDrawing();
}

/* ================= 调试入口 ================= */

void ui_debug_force(UiScreen screen, UiOverlay overlay)
{
    g_ui.screen = screen;
    g_ui.overlay = overlay;
    g_ui.ovl_t = overlay != UI_OVL_NONE ? 1.0f : 0.0f;
}

/* 动作映射自检：把“按钮 → GameAction”的对应关系暴露给测试，避免字段填错没人发现 */
bool ui_debug_map_button(int id, int target, int option, GameAction *out)
{
    UiButton b;
    memset(&b, 0, sizeof(b));
    b.id = (UiButtonId)id;
    b.target_id = target;
    b.option = option;
    if (!action_needs_game(b.id)) return false;
    fill_action(&b, out);
    return true;
}

void ui_debug_set_codex_tab(int tab) { g_ui.codex_tab = tab; }
void ui_debug_set_keep_parent(int v) { g_ui.keep_parent = v; }

void ui_debug_capture(const GameState *game, const char *path)
{
    g_ui.time = 2.0f;
    g_ui.toast_t = 0.0f;
    screens_build(&g_ui.layout, game, &g_ui);
    g_ui.hot = -1;

    /* --shot 模式下 SS 由命令行强制（默认 3）→ 导出的就是 3840×2400 真 4K */
    canvas_ensure(compute_ss());
    atm_ensure(); /* 幂等：SS 未变时只做一次 int 比较；变了则按新 SS 重烧 */

    /* ⚠ 这里必须与 ui_render() 的 z 序逐字一致。截图读的是 g_canvas，
     * 谁漏在上面就等于"玩家看得见、PNG 里没有"（暗角尤其容易漏）。 */
    BeginTextureMode(g_canvas);
    ClearBackground(COL_BG_DEEP);
    BeginMode2D(ui_camera());
    atm_draw_static();
    screens_draw(game, &g_ui, NULL);
    atm_draw_vignette();
    /* 这一行是验收入口：4K PNG 里"有没有静态层与暗角"只能靠它证，
     * 而不是靠肉眼看（两者都是 CR ≤1.10 的极淡层）。主界面应为 16。 */
    TraceLog(LOG_INFO, "CAPTURE %s: atm draw calls=%d", path, atm_last_call_count());
    EndMode2D();
    EndTextureMode();

    Image image = LoadImageFromTexture(g_canvas.texture);
    ImageFlipVertical(&image);
    ExportImage(image, path);
    UnloadImage(image);
}

void ui_report_save(bool success) { if(!success) toast("存档失败，请检查目录权限；退出前可重试",true,5.0f); }
