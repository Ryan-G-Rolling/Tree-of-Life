/*
 * main.c  ——  窗口、帧循环、固定步长（Member C）
 *
 * 数据流（01 文档 §7）：
 *   C 读输入 → 生成 0/1 个 GameAction → A 校验并提交 → A 按固定步推进 → C 只读绘制
 *
 * 这里也是唯一允许“碰 GameState 变量本身”的地方（初始化/读档/传给 A），
 * 但没有任何一处直接改 run.xxx 字段——全部经由 game_apply_action。
 *
 * 逻辑画布 1280×720（16:9），窗口默认 1600×900，SS 自动 = 2；
 * --shot 默认 SS=3，导出 3840×2160 真 4K UHD。
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MAKE_DIR(p) mkdir(p, 0777)
#endif

#include "raylib.h"
#include "ui.h"

#ifdef UI_MOCK
#include "mock_game.h"
int mock_selftest(void);
#else
#include "game.h"
#include "save.h"
#include "evolution.h"   /* 截图演示：生成突变候选 */
#endif

#include "ui_internal.h"
#include "codex.h"      /* 截图演示：按中文名定位图鉴条目 */

#define SAVE_PATH "saves/treeoflife-v8.sav"
#define FIXED_STEP 0.1
#define AUTOSAVE_SECONDS 15.0

/* 窗口默认 1600×900：逻辑画布 1280×720 的 1.25 倍，自动超采样档位 SS=2
 * （画布 2560×1440），非整数倍窗口也能拿到 SSAA 的下采样收益。 */
#define DEFAULT_WIN_W 1600
#define DEFAULT_WIN_H 900

static void capture_screen(const GameState *game, const char *file);
static void toggle_fullscreen(void);

/* 15 秒自动存档 + 退出存档都写 saves/。目录不存在时 save_write 一定失败，
 * 而失败是静默的（只返回 SAVE_FILE_ERROR），玩家会以为进度丢了。
 * 这里在建窗后、第一次读档前把目录补上。 */
static void ensure_save_dir(void)
{
    if (DirectoryExists("saves")) return;
    int r = MAKE_DIR("saves");
    if (r == 0 || DirectoryExists("saves")) {
        TraceLog(LOG_INFO, "saves/ created");
    } else {
        TraceLog(LOG_WARNING, "saves/ cannot be created (errno=%d); progress will not persist", r);
    }
}

int main(int argc, char **argv)
{
    ChangeDirectory(GetApplicationDirectory());
    bool shot_mode = false;
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
            printf("自测请运行: mingw32-make test\n"
                   "  build/test_b.exe            规则层断言\n"
                   "  build/test_integration.exe  A/B 接线与存档往返\n");
            return 0;
#endif
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
    /* 截图验收：默认 SS=3 → PNG 就是 3840×2160 真 4K，不依赖真实窗口大小 */
    if (shot_mode && forced_ss == 0) forced_ss = 3;

    /* 用离屏超采样替代高 DPI：开 HIGHDPI 会让窗口坐标与帧缓冲比例漂移，
     * 和 SS 推导打架，所以坚决不加 FLAG_WINDOW_HIGHDPI。 */
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(win_w, win_h, "Tree of Life  ·  Evolution & Legacy");
    if (!IsWindowReady()) {
        fprintf(stderr, "InitWindow failed\n");
        return 1;
    }

    /* 最小窗口 = 逻辑画布 1:1（1280×720）。1366×768 这类小屏放不下 720 高的窗口
     * 会被任务栏切掉底栏，所以按显示器可用区域回退，宁可降档不许裁切。 */
    {
        int min_w = UI_W;
        int min_h = UI_H;
        int mon_w = GetMonitorWidth(GetCurrentMonitor());
        int mon_h = GetMonitorHeight(GetCurrentMonitor());
        if (mon_w > 0 && min_w > mon_w - 80) min_w = mon_w - 80;
        if (mon_h > 0 && min_h > mon_h - 120) min_h = mon_h - 120;
        if (min_w < 320) min_w = 320;
        if (min_h < 240) min_h = 240;
        SetWindowMinSize(min_w, min_h);
        if (min_w != UI_W || min_h != UI_H) {
            TraceLog(LOG_INFO, "Window min size relaxed to %dx%d (monitor %dx%d too small for %dx%d)",
                     min_w, min_h, mon_w, mon_h, UI_W, UI_H);
        }
    }
    SetTargetFPS(60);
    InitAudioDevice();
    ensure_save_dir();

    GameState game;
    memset(&game, 0, sizeof(game));

#ifdef UI_MOCK
    if (shot_mode) {
        mock_fill_demo_state(&game);
    } else {
        if (save_read(SAVE_PATH, &game) != SAVE_OK) game_init(&game);
    }
#else
    if (save_read(SAVE_PATH, &game) != SAVE_OK) game_init(&game);
#endif

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
        /* F11 全屏切换。窗口尺寸变了，ui_render 会按新尺寸重算 SS 并重建画布。 */
        if (IsKeyPressed(KEY_F11)) toggle_fullscreen();

        GameAction action;
        if (ui_update(&game, &action)) {
            ActionResult result = game_apply_action(&game, action);
            last_result = result;
            has_result = true;
            ui_notify_result(&game, action, result);
            if (result.code == ACTION_OK) {
                save_write(SAVE_PATH, &game);
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
            save_write(SAVE_PATH, &game);
            autosave_timer = 0.0;
        }

        ui_render(&game, has_result ? &last_result : NULL);
    }

    save_write(SAVE_PATH, &game);
    ui_shutdown();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}

