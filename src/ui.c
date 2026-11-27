/*
 * ui.c  ——  UI 状态机、超采样画布、点击粒子与提示条（Member C）
 *
 * 全文件唯一允许写游戏数据的地方：把玩家意图打包成 GameAction 交给 A。
 * 这里没有一处 game->run.xxx = ...，也没有一处资源公式。
 *
 * 画布：逻辑 1280×720（16:9），画布 = UI_W*SS × UI_H*SS，SS 为整数。
 *   SS=1 → 1280×720，SS=2 → 2560×1440，SS=3 → 3840×2160（真 4K UHD，截图用）。
 *
 * 爽感系统（本轮新增）：
 *   · punch      —— 点击时主体膨胀 5%，0.35s 内回弹（Cookie Clicker 的“捏一下”）
 *   · ui_fx_*    —— 点击炸出的 18 粒荧光星火，加法混合、带拖尾与重力
 *   · gain_t     —— 皿上浮出的 "+N ATP"，上升 40px 后淡出
 *   三者都不写游戏状态，只活在 g_ui 里。
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "ui.h"
#include "ui_internal.h"
#include "ui_adapter.h"
#include "embedded.h"
#include "atmosphere.h"
#include "codex.h"

UIState g_ui;

static RenderTexture2D g_canvas;
static Sound g_snd_click = { 0 };
static Sound g_snd_buy = { 0 };
static Sound g_snd_evolve = { 0 };
static bool g_audio = false;

/* 音效加载：内嵌优先（单文件版），否则退回文件路径。
 * 波形先解到内存再转 Sound，解完立刻 UnloadWave —— 与 LoadSound 内部一致。
 * 缺任何一个都不算错：静音不影响玩法（原代码也是用 FileExists 挡一层）。 */
static Sound load_sound(EmbId id, const char *path)
{
    const unsigned char *data = NULL;
    int size = 0;
    Wave w = { 0 };
    if (emb_get(id, &data, &size)) {
        w = LoadWaveFromMemory(".wav", data, size);
    } else if (FileExists(path)) {
        w = LoadWave(path);
    }
    if (!IsWaveValid(w)) return (Sound){ 0 };
    Sound snd = LoadSoundFromWave(w);
    UnloadWave(w);
    return snd;
}

/* ================= 超采样画布 ================= */

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
 * 减一个极小量再取整：避免 2560×1440 这类恰好整数倍的档位被浮点误差抬高一档。 */
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
    if (ss > UI_SS_MAX) ss = UI_SS_MAX;
    return ss;
}

static void canvas_ensure(int ss)
{
    if (ss < UI_SS_MIN) ss = UI_SS_MIN;
    if (ss > UI_SS_MAX) ss = UI_SS_MAX;
    if (ss == g_ss && IsRenderTextureValid(g_canvas)) return;

    if (IsRenderTextureValid(g_canvas)) UnloadRenderTexture(g_canvas);

    g_canvas = LoadRenderTexture(UI_W * ss, UI_H * ss);
    /* 老旧显卡建不出 3840×2160 的 FBO：宁可退回 SS=1 也不能黑屏。 */
    if (!IsRenderTextureValid(g_canvas) && ss > UI_SS_MIN) {
        TraceLog(LOG_WARNING, "UI canvas: SS=%d (%dx%d) failed, falling back to SS=%d",
                 ss, UI_W * ss, UI_H * ss, UI_SS_MIN);
        ss = UI_SS_MIN;
        g_canvas = LoadRenderTexture(UI_W * ss, UI_H * ss);
    }
    SetTextureFilter(g_canvas.texture, TEXTURE_FILTER_BILINEAR);
    g_ss = ss;

    /* 氛围层按新 SS 重烧（内部只做一次 int 比较） */
    atm_ensure();

    TraceLog(LOG_INFO, "UI canvas: %dx%d (SS=%d) for window %dx%d",
             UI_W * ss, UI_H * ss, ss, GetScreenWidth(), GetScreenHeight());
}

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
    case UI_BTN_UPGRADE:
    case UI_BTN_OPEN_MUTATION:
    case UI_BTN_CHOOSE_MUTATION:
    case UI_BTN_SPECIATE:
    case UI_BTN_OPEN_END_ERA:
    case UI_BTN_CANCEL_END_ERA:
    case UI_BTN_CONFIRM_END_ERA:
    case UI_BTN_BUY_LEGACY:
    case UI_BTN_START_NEW_RUN:
    case UI_BTN_COLLECT_BUFF:
    case UI_BTN_BUY_INSIGHT:
    case UI_BTN_BUY_CAPACITY:
    case UI_BTN_BUY_RESERVE:
        return true;
    default:
        return false;
    }
}

