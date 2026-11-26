/*
 * main.c  ——  窗口、帧循环、固定步长（Member C）
 *
 * 数据流（01 文档 §7）：
 *   C 读输入 → 生成 0/1 个 GameAction → A 校验并提交 → A 按固定步推进 → C 只读绘制
 *
 * 这里也是唯一允许“碰 GameState 变量本身”的地方（初始化/读档/传给 A），
 * 但没有任何一处直接改 run.xxx 字段——全部经由 game_apply_action。
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "ui.h"

#ifdef UI_MOCK
#include "mock_game.h"
int mock_selftest(void);
#else
#include "game.h"
#include "save.h"
#endif

#include "ui_internal.h"

#define SAVE_PATH "saves/treeoflife-v7.sav"
#define FIXED_STEP 0.1
#define AUTOSAVE_SECONDS 15.0

/* 窗口默认开在 1600×1000：逻辑画布 1280×800 的 1.25 倍，此时自动超采样档位为
 * SS=2（画布 2560×1600），非整数倍窗口也能拿到 SSAA 的下采样收益。 */
#define DEFAULT_WIN_W 1600
#define DEFAULT_WIN_H 1000

static void capture_screen(const GameState *game, const char *file);
static void toggle_fullscreen(void);

int main(int argc, char **argv)
{
    /* Launching from Explorer or a launcher must find assets and saves beside the exe. */
    ChangeDirectory(GetApplicationDirectory());
    bool shot_mode = false;
    const char *load_path=SAVE_PATH;
    const char *write_path=SAVE_PATH;
    int win_w = DEFAULT_WIN_W;
    int win_h = DEFAULT_WIN_H;
    int forced_ss = 0; /* 0 = 按窗口自动推导，1..3 = 强制 */

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--shot") == 0) {
            shot_mode = true;
        } else if (strcmp(argv[i], "--selftest") == 0) {
#ifdef UI_MOCK
            return mock_selftest();
#else
            printf("--selftest 只在 MOCK=1 构建下可用\n");
            return 1;
#endif
        } else if(strcmp(argv[i], "--save")==0 && i+1<argc) {
            write_path=argv[++i];
        } else if(strcmp(argv[i], "--load")==0 && i+1<argc) {
            load_path=argv[++i];
        } else if (strcmp(argv[i], "--w") == 0 && i + 1 < argc) {
            win_w = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--h") == 0 && i + 1 < argc) {
            win_h = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--ss") == 0 && i + 1 < argc) {
            forced_ss = atoi(argv[++i]);
        }
    }

    if (win_w < 320) win_w = DEFAULT_WIN_W;
    if (win_h < 240) win_h = DEFAULT_WIN_H;
    if (forced_ss < 0) forced_ss = 0;
    if (forced_ss > UI_SS_MAX) forced_ss = UI_SS_MAX;
    /* 截图验收：默认 SS=3 → PNG 就是 3840×2400 真 4K，不依赖真实窗口大小 */
    if (shot_mode && forced_ss == 0) forced_ss = 3;

    /* 本项目用离屏超采样替代高 DPI：开 HIGHDPI 会让窗口坐标与帧缓冲比例漂移，
     * 和 SS 推导打架，所以坚决不加 FLAG_WINDOW_HIGHDPI。 */
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(win_w, win_h, "Tree of Life  ·  Evolution & Legacy");
    if (!IsWindowReady()) {
        fprintf(stderr, "InitWindow failed\n");
        return 1;
    }
    /* 最小窗口 = 逻辑画布 1:1（1280×800，docs/art/01-visual-identity.md D15）：
     * 低于它 SS 恒为 1，12px 小字只能拿到图集 1/3 的采样密度。
     * 但 1366×768 / 1280×800 这类小屏笔记本（教室投影机常见）放不下 800px 高的
     * 窗口，会被任务栏切掉底栏 —— 所以按当前显示器可用区域回退，宁可降档不许裁切。 */
    {
        int min_w = 1280;
        int min_h = 800;
        int mon_w = GetMonitorWidth(GetCurrentMonitor());
        int mon_h = GetMonitorHeight(GetCurrentMonitor());
        if (mon_w > 0 && min_w > mon_w - 80) min_w = mon_w - 80;   /* 留窗口边框 */
        if (mon_h > 0 && min_h > mon_h - 120) min_h = mon_h - 120; /* 留标题栏+任务栏 */
        if (min_w < 320) min_w = 320;
        if (min_h < 240) min_h = 240;
        SetWindowMinSize(min_w, min_h);
        if (min_w != 1280 || min_h != 800) {
            TraceLog(LOG_INFO, "Window min size relaxed to %dx%d (monitor %dx%d too small for 1280x800)",
                     min_w, min_h, mon_w, mon_h);
        }
    }
    SetTargetFPS(60);
    InitAudioDevice();

    GameState game;
    memset(&game, 0, sizeof(game));