/* F11：切全屏 / 回窗口化。
 * 用 borderless 全屏（铺满当前显示器 + 原生分辨率）而不是改显示模式的独占全屏：
 * 4K 显示器上全屏即 3840×2160 → SS=3，不闪屏、不改用户桌面分辨率。 */
static void toggle_fullscreen(void)
{
    ToggleBorderlessWindowed();
}

/* ================= 截图演示状态 =================
 * 只在 --shot 下使用。正常流程仍由 game_init / save_read 产生状态。
 * 这里直接填字段，是因为截图要展示“已经玩了一会儿”的界面：冷启动的截图里
 * 商店全灰、图鉴全空、进化树只有原点。 */

static void set_mutation(SpeciesState *sp, int id) { sp->mutations[id] = true; }

static void fill_discoveries(GameState *g, int up_to)
{
    for (int id = 0; id <= up_to && id < SPECIES_COUNT; ++id) {
        g->run.discovered_species[id] = true;
        g->legacy.codex_species[id] = true;
    }
}

/* 早期：单槽 + 杆状菌。用来验证"生物图像不出画框"——
 * 杆菌的胶囊体＋鞭毛是最容易顶出培养皿/展台的一种形态。 */
static void fill_demo_early(GameState *g)
{
    memset(g, 0, sizeof(*g));
    g->phase = PHASE_RUNNING;
    g->run.era = 1;
    g->run.atp = 1860.0;
    g->run.biomass = 940.0;
    g->run.dna = 320.0;
    g->run.elapsed_time = 420.0;
    g->run.max_biomass = 1400.0;
    g->run.capacity_levels = 2;
    g->run.run_upgrade_levels[UPGRADE_METABOLISM] = 2;
    g->run.run_upgrade_levels[UPGRADE_REPRODUCTION] = 1;

    g->run.current_environment_id = ENV_NORMAL_OCEAN;
    g->run.next_environment_id = ENV_LOW_LIGHT_PERIOD;
    g->run.event_interval_left = 30.0;

    g->run.active_species_count = 1;
    SpeciesState *main = &g->run.active_species[0];
    main->species_id = SPECIES_CYANOBACTERIUM;
    main->is_main_lineage = true;
    main->population = 180.0;
    set_mutation(main, MUTATION_CELL_MEMBRANE);
    set_mutation(main, MUTATION_PHOTOSYNTHESIS);

    fill_discoveries(g, 4);
}