static void fill_action(const UiButton *b, GameAction *out)
{
    memset(out, 0, sizeof(*out));
    switch (b->id) {
    case UI_BTN_COLLECT_BUFF:     out->type = ACTION_COLLECT_BUFF; break;
    case UI_BTN_BUY_INSIGHT:      out->type = ACTION_BUY_INSIGHT; break;
    case UI_BTN_BUY_CAPACITY:     out->type = ACTION_BUY_CAPACITY; break;
    case UI_BTN_BUY_RESERVE:      out->type = ACTION_BUY_RESERVE; break;
    case UI_BTN_CLICK:            out->type = ACTION_CLICK_ATP; break;
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
void ui_handle_local_button(const UiButton *b, const GameState *game)
{
    if (!b || !game || !b->enabled) return;
    switch (b->id) {
    case UI_BTN_OPEN_SPECIATION: {
        if (!b->enabled) {
            toast("还没有可用的分化方向：先通过突变满足前置条件", true, 3.0f);
            break;
        }
        g_ui.overlay = UI_OVL_SPECIATION;
        g_ui.speciation_page = 0;
        int children[SPECIES_COUNT];
        int n = ua_speciation_children(game, children, SPECIES_COUNT);
        for (int i = 0; i < n; ++i)
            if (children[i] == b->target_id) g_ui.speciation_page = i / UI_SPECIATION_PAGE_SIZE;
        g_ui.ovl_t = 0.0f;
        g_ui.keep_parent = (game->legacy.levels[LEGACY_DIVERSIFICATION] >= 2) ? 1 : 0;
        break;
    }
    case UI_BTN_TOGGLE_KEEP_PARENT:
        g_ui.keep_parent = g_ui.keep_parent ? 0 : 1;
        break;
    case UI_BTN_SPECIATION_PAGE:
        g_ui.speciation_page = b->option;
        break;
    case UI_BTN_OPEN_CODEX:
        g_ui.overlay = UI_OVL_CODEX;
        g_ui.ovl_t = 0.0f;
        break;
    case UI_BTN_OPEN_TREE:
        g_ui.overlay = UI_OVL_TREE;
        g_ui.ovl_t = 0.0f;
        break;
    case UI_BTN_OPEN_MUT_LIST:
        g_ui.overlay = UI_OVL_MUT_LIST;
        g_ui.ovl_t = 0.0f;
        break;
    case UI_BTN_TREE_NODE:
        /* 再点一次已选中的节点 = 取消选中，回到跟随主谱系。
         * 只改 g_ui，不碰 GameState —— 选中只是“看哪条路线”，不是玩法动作。 */
        g_ui.tree_sel = (g_ui.tree_sel == b->target_id) ? -1 : b->target_id;
        break;
    case UI_BTN_TREE_SKY:
        /* 空白区的按下由拖拽逻辑处理（见 ui_render），这里不发动作、不关弹层 */
        break;
    case UI_BTN_TREE_RESET:
        g_ui.tree_pan_x = 0.0f;
        g_ui.tree_pan_y = 0.0f;
        g_ui.tree_zoom = 1.0f;
        break;
    case UI_BTN_CODEX_TAB:
        if (b->option < 0 || b->option > 2) break;
        g_ui.codex_tab = b->option;
        g_ui.codex_page = 0;
        break;
    case UI_BTN_CODEX_PAGE: {
        int count = codex_count((CodexKind)g_ui.codex_tab);
        int last = count > 0 ? (count - 1) / UI_CODEX_PAGE_SIZE : 0;
        if (b->option >= 0 && b->option <= last) g_ui.codex_page = b->option;
        break;
    }
    case UI_BTN_CODEX_ENTRY: {
        const CodexEntry *entry = codex_entry((CodexKind)b->option, b->target_id);
        if (!entry) break;
        bool found = b->option == CODEX_SPECIES
                   ? (game->legacy.codex_species[b->target_id] || game->run.discovered_species[b->target_id])
                   : (game->legacy.codex_mutations[b->target_id] || game->run.discovered_mutations[b->target_id]);
        if (!found) break;
        g_ui.codex_detail_kind = b->option;
        g_ui.codex_detail_id = b->target_id;
        g_ui.overlay = UI_OVL_CODEX_DETAIL;
        g_ui.ovl_t = 0.0f;
        break;
    }
    case UI_BTN_CODEX_BACK:
        g_ui.overlay = UI_OVL_CODEX;
        break;
    case UI_BTN_STORE_TAB:
        g_ui.store_tab = b->option;
        break;
    case UI_BTN_INFO_TAB:
        g_ui.info_tab = b->option;
        break;
    case UI_BTN_CLOSE_OVERLAY:
    case UI_BTN_BACKDROP:
        ui_dismiss_overlay();
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

void ui_dismiss_overlay(void)
{
    if (g_ui.overlay == UI_OVL_CODEX_DETAIL) g_ui.overlay = UI_OVL_CODEX;
    else close_overlay_now();
}

/* ================= 点击粒子（爽感系统） ================= */

void ui_fx_burst(Vector2 c, Color col, int count, float speed)
{
    if (count <= 0) return;
    for (int i = 0; i < count; ++i) {
        if (g_ui.fx_n >= UI_FX_MAX) {
            /* 池满：复用最老的槽位（环形覆盖），保证连点也不会“没反应” */
            g_ui.fx_n = 0;
        }
        UiFx *f = &g_ui.fx[g_ui.fx_n];
        float ang = ((float)rand() / (float)RAND_MAX) * 2.0f * PI;
        float v = speed * (0.35f + 0.65f * ((float)rand() / (float)RAND_MAX));
        f->x = c.x + cosf(ang) * 6.0f;
        f->y = c.y + sinf(ang) * 6.0f;
        f->vx = cosf(ang) * v;
        f->vy = sinf(ang) * v - speed * 0.25f;
        f->ttl = 0.55f + 0.55f * ((float)rand() / (float)RAND_MAX);
        f->t = 1.0f;
        f->size = 1.4f + 2.4f * ((float)rand() / (float)RAND_MAX);
        f->drag = 3.0f + 5.0f * ((float)rand() / (float)RAND_MAX);
        f->shape = (int)(rand() % 3);
        f->col = col;
        if ((rand() % 5) == 0) f->col = COL_TEXT;
        g_ui.fx_n++;
    }
}

void ui_fx_tick(float dt)
{
    for (int i = 0; i < g_ui.fx_n; ++i) {
        UiFx *f = &g_ui.fx[i];
        if (f->t <= 0.0f) continue;
        f->x += f->vx * dt;
        f->y += f->vy * dt;
        float k = 1.0f - f->drag * dt;
        if (k < 0.0f) k = 0.0f;
        f->vx *= k;
        f->vy = f->vy * k + 40.0f * dt;   /* 一点点重力，星火最后会“落下” */
        f->t -= dt / (f->ttl > 0.02f ? f->ttl : 0.02f);
        if (f->t < 0.0f) f->t = 0.0f;
    }
    /* 压缩：全部熄灭后清空计数，避免每帧遍历 96 个空槽 */
    int alive = 0;
    for (int i = 0; i < g_ui.fx_n; ++i) {
        if (g_ui.fx[i].t > 0.0f) g_ui.fx[++alive - 1] = g_ui.fx[i];
    }
    g_ui.fx_n = alive;
}

void ui_fx_draw(Vector2 c, float radius)
{
    if (g_ui.fx_n <= 0) return;
    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < g_ui.fx_n; ++i) {
        const UiFx *f = &g_ui.fx[i];
        if (f->t <= 0.0f) continue;
        float dx = f->x - c.x, dy = f->y - c.y;
        float d2 = dx * dx + dy * dy;
        float a = f->t * f->t;
        /* 出了培养皿就淡掉：粒子是皿里的东西，不该糊到面板外面 */
        float lim = radius * 1.10f;
        if (d2 > lim * lim) a *= 0.35f;

        if (f->shape == 0) {
            DrawCircleV((Vector2){ f->x, f->y }, f->size * (0.4f + 0.6f * f->t),
                        Fade(f->col, 0.95f * a));
        } else if (f->shape == 1) {
            float len = f->size * 2.6f * (0.3f + 0.7f * f->t);
            DrawLineEx((Vector2){ f->x - len, f->y }, (Vector2){ f->x + len, f->y }, 1.2f,
                       Fade(f->col, 0.75f * a));
            DrawLineEx((Vector2){ f->x, f->y - len }, (Vector2){ f->x, f->y + len }, 1.2f,
                       Fade(f->col, 0.75f * a));
        } else {
            /* 拖尾：沿速度方向的反方向拉一条短线 */
            float vx = f->vx, vy = f->vy;
            float m = sqrtf(vx * vx + vy * vy) + 1e-3f;
            float len = f->size * 2.2f;
            DrawLineEx((Vector2){ f->x, f->y },
                       (Vector2){ f->x - vx / m * len, f->y - vy / m * len }, 1.4f,
                       Fade(f->col, 0.85f * a));
        }
    }
    EndBlendMode();
}

/* 点击时统一触发：膨胀 + 星火 + 闪光 + 音效 */
static void click_feedback(void)
{
    g_ui.punch = 1.0f;
    g_ui.flash = 0.9f;
    g_ui.press_anim = 1.0f;
    ui_fx_burst((Vector2){ 194.0f, 322.0f }, COL_ATP, 18, 150.0f);
    play(g_snd_click);
}

/* ================= 产出曲线采样 / 成就监听 ================= */

#define HIST_N 72
#define HIST_DT 0.25f

/* 每 0.25 秒记一次三条资源的存量，环形滚动。第一次调用先把整条铺平，
 * 面板一出现就是“满的曲线”，之后新点从右边挤进来。 */
static void hist_sample(const GameState *game, float dt)
{
    double cur[3] = { game->run.atp, game->run.biomass, game->run.dna };

    if (g_ui.hist_n <= 0) {
        for (int s = 0; s < 3; ++s)
            for (int i = 0; i < HIST_N; ++i) g_ui.hist[s][i] = (float)cur[s];
        g_ui.hist_n = HIST_N;
        g_ui.hist_acc = 0.0f;
        return;
    }

    g_ui.hist_acc += dt;
    if (g_ui.hist_acc < HIST_DT) return;
    g_ui.hist_acc = 0.0f;

    for (int s = 0; s < 3; ++s) {
        memmove(&g_ui.hist[s][0], &g_ui.hist[s][1], (size_t)(HIST_N - 1) * sizeof(float));
        g_ui.hist[s][HIST_N - 1] = (float)cur[s];
    }
}

/* 成就解锁 → 一条提示。不写 GameState，只比对 ua_achievement_unlocked。 */
static void check_achievements(const GameState *game)
{
    static bool primed = false;
    int n = ua_achievement_count();
    if (n > 8) n = 8;

    for (int i = 0; i < n; ++i) {
        bool un = ua_achievement_unlocked(game, i);
        if (un && !g_ui.ach_seen[i]) {
            g_ui.ach_seen[i] = 1;
            if (primed) {
                char msg[192];
                snprintf(msg, sizeof(msg), "成就解锁：%s", ua_achievement_name(i));
                toast(msg, false, 3.2f);
                play(g_snd_evolve);
            }
        } else if (!un) {
            g_ui.ach_seen[i] = 0;
        }
    }
    primed = true;
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
    g_ui.tree_sel = -1;
    g_ui.tree_zoom = 1.0f;      /* memset 是 0，不显式设会让星图塌成一个点 */
    g_ui.prev_env_id = -1;
    g_ui.crisis_env = -1;

    if (emb_any()) TraceLog(LOG_INFO, "UI: single-file build, assets embedded");
    ui_font_init();

    g_ss = 0;
    canvas_ensure(compute_ss());

    if (IsAudioDeviceReady()) {
        g_snd_click  = load_sound(EMB_CLICK,  "assets/sfx/click.wav");
        g_snd_buy    = load_sound(EMB_BUY,    "assets/sfx/buy.wav");
        g_snd_evolve = load_sound(EMB_EVOLVE, "assets/sfx/evolve.wav");
        g_audio = IsSoundValid(g_snd_click);
    }

    toast("点击培养皿开始积累 ATP", false, 3.0f);
}

void ui_shutdown(void)
{
    if (g_audio) {
        if (IsSoundValid(g_snd_click)) UnloadSound(g_snd_click);
        if (IsSoundValid(g_snd_buy)) UnloadSound(g_snd_buy);
        if (IsSoundValid(g_snd_evolve)) UnloadSound(g_snd_evolve);
    }
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
    if (b->id == UI_BTN_NONE) return;
    g_ui.pressed = index;

    if (action_needs_game(b->id)) {
        fill_action(b, out);
        *has_action = true;
        if (b->id == UI_BTN_OPEN_MUTATION) {
            g_ui.overlay = UI_OVL_MUTATION;
            g_ui.ovl_t = 0.0f;
        }
    } else {
        ui_handle_local_button(b, game);
    }

    if (b->id == UI_BTN_CLICK) click_feedback();
    else play(g_snd_buy);
}

bool ui_update(const GameState *game, GameAction *pending_action)
{
    bool has_action = false;
    if (pending_action) memset(pending_action, 0, sizeof(*pending_action));

    input_begin_frame();

    hist_sample(game, GetFrameTime());
    check_achievements(game);

    /* 阶段变化 → 切页面并关弹层（phase 是唯一权威） */
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

    /* ---- 滚轮：星图缩放（以鼠标为锚点，指到哪个节点放大后它还在指针下） ---- */
    if (g_ui.overlay == UI_OVL_TREE) {
        float wheel = input_wheel();
        if (wheel != 0.0f) {
            Vector2 m = input_mouse_virtual();
            Vector2 c = screens_tree_center();
            float old = g_ui.tree_zoom;
            float nz = old * (1.0f + wheel * 0.12f);
            if (nz < 0.75f) nz = 0.75f;
            if (nz > 2.80f) nz = 2.80f;
            if (nz != old) {
                float tx = m.x - c.x - g_ui.tree_pan_x;
                float ty = m.y - c.y - g_ui.tree_pan_y;
                g_ui.tree_pan_x += tx - tx * (nz / old);
                g_ui.tree_pan_y += ty - ty * (nz / old);
                g_ui.tree_zoom = nz;
            }
        }
    }

    /* ---- 星图拖拽平移：只有从空白区按下才算拖，点节点仍是选中 ---- */
    if (g_ui.overlay == UI_OVL_TREE) {
        if (input_click_pressed() && g_ui.hot >= 0
            && g_ui.layout.items[g_ui.hot].id == UI_BTN_TREE_SKY) {
            g_ui.tree_dragging = true;
        }
        if (g_ui.tree_dragging && input_click_down()) {
            Vector2 d = input_mouse_delta();
            g_ui.tree_pan_x += d.x;
            g_ui.tree_pan_y += d.y;
        }
        if (!input_click_down()) g_ui.tree_dragging = false;

        /* 夹住平移量：186 个节点的星图很容易一拖就拖到画框外 */
        float lim = screens_tree_pan_limit(game);
        if (g_ui.tree_pan_x > lim) g_ui.tree_pan_x = lim;
        if (g_ui.tree_pan_x < -lim) g_ui.tree_pan_x = -lim;
        if (g_ui.tree_pan_y > lim) g_ui.tree_pan_y = lim;
        if (g_ui.tree_pan_y < -lim) g_ui.tree_pan_y = -lim;
    } else {
        g_ui.tree_dragging = false;
    }

    /* 鼠标 */
    if (input_click_pressed()) {
        if (g_ui.hot >= 0) {
            UiButton *b = &g_ui.layout.items[g_ui.hot];
            if (b->enabled) {
                fire(g_ui.hot, game, pending_action, &has_action);
            } else {
                if (b->id == UI_BTN_CODEX_ENTRY)
                    toast("尚未发现该条目，继续探索后收录", false, 2.0f);
                else if (b->id == UI_BTN_CODEX_PAGE)
                    toast("已经到达这一侧的最后一页", false, 1.5f);
                else toast("条件不足：资源不够或前置未满足", true, 2.0f);
            }
        }
    }

    /* 键盘快捷键 */
    if (g_ui.overlay != UI_OVL_NONE) {
        if (input_key_pressed(KEY_ESCAPE)) ui_dismiss_overlay();
    } else if (g_ui.screen == UI_SCREEN_MAIN) {
        if (input_key_pressed(KEY_SPACE)) {
            if (pending_action) { pending_action->type = ACTION_CLICK_ATP; has_action = true; }
            click_feedback();
        }
        int up = -1;
        if (input_key_pressed(KEY_ONE) || input_key_pressed(KEY_KP_1)) up = 0;
        if (input_key_pressed(KEY_TWO) || input_key_pressed(KEY_KP_2)) up = 1;
        if (input_key_pressed(KEY_THREE) || input_key_pressed(KEY_KP_3)) up = 2;
        if (up >= 0) {
            for (int i = 0; i < g_ui.layout.count; ++i) {
                UiButton *b = &g_ui.layout.items[i];
                if (b->id == UI_BTN_UPGRADE && b->target_id == up && b->enabled && pending_action) {
                    fill_action(b, pending_action);
                    has_action = true;
                    play(g_snd_buy);
                    break;
                }
            }
        }
        if (pending_action && (input_key_pressed(KEY_FOUR) || input_key_pressed(KEY_FIVE) || input_key_pressed(KEY_SIX))) {
            pending_action->type = input_key_pressed(KEY_FOUR) ? ACTION_BUY_CAPACITY
                                 : input_key_pressed(KEY_FIVE) ? ACTION_BUY_RESERVE : ACTION_BUY_SUB_UPGRADE;
            pending_action->target_species_slot = 0;
            has_action = true;
        }
        if (input_key_pressed(KEY_M)) {
            g_ui.overlay = UI_OVL_MUTATION;
            g_ui.ovl_t = 0.0f;
            if (pending_action) { pending_action->type = ACTION_GENERATE_MUTATION_OFFER; has_action = true; }
        }
        if (pending_action && input_key_pressed(KEY_SEVEN)) {
            pending_action->type = ACTION_BUY_INSIGHT;
            has_action = true;
        }
        if (pending_action && input_key_pressed(KEY_B)) {
            pending_action->type = ACTION_COLLECT_BUFF;
            has_action = true;
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
        if (input_key_pressed(KEY_V)) {
            g_ui.overlay = UI_OVL_MUT_LIST;
            g_ui.ovl_t = 0.0f;
        }
        if (input_key_pressed(KEY_TAB)) {
            g_ui.store_tab = g_ui.store_tab ? 0 : 1;
        }
    }

    /* 鼠标指针 */
    if (g_ui.hot >= 0 && g_ui.layout.items[g_ui.hot].enabled
        && g_ui.layout.items[g_ui.hot].id != UI_BTN_NONE
        && g_ui.layout.items[g_ui.hot].id != UI_BTN_BACKDROP) SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
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
            /* 采集反馈走皿上的漂浮数字，不进全局提示条 */
            int slot = g_ui.gain_slot++ % 3;
            /* Clicks earn ATP; amount_spent is zero for this action. */
            g_ui.gain_val[slot] = ua_click_atp(game);
            g_ui.gain_t[slot] = 1.0f;
            break;
        }
        case ACTION_BUY_RUN_UPGRADE:
            toast("升级完成", false, 1.4f);
            break;
        case ACTION_COLLECT_BUFF:
            toast("已领取随机奖励", false, 2.0f);
            break;
        case ACTION_BUY_INSIGHT:
            toast("洞见升级完成，全产率提升", false, 2.0f);
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
    case ACTION_INVALID_PHASE:       toast("当前阶段不能执行该操作", true, 2.0f); break;
    case ACTION_INVALID_TARGET:      toast("目标无效", true, 2.0f); break;
    case ACTION_NOT_ENOUGH_ATP:      toast("ATP 不足", true, 1.8f); break;
    case ACTION_NOT_ENOUGH_BIOMASS:  toast("生物量不足", true, 1.8f); break;
    case ACTION_NOT_ENOUGH_DNA:      toast("DNA 不足", true, 1.8f); break;
    case ACTION_NOT_ENOUGH_EP:       toast("Evolution Points 不足", true, 1.8f); break;
    case ACTION_NOT_ENOUGH_KNOWLEDGE: toast("理念不足", true, 1.8f); break;
    case ACTION_REQUIREMENT_NOT_MET: toast("前置条件未满足", true, 2.2f); break;
    case ACTION_LEVEL_MAXED:         toast("已经满级", true, 1.8f); break;
    case ACTION_ALREADY_CLAIMED:     toast("本轮奖励已经领取过", true, 2.2f); break;
    default:                         toast("操作未生效", true, 1.8f); break;
    }
    (void)game;
}

/* ================= 绘制 ================= */

/* 提示条：底栏右侧空白带，右对齐到内容右边距。
 * 三个动作按钮 16..660，提示条 680..1264 —— 两者永不重叠；
 * 提示条出现时底栏的键位提示会隐藏（screens.c 里判断 toast_t）。 */
static void draw_toast(void)
{
    if (g_ui.toast_t <= 0.0f || !g_ui.toast[0]) return;

    float alpha = 1.0f;
    if (g_ui.toast_t < 0.35f) alpha = g_ui.toast_t / 0.35f;

    float size = F_BODY;
    float w = ui_text_w(g_ui.toast, size) + 54.0f;
    float max_w = 1264.0f - 680.0f;
    if (w > max_w) {
        size = F_CAPTION;
        w = ui_text_w(g_ui.toast, size) + 46.0f;
        if (w > max_w) w = max_w;
    }
    Rectangle r = { 1264.0f - w, 660.0f, w, 48.0f };

    Color bd = g_ui.toast_bad ? COL_BAD : COL_ACCENT;
    Color bg = g_ui.toast_bad ? COL_BAD_SURFACE : COL_ACCENT_SURFACE;
    ui_panel(r, R_BTN, Fade(bg, 0.92f * alpha), Fade(bd, 0.75f * alpha));
    DrawCircleV((Vector2){ r.x + 16.0f, r.y + 24.0f }, 4.0f, Fade(bd, alpha));
    ui_text(g_ui.toast, r.x + 27.0f, r.y + (r.height - size) * 0.5f - 1.0f, size,
            Fade(COL_TEXT, alpha));
}

/* ================= 危机监听 ================= */

/* 抖屏幅度（逻辑像素）。陨石最狠，其次是太阳耀斑；生态类的只是“轻轻一震”。 */
static float crisis_shake_amp(int env)
{
    switch (env) {
    case ENV_METEOR_IMPACT:   return 15.0f;
    case ENV_SOLAR_FLARE:     return 9.0f;
    case ENV_TOXIC_BLOOM:     return 5.0f;
    case ENV_GLOBAL_WARMING:
    case ENV_GLOBAL_COOLING:  return 4.0f;
    default:                  return 6.0f;
    }
}

/* 只在“环境 id 发生变化且新环境是危机”的那一帧触发一次。
 * 中途读档读到危机里也不补演（prev_env_id 未初始化时只记基线）。 */
static void crisis_watch(const GameState *game)
{
    if (game->phase != PHASE_RUNNING) {
        g_ui.prev_env_id = -1;
        return;
    }
    int env = game->run.current_environment_id;
    if (g_ui.prev_env_id < 0) { g_ui.prev_env_id = env; return; }
    if (env == g_ui.prev_env_id) return;
    g_ui.prev_env_id = env;

    double rem = 0.0, tot = 1.0;
    bool in_event = false;
    ua_environment_timer(game, &rem, &tot, &in_event);
    if (!in_event || !ua_environment_is_crisis(env)) return;

    g_ui.crisis_env = env;
    g_ui.crisis_t = 2.2f;
    g_ui.shake = 1.0f;
    g_ui.crisis_flash = 1.0f;

    char msg[96];
    snprintf(msg, sizeof(msg), "危机：%s", ua_environment_name(env));
    toast(msg, true, 3.0f);
    play(g_snd_evolve);
}

void ui_render(const GameState *game, const ActionResult *last_result)
{
    float dt = GetFrameTime();
    if (dt > 0.12f) dt = 0.12f;

    g_ui.time += dt;
    if (g_ui.toast_t > 0.0f) g_ui.toast_t -= dt;

    /* 采集浮字：1 → 0 走完约 0.7s */
    for (int i = 0; i < 3; ++i) {
        if (g_ui.gain_t[i] > 0.0f) g_ui.gain_t[i] -= dt * 1.45f;
        if (g_ui.gain_t[i] < 0.0f) g_ui.gain_t[i] = 0.0f;
    }
    /* 膨胀回弹：0.35s 内从 1 掉到 0，衰减指数让“弹”更有弹性 */
    if (g_ui.punch > 0.0f) {
        g_ui.punch -= dt * 2.9f;
        if (g_ui.punch < 0.0f) g_ui.punch = 0.0f;
    }
    if (g_ui.press_anim > 0.0f) g_ui.press_anim -= dt * 2.2f;
    if (g_ui.press_anim < 0.0f) g_ui.press_anim = 0.0f;
    if (g_ui.flash > 0.0f) g_ui.flash -= dt * 3.4f;
    if (g_ui.flash < 0.0f) g_ui.flash = 0.0f;
    /* 危机演出的两条包络：演出本体降得慢（看得见），闪光降得快（一下就过） */
    if (g_ui.crisis_t > 0.0f) {
        g_ui.crisis_t -= dt * 1.15f;
        if (g_ui.crisis_t < 0.0f) g_ui.crisis_t = 0.0f;
    }
    if (g_ui.crisis_flash > 0.0f) {
        g_ui.crisis_flash -= dt * 1.8f;
        if (g_ui.crisis_flash < 0.0f) g_ui.crisis_flash = 0.0f;
    }

    crisis_watch(game);

    ui_fx_tick(dt);

    if (g_ui.overlay != UI_OVL_NONE) {
        g_ui.ovl_t += dt * 7.0f;
        if (g_ui.ovl_t > 1.0f) g_ui.ovl_t = 1.0f;
    } else {
        g_ui.ovl_t = 0.0f;
    }

    for (int i = 0; i < g_ui.layout.count; ++i) {
        UiButton *b = &g_ui.layout.items[i];
        float target = (g_ui.pressed >= 0 && &g_ui.layout.items[g_ui.pressed] == b) ? 1.0f : 0.0f;
        b->press += (target - b->press) * (dt * 14.0f);
    }

    input_begin_frame();

    /* 窗口尺寸变了 → 按新 SS 重建画布 */
    canvas_ensure(compute_ss());

    BeginTextureMode(g_canvas);
    ClearBackground(COL_BG_DEEP);
    BeginMode2D(ui_camera());
    /* z 序：静态星云 → 页面内容 → 提示条 → 前景暗角 */
    atm_draw_static();
    screens_draw(game, &g_ui, last_result);
    draw_toast();
    atm_draw_vignette();
    EndMode2D();
    EndTextureMode();

    float scale = input_ui_scale();
    float ox = input_ui_off_x();
    float oy = input_ui_off_y();

    /* 抖屏：整帧平移。dst 同时向外撑开，避免平移时露出黑边。
     * 这里不重画任何内容 —— 只是把已经合成好的画布整体挪几个像素，
     * 所以 DOM 式布局不会因为抖而错位。 */
    float sx = 0.0f, sy = 0.0f;
    if (g_ui.crisis_t > 0.0f && g_ui.crisis_env >= 0) {
        float k = g_ui.crisis_t / 2.2f;
        k = k * k;
        float amp = crisis_shake_amp(g_ui.crisis_env) * k;
        float t = g_ui.time;
        sx = (sinf(t * 57.0f) + 0.45f * sinf(t * 31.0f)) * amp;
        sy = (cosf(t * 49.0f) + 0.45f * cosf(t * 67.0f)) * amp;
    }
    float sxd = sx * scale, syd = sy * scale;
    float pad = fmaxf(fabsf(sxd), fabsf(syd)) + 1.0f;

    BeginDrawing();
    ClearBackground(COL_BG_DEEP);
    Rectangle src = { 0.0f, 0.0f, (float)(UI_W * g_ss), -(float)(UI_H * g_ss) };
    Rectangle dst = { ox + sxd - pad, oy + syd - pad,
                      (float)UI_W * scale + pad * 2.0f, (float)UI_H * scale + pad * 2.0f };
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
void ui_debug_set_store_tab(int tab) { g_ui.store_tab = tab; }
void ui_debug_set_info_tab(int tab) { g_ui.info_tab = tab; }

/* 截图用：模拟一次采集成功。gain_t 固定在 0.62，浮字停在“刚冒出来”的高度；
 * 粒子的 t 也定格在半程，PNG 里能看到完整的星火形状。 */
void ui_debug_poke_click(double gain)
{
    if (gain < 0.0) {
        for (int i = 0; i < 3; ++i) g_ui.gain_t[i] = 0.0f;
        g_ui.flash = 0.0f;
        g_ui.press_anim = 0.0f;
        g_ui.punch = 0.0f;
        g_ui.fx_n = 0;
        return;
    }
    g_ui.gain_val[0] = gain;
    g_ui.gain_t[0] = 0.62f;
    g_ui.flash = 0.55f;
    g_ui.press_anim = 0.65f;
    g_ui.punch = 0.55f;

    ui_fx_burst((Vector2){ 194.0f, 322.0f }, COL_ATP, 26, 190.0f);
    /* 手工把粒子推进 0.18s：导出瞬间正好是“炸开但还没落地”的最佳形态 */
    for (int i = 0; i < g_ui.fx_n; ++i) {
        UiFx *f = &g_ui.fx[i];
        f->x += f->vx * 0.18f;
        f->y += f->vy * 0.18f;
        f->t = 0.72f;
    }
}

void ui_debug_poke_toast(const char *text, bool bad) { toast(text, bad, 6.0f); }

void ui_debug_poke_crisis(int env_id, float t)
{
    g_ui.crisis_env = env_id;
    g_ui.crisis_t = t;
    g_ui.crisis_flash = 1.0f;
    g_ui.shake = 1.0f;
    g_ui.prev_env_id = env_id;   /* 免得下一帧 crisis_watch 又触发一次 */
}

void ui_debug_capture(const GameState *game, const char *path)
{
    g_ui.time = 2.0f;
    screens_build(&g_ui.layout, game, &g_ui);
    g_ui.hot = -1;

    /* --shot 模式下 SS 由命令行强制（默认 3）→ 3840×2160 真 4K */
    canvas_ensure(compute_ss());
    atm_ensure();

    /* ⚠ 必须与 ui_render() 的 z 序逐字一致：谁漏在上面，就是“玩家看得见、PNG 里没有” */
    BeginTextureMode(g_canvas);
    ClearBackground(COL_BG_DEEP);
    BeginMode2D(ui_camera());
    atm_draw_static();
    screens_draw(game, &g_ui, NULL);
    draw_toast();
    atm_draw_vignette();
    TraceLog(LOG_INFO, "CAPTURE %s: atm draw calls=%d", path, atm_last_call_count());
    EndMode2D();
    EndTextureMode();

    Image image = LoadImageFromTexture(g_canvas.texture);
    ImageFlipVertical(&image);

    /* ⚠ 必须把 alpha 强制为 255。
     * 画布是 RGBA 帧缓冲：凡是 alpha<1 的层（暗角、柔光、玻璃渐变）都会按
     * dstA = srcA² + dstA(1-srcA) 拉低画布自身的 alpha。窗口里这层会再和黑底
     * 合成，看不出问题；但导出的 PNG 带着半透明像素，看图器会把它们和**白色**
     * 背景合成 —— 整张 4K 图会变成一层灰雾，颜色全部失真（这是 12 版遗留 bug）。 */
    if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        unsigned char *px = (unsigned char *)image.data;
        int n = image.width * image.height;
        for (int i = 0; i < n; ++i) px[i * 4 + 3] = 255;
    }

    ExportImage(image, path);
    UnloadImage(image);
}