#ifdef UI_MOCK
    if (shot_mode) {
        mock_fill_demo_state(&game);
    } else {
        if (save_read(load_path, &game) != SAVE_OK) game_init(&game);
    }
#else
    if (save_read(load_path, &game) != SAVE_OK) game_init(&game);
#endif

    if (!DirectoryExists("saves")) MakeDirectory("saves");
    ui_set_forced_ss(forced_ss);
    ui_init();

    if (shot_mode) {
        capture_screen(&game, "shot_01_main.png");
        ui_shutdown();
        CloseAudioDevice();
        CloseWindow();
        return 0;
    }

    double accumulator = 0.0;
    double autosave_timer = 0.0;
    ActionResult last_result;
    memset(&last_result, 0, sizeof(last_result));
    bool has_result = false;

    while (!ui_should_close()) {
        /* F11 全屏切换。窗口尺寸变了，ui_render 会按新尺寸重算 SS 并重建画布；
           转回窗口化时同理，SS 降档也不会崩（画布每次都整张重建）。 */
        if (IsKeyPressed(KEY_F11)) toggle_fullscreen();

        GameAction action;
        if (ui_update(&game, &action)) {
            ActionResult result = game_apply_action(&game, action);
            last_result = result;
            has_result = true;
            ui_notify_result(&game, action, result);
            if (result.code == ACTION_OK) {
                ui_report_save(save_write(write_path, &game)==SAVE_OK);
                autosave_timer = 0.0;
            }
        }

        /* 真实时间 → 固定步。渲染帧率变化不影响模拟结果。 */
        accumulator += (double)GetFrameTime();
        if (accumulator > 0.5) accumulator = 0.5;
        while (accumulator >= FIXED_STEP) {
            game_update(&game, FIXED_STEP);
            accumulator -= FIXED_STEP;
        }

        autosave_timer += (double)GetFrameTime();
        if (autosave_timer >= AUTOSAVE_SECONDS) {
            ui_report_save(save_write(write_path, &game)==SAVE_OK);
            autosave_timer = 0.0;
        }

        ui_render(&game, has_result ? &last_result : NULL);
    }

    if(save_write(write_path, &game)!=SAVE_OK) TraceLog(LOG_ERROR,"Failed to save game on exit");
    ui_shutdown();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}

/* F11：切全屏 / 回窗口化。
 * 用 borderless 全屏（铺满当前显示器 + 原生分辨率）而不是改显示模式的独占全屏：
 *   raylib 5.5 的 ToggleFullscreen() 会把显示模式设成“最接近当前窗口尺寸”的那一档，
 *   1600×1000 的窗口全屏后可能落到 1920×1080，SS 只升到 2，拿不到 4K 原生清晰；
 *   而 borderless 直接给到显示器原生分辨率（4K → 3840×2160，SS=3），
 *   不闪屏、不改用户桌面分辨率，崩溃也不会把分辨率留在错误档位。 */
static void toggle_fullscreen(void)
{
    ToggleBorderlessWindowed();
}

/* 截图验证：把三种页面与四种弹层各导出一张，方便离线检查排版 */
static void capture_screen(const GameState *base, const char *first_file)
{
    GameState game = *base;

    ui_debug_force(UI_SCREEN_MAIN, UI_OVL_NONE);
    ui_debug_set_keep_parent(1);
    ui_debug_capture(&game, first_file);

    game_apply_action(&game,(GameAction){ACTION_GENERATE_MUTATION_OFFER,0,0,0});
    ui_debug_force(UI_SCREEN_MAIN, UI_OVL_MUTATION);
    ui_debug_capture(&game, "shot_02_mutation.png");

    ui_debug_force(UI_SCREEN_MAIN, UI_OVL_SPECIATION);
    ui_debug_capture(&game, "shot_03_speciation.png");

    ui_debug_force(UI_SCREEN_MAIN, UI_OVL_CODEX);
    ui_debug_set_codex_tab(0);
    ui_debug_capture(&game, "shot_04_codex.png");

    ui_debug_set_codex_tab(1);
    ui_debug_capture(&game, "shot_04b_codex_mut.png");

    ui_debug_set_codex_tab(2);
    ui_debug_capture(&game, "shot_04c_codex_record.png");

    ui_debug_force(UI_SCREEN_MAIN, UI_OVL_TREE);
    ui_debug_capture(&game, "shot_05_tree.png");
    for(int page=1;page<4;page++) {
        char file[64];snprintf(file,sizeof file,"shot_05_tree_page%d.png",page+1);
        screens_debug_tree_page(page);ui_debug_capture(&game,file);
    }
    screens_debug_tree_page(0);

    game.phase = PHASE_END_ERA_PREVIEW;
    ui_debug_force(UI_SCREEN_END_ERA, UI_OVL_NONE);
    ui_debug_capture(&game, "shot_06_end_era.png");

    game.phase = PHASE_LEGACY;
    ui_debug_force(UI_SCREEN_LEGACY, UI_OVL_NONE);
    ui_debug_capture(&game, "shot_07_legacy.png");
}