/* 后期：技术圈 / 文明阶段的繁荣态（主界面、图鉴、进化树、结算、Legacy 用） */
static void fill_demo_late(GameState *g)
{
    memset(g, 0, sizeof(*g));
    g->phase = PHASE_RUNNING;
    g->run.era = 4;
    g->run.atp = 84200.0;
    g->run.biomass = 361000000.0;
    g->run.dna = 12800.0;
    /* 理念与洞见等级给足，截图才能同时展示“可购买”和“已有倍率”两种状态 */
    g->run.knowledge = 26400.0;
    g->run.insight_levels = 3;
    g->run.elapsed_time = 3725.0;
    g->run.max_biomass = 420000000.0;
    g->run.capacity_levels = 5;
    g->run.reserve_levels = 3;
    g->run.run_upgrade_levels[UPGRADE_METABOLISM] = 7;
    g->run.run_upgrade_levels[UPGRADE_REPRODUCTION] = 5;
    g->run.run_upgrade_levels[UPGRADE_GENOME] = 4;

    g->run.current_environment_id = ENV_NORMAL_OCEAN;
    g->run.next_environment_id = ENV_GLOBAL_COOLING;
    g->run.environment_time_left = 0.0;
    g->run.event_interval_left = 74.0;

    g->run.active_species_count = 3;

    SpeciesState *main = &g->run.active_species[0];
    main->species_id = SPECIES_CIVILIZATION;
    main->is_main_lineage = true;
    main->population = 5240.0;
    set_mutation(main, MUTATION_CELL_MEMBRANE);
    set_mutation(main, MUTATION_PHOTOSYNTHESIS);
    set_mutation(main, MUTATION_NUCLEUS);
    set_mutation(main, MUTATION_MITOCHONDRIA);
    set_mutation(main, MUTATION_MULTICELLULARITY);
    set_mutation(main, MUTATION_CELL_DIFFERENTIATION);
    set_mutation(main, MUTATION_NEURAL_SIGNALING);
    set_mutation(main, MUTATION_QUORUM_SENSING);
    set_mutation(main, MUTATION_ABSTRACT_THOUGHT);
    set_mutation(main, MUTATION_PLANETARY_INDUSTRY);
    set_mutation(main, MUTATION_LANGUAGE);
    set_mutation(main, MUTATION_URBANIZATION);

    SpeciesState *second = &g->run.active_species[1];
    second->species_id = SPECIES_REEF_METROPOLIS;
    second->population = 1720.0;
    set_mutation(second, MUTATION_PHOTOSYNTHESIS);
    set_mutation(second, MUTATION_NUCLEUS);
    set_mutation(second, MUTATION_PHOTOSYMBIOSIS);
    set_mutation(second, MUTATION_REEF_ARCHITECTURE);

    SpeciesState *third = &g->run.active_species[2];
    third->species_id = SPECIES_MYCORRHIZAL_WEB;
    third->population = 860.0;
    set_mutation(third, MUTATION_DECOMPOSER_ENZYMES);
    set_mutation(third, MUTATION_MYCORRHIZAL_SYMBIOSIS);
    set_mutation(third, MUTATION_SYMBIOTIC_NITROGEN_FIXATION);

    fill_discoveries(g, 16);

    static const int muts[] = {
        MUTATION_CELL_MEMBRANE, MUTATION_PHOTOSYNTHESIS, MUTATION_CHEMOSYNTHESIS,
        MUTATION_CELL_WALL, MUTATION_HEAT_RESISTANCE, MUTATION_COLD_RESISTANCE,
        MUTATION_NUCLEUS, MUTATION_MITOCHONDRIA, MUTATION_MULTICELLULARITY,
        MUTATION_TOXIN_RESISTANCE, MUTATION_EFFICIENT_RESPIRATION,
        MUTATION_CELL_DIFFERENTIATION, MUTATION_NEURAL_SIGNALING,
        MUTATION_SYMBIOTIC_NITROGEN_FIXATION, MUTATION_QUORUM_SENSING,
        MUTATION_DECOMPOSER_ENZYMES, MUTATION_PHOTOSYMBIOSIS,
        MUTATION_MYCORRHIZAL_SYMBIOSIS, MUTATION_REEF_ARCHITECTURE,
        MUTATION_ABSTRACT_THOUGHT, MUTATION_PLANETARY_INDUSTRY, MUTATION_LANGUAGE,
        MUTATION_URBANIZATION
    };
    for (unsigned i = 0; i < sizeof muts / sizeof muts[0]; ++i) {
        int id = muts[i];
        if (id < 0 || id >= MUTATION_COUNT) continue;
        g->run.discovered_mutations[id] = true;
        g->legacy.codex_mutations[id] = true;
    }

    g->run.survived_crises[ENV_GLOBAL_WARMING] = true;
    g->run.survived_crises[ENV_TOXIC_BLOOM] = true;
    g->run.survived_crises[ENV_INDUSTRIAL_SMOG] = true;

    g->legacy.evolution_points = 168;
    g->legacy.levels[LEGACY_METABOLISM] = 4;
    g->legacy.levels[LEGACY_MUTATION] = 3;
    g->legacy.levels[LEGACY_ADAPTATION] = 2;
    g->legacy.levels[LEGACY_DIVERSIFICATION] = 3;
    g->legacy.best_era = 5;
    g->legacy.best_evolution_points = 214;
    g->legacy.best_biomass = 980000000.0;
}

/* 中期：多细胞群落刚打开 5 条分化路线（商店分化页与分化弹层用） */
static void fill_demo_mid(GameState *g)
{
    memset(g, 0, sizeof(*g));
    g->phase = PHASE_RUNNING;
    g->run.era = 2;
    g->run.atp = 4860.0;
    g->run.biomass = 26400.0;
    g->run.dna = 1840.0;
    g->run.knowledge = 0.0;
    g->run.elapsed_time = 960.0;
    g->run.max_biomass = 31200.0;
    g->run.capacity_levels = 3;
    g->run.reserve_levels = 1;
    g->run.run_upgrade_levels[UPGRADE_METABOLISM] = 4;
    g->run.run_upgrade_levels[UPGRADE_REPRODUCTION] = 3;
    g->run.run_upgrade_levels[UPGRADE_GENOME] = 2;

    g->run.current_environment_id = ENV_NORMAL_OCEAN;
    g->run.next_environment_id = ENV_GLOBAL_WARMING;
    g->run.environment_time_left = 0.0;
    g->run.event_interval_left = 42.0;

    g->run.active_species_count = 2;

    SpeciesState *main = &g->run.active_species[0];
    main->species_id = SPECIES_MULTICELLULAR_COLONY;
    main->is_main_lineage = true;
    main->population = 640.0;
    set_mutation(main, MUTATION_CELL_MEMBRANE);
    set_mutation(main, MUTATION_PHOTOSYNTHESIS);
    set_mutation(main, MUTATION_NUCLEUS);
    set_mutation(main, MUTATION_MITOCHONDRIA);
    set_mutation(main, MUTATION_MULTICELLULARITY);
    set_mutation(main, MUTATION_CELL_DIFFERENTIATION);
    set_mutation(main, MUTATION_SYMBIOTIC_NITROGEN_FIXATION);
    set_mutation(main, MUTATION_DECOMPOSER_ENZYMES);
    set_mutation(main, MUTATION_PHOTOSYMBIOSIS);
    set_mutation(main, MUTATION_HEAT_RESISTANCE);
    set_mutation(main, MUTATION_TOXIN_RESISTANCE);

    SpeciesState *second = &g->run.active_species[1];
    second->species_id = SPECIES_ALGAL_EUKARYOTE;
    second->population = 220.0;
    set_mutation(second, MUTATION_PHOTOSYNTHESIS);
    set_mutation(second, MUTATION_NUCLEUS);

    fill_discoveries(g, 6);

    static const int muts[] = {
        MUTATION_CELL_MEMBRANE, MUTATION_PHOTOSYNTHESIS, MUTATION_CHEMOSYNTHESIS,
        MUTATION_CELL_WALL, MUTATION_HEAT_RESISTANCE, MUTATION_COLD_RESISTANCE,
        MUTATION_NUCLEUS, MUTATION_MITOCHONDRIA, MUTATION_MULTICELLULARITY,
        MUTATION_TOXIN_RESISTANCE, MUTATION_CELL_DIFFERENTIATION,
        MUTATION_SYMBIOTIC_NITROGEN_FIXATION, MUTATION_DECOMPOSER_ENZYMES,
        MUTATION_PHOTOSYMBIOSIS
    };
    for (unsigned i = 0; i < sizeof muts / sizeof muts[0]; ++i) {
        int id = muts[i];
        if (id < 0 || id >= MUTATION_COUNT) continue;
        g->run.discovered_mutations[id] = true;
        g->legacy.codex_mutations[id] = true;
    }

    g->run.survived_crises[ENV_GLOBAL_WARMING] = true;

    g->legacy.evolution_points = 24;
    g->legacy.levels[LEGACY_METABOLISM] = 2;
    g->legacy.levels[LEGACY_MUTATION] = 1;
    g->legacy.levels[LEGACY_ADAPTATION] = 1;
    g->legacy.levels[LEGACY_DIVERSIFICATION] = 5;
    g->legacy.best_era = 3;
    g->legacy.best_evolution_points = 31;
    g->legacy.best_biomass = 18600.0;
}

/* 终局：奇点（黑洞 + 吸积盘）作为主视觉 */
static void fill_demo_singularity(GameState *g)
{
    memset(g, 0, sizeof(*g));
    g->phase = PHASE_RUNNING;
    g->run.era = 6;
    g->run.atp = 9.62e9;
    g->run.biomass = 4.18e12;
    g->run.dna = 3.44e8;
    g->run.knowledge = 2.6e6;
    g->run.elapsed_time = 12640.0;
    g->run.max_biomass = 4.9e12;
    g->run.run_upgrade_levels[UPGRADE_METABOLISM] = 16;
    g->run.run_upgrade_levels[UPGRADE_REPRODUCTION] = 13;
    g->run.run_upgrade_levels[UPGRADE_GENOME] = 11;

    g->run.current_environment_id = ENV_NORMAL_OCEAN;
    g->run.next_environment_id = ENV_SOLAR_FLARE;
    g->run.event_interval_left = 58.0;

    g->run.active_species_count = 2;
    SpeciesState *main = &g->run.active_species[0];
    main->species_id = SPECIES_SINGULARITY;
    main->is_main_lineage = true;
    main->population = 42100.0;
    for (int m = 0; m < MUTATION_COUNT; ++m) main->mutations[m] = true;

    SpeciesState *second = &g->run.active_species[1];
    second->species_id = SPECIES_SPACE_AGE;
    second->population = 12600.0;
    for (int m = 0; m < MUTATION_COUNT; ++m) second->mutations[m] = true;

    fill_discoveries(g, SPECIES_COUNT - 1);
    for (int m = 0; m < MUTATION_COUNT; ++m) {
        g->run.discovered_mutations[m] = true;
        g->legacy.codex_mutations[m] = true;
    }
    for (int e = 0; e < ENV_COUNT; ++e) g->run.survived_crises[e] = true;

    g->legacy.evolution_points = 940;
    g->legacy.levels[LEGACY_METABOLISM] = 7;
    g->legacy.levels[LEGACY_MUTATION] = 6;
    g->legacy.levels[LEGACY_ADAPTATION] = 6;
    g->legacy.levels[LEGACY_DIVERSIFICATION] = 6;
    g->legacy.best_era = 7;
    g->legacy.best_evolution_points = 1204;
    g->legacy.best_biomass = 6.1e12;
}

/* 截图验证：把三种页面与四种弹层各导出一张，方便离线检查排版 */
static void capture_screen(const GameState *base, const char *first_file)
{
    GameState game = *base;
    fill_demo_late(&game);

    /* 首帧那条引导提示（"点击培养皿开始积累 ATP"）不该跟着归档截图走；
     * 需要提示条的图由 ui_debug_poke_toast() 单独挂。 */
    g_ui.toast_t = 0.0f;

    ui_debug_force(UI_SCREEN_MAIN, UI_OVL_NONE);
    ui_debug_set_keep_parent(1);
    ui_debug_set_store_tab(0);
    ui_debug_set_info_tab(0);
    ui_debug_capture(&game, first_file);

    /* 随机增益因子：稀有强增益 + 光链图标，一次看清“类型色 / 图标 / 提示行”三件事 */
    game.run.buff_visible = true;
    game.run.buff_visible_type = BUFF_PRODUCTION_STRONG;
    game.run.buff_visible_icon = BUFF_ICON_DNA;
    ui_debug_capture(&game, "shot_01g_buff.png");

    /* 增益生效：皿壁进度弧 + ATP 旁的倍率胶囊 */
    game.run.buff_visible = false;
    game.run.buff_active_type = BUFF_PRODUCTION;
    game.run.buff_active_time_left = 19.0;
    ui_debug_capture(&game, "shot_01h_buff_active.png");
    game.run.buff_active_time_left = 0.0;

    /* 情报面板的另外两页：产出曲线 / 纪录 */
    ui_debug_set_info_tab(1);
    ui_debug_capture(&game, "shot_01e_info_output.png");
    ui_debug_set_info_tab(2);
    ui_debug_capture(&game, "shot_01f_info_record.png");
    ui_debug_set_info_tab(0);

    /* 采集反馈：皿上浮字 + 星火粒子 */
    GameAction click = { ACTION_CLICK_ATP, 0, 0, 0 };
    ActionResult click_result = game_apply_action(&game, click);
    ui_notify_result(&game, click, click_result);
    ui_debug_capture(&game, "shot_01c_click.png");
    ui_debug_poke_click(-1.0);   /* 复位，别跟进下一张 */

    ui_debug_poke_toast("还没有可用的分化方向：先通过突变满足前置条件", true);
    ui_debug_capture(&game, "shot_01d_toast.png");
    g_ui.toast_t = 0.0f;

    /* ---- 中期：商店分化页 + 分化弹层 ---- */
    fill_demo_mid(&game);
    ui_debug_force(UI_SCREEN_MAIN, UI_OVL_NONE);
    ui_debug_set_store_tab(1);
    ui_debug_capture(&game, "shot_01b_store_branch.png");

    ui_debug_force(UI_SCREEN_MAIN, UI_OVL_SPECIATION);
    ui_debug_capture(&game, "shot_03_speciation.png");

    /* 突变候选用 B 的真实抽取函数生成，弹层里就是这一局真的会出现的候选 */
#ifndef UI_MOCK
    game.run.mutation_offer = evolution_generate_mutation_offer(&game, 0);
#endif
    ui_debug_force(UI_SCREEN_MAIN, UI_OVL_MUTATION);
    ui_debug_capture(&game, "shot_02_mutation.png");

    /* ---- 后期：图鉴三页 / 进化树 / 结算 / Legacy ---- */
    fill_demo_late(&game);
    ui_debug_force(UI_SCREEN_MAIN, UI_OVL_CODEX);
    ui_debug_set_codex_tab(0);
    ui_debug_capture(&game, "shot_04_codex.png");

    ui_debug_set_codex_tab(1);
    ui_debug_capture(&game, "shot_04b_codex_mut.png");

    ui_debug_set_codex_tab(2);
    ui_debug_capture(&game, "shot_04c_codex_record.png");

    /* 图鉴详情：物种大图里的生境背景（石松林 → 湿地与森林）。
     * 大图与展台共用 habitat_draw，一张图就能核对两处是否一致。 */
    {
        int probe = -1;
        for (int i = 0; i < SPECIES_COUNT; ++i) {
            const CodexEntry *e = codex_entry(CODEX_SPECIES, i);
            if (e && e->name && strcmp(e->name, "石松林") == 0) { probe = i; break; }
        }
        if (probe >= 0) {
            game.run.discovered_species[probe] = true;
            game.legacy.codex_species[probe] = true;
            g_ui.codex_detail_kind = CODEX_SPECIES;
            g_ui.codex_detail_id = probe;
            ui_debug_force(UI_SCREEN_MAIN, UI_OVL_CODEX_DETAIL);
            ui_debug_capture(&game, "shot_04d_codex_habitat.png");
            ui_debug_force(UI_SCREEN_MAIN, UI_OVL_CODEX);
        }
    }

    ui_debug_force(UI_SCREEN_MAIN, UI_OVL_TREE);
    ui_debug_capture(&game, "shot_05_tree.png");

    /* 星图缩放：滚轮放大约 1.9 倍并向右下平移，验证节点/连线/标签跟着视图走 */
    g_ui.tree_zoom = 1.9f;
    g_ui.tree_pan_x = -110.0f;
    g_ui.tree_pan_y = -30.0f;
    ui_debug_capture(&game, "shot_05c_tree_zoom.png");
    g_ui.tree_zoom = 1.0f;
    g_ui.tree_pan_x = 0.0f;
    g_ui.tree_pan_y = 0.0f;

    /* 突变图谱：本轮已获得的全部突变 + 共振 */
    ui_debug_force(UI_SCREEN_MAIN, UI_OVL_MUT_LIST);
    ui_debug_capture(&game, "shot_05b_mutations.png");

    game.phase = PHASE_END_ERA_PREVIEW;
    ui_debug_force(UI_SCREEN_END_ERA, UI_OVL_NONE);
    ui_debug_capture(&game, "shot_06_end_era.png");

    game.phase = PHASE_LEGACY;
    ui_debug_force(UI_SCREEN_LEGACY, UI_OVL_NONE);
    ui_debug_capture(&game, "shot_07_legacy.png");

    /* 危机横幅：常态 / 事件两种计时各出一张，回归时能直接对比环的语义 */
    fill_demo_late(&game);
    game.phase = PHASE_RUNNING;
    game.run.current_environment_id = ENV_GLOBAL_COOLING;
    game.run.environment_time_left = 18.0;
    game.run.event_interval_left = 0.0;
    ui_debug_force(UI_SCREEN_MAIN, UI_OVL_NONE);
    ui_debug_capture(&game, "shot_08_crisis.png");

    /* 陨石撞击：进入瞬间的爆发（火痕 + 冲击波 + 抖屏 + 标题条） */
    fill_demo_late(&game);
    game.phase = PHASE_RUNNING;
    game.run.current_environment_id = ENV_METEOR_IMPACT;
    game.run.environment_time_left = 22.0;
    game.run.event_interval_left = 0.0;
    ui_debug_force(UI_SCREEN_MAIN, UI_OVL_NONE);
    ui_debug_poke_crisis(ENV_METEOR_IMPACT, 2.0f);
    ui_debug_capture(&game, "shot_08b_meteor.png");
    ui_debug_poke_crisis(-1, 0.0f);
    g_ui.crisis_flash = 0.0f;

    /* 终局形态：奇点（黑洞 + 吸积盘 + 光子环），展示形态引擎的第 10 类 */
    fill_demo_singularity(&game);
    ui_debug_force(UI_SCREEN_MAIN, UI_OVL_NONE);
    ui_debug_capture(&game, "shot_09_singularity.png");

    /* 突变弹层的文案上限：后期突变的加成项最多，确认中文摘要不会顶到"点击选择" */
#ifndef UI_MOCK
    fill_demo_late(&game);
    game.run.mutation_offer = evolution_generate_mutation_offer(&game, 0);
    ui_debug_force(UI_SCREEN_MAIN, UI_OVL_MUTATION);
    ui_debug_capture(&game, "shot_02b_mutation_late.png");
    ui_debug_force(UI_SCREEN_MAIN, UI_OVL_NONE);
#endif

    /* 早期杆状菌：培养皿 + 展台里的生物都必须落在自己的画框内 */
    fill_demo_early(&game);
    ui_debug_force(UI_SCREEN_MAIN, UI_OVL_NONE);
    ui_debug_capture(&game, "shot_10_early_rod.png");

    /* 微光期：危机横幅 + 压暗的展台，同一只杆状菌不该压到横幅上 */
    game.run.current_environment_id = ENV_LOW_LIGHT_PERIOD;
    game.run.environment_time_left = 24.0;
    game.run.event_interval_left = 0.0;
    ui_debug_capture(&game, "shot_10b_lowlight.png");

    /* 分化弹层：前置条件用中文突变名（"需要：脂质修复"） */
    game.run.biomass = 4200.0;
    fill_discoveries(&game, SPECIES_COUNT - 1);
    ui_debug_force(UI_SCREEN_MAIN, UI_OVL_SPECIATION);
    ui_debug_capture(&game, "shot_10c_speciation_zh.png");
    ui_debug_force(UI_SCREEN_MAIN, UI_OVL_NONE);

    /* 生境样张：8 种大环境并排，核对"每个阶段站在什么地上" */
    ui_debug_capture_habitats("shot_11_habitats.png");

    TraceLog(LOG_INFO, "shot capture done: 27 frames");
}
