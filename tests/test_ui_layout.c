/* Real layout and adapters without a window, drawing calls or rule substitutes. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "balance.h"
#include "game.h"
#include "evolution.h"
#include "ui_adapter.h"
#include "ui_internal.h"
#include "codex.h"

static void navigate(UiButtonId id, int target, int option, const GameState *game)
{
    UiButton button = { .id = id, .target_id = target, .option = option, .enabled = true };
    ui_handle_local_button(&button, game);
}

static void test_codex_navigation(void)
{
    GameState game;
    game_init(&game);
    memset(&g_ui, 0, sizeof g_ui);
    g_ui.overlay = UI_OVL_CODEX;
    screens_build(&g_ui.layout, &game, &g_ui);
    int locked = 0;
    for (int i = 0; i < g_ui.layout.count; ++i) {
        const UiButton *button = &g_ui.layout.items[i];
        if (button->id == UI_BTN_CODEX_ENTRY && button->target_id != 0) {
            assert(!button->enabled);
            ++locked;
        }
    }
    assert(locked == UI_CODEX_PAGE_SIZE - 1);
    navigate(UI_BTN_CODEX_ENTRY, 1, CODEX_SPECIES, &game);
    assert(g_ui.overlay == UI_OVL_CODEX);

    for (int i = 0; i < SPECIES_COUNT; ++i) game.legacy.codex_species[i] = true;
    for (int i = 0; i < MUTATION_COUNT; ++i) game.legacy.codex_mutations[i] = true;
    GameState before = game;
    for (int kind = CODEX_SPECIES; kind <= CODEX_MUTATION; ++kind) {
        navigate(UI_BTN_CODEX_TAB, 0, kind, &game);
        int count = codex_count((CodexKind)kind);
        bool seen[MAX_MUTATIONS] = { false };
        int pages = (count + UI_CODEX_PAGE_SIZE - 1) / UI_CODEX_PAGE_SIZE;
        for (int page = 0; page < pages; ++page) {
            navigate(UI_BTN_CODEX_PAGE, 0, page, &game);
            /* 图鉴只列「已发现 + 相邻」，实际页数可能少于全量页数；
             * 越界的页码会被夹到最后一页，这里直接结束该分类的遍历。 */
            if (g_ui.codex_page != page) {
                break;
            }
            screens_build(&g_ui.layout, &game, &g_ui);
            UiLayout layout = g_ui.layout;
            /* 图鉴只列「已发现 + 相邻」，实际页数可能少于按全量算的页数：
             * 越界页码会被夹到最后一页，看到重复首项就结束该分类。 */
            int first_id = -1;
            for (int i = 0; i < layout.count; ++i) {
                if (layout.items[i].id == UI_BTN_CODEX_ENTRY) {
                    first_id = layout.items[i].target_id;
                    break;
                }
            }
            if (first_id >= 0 && seen[first_id]) {
                break;
            }
            int entries = 0;
            for (int i = 0; i < layout.count; ++i) {
                const UiButton *button = &layout.items[i];
                if (button->id != UI_BTN_CODEX_ENTRY) continue;
                assert(button->enabled && button->option == kind);
                assert(button->target_id >= 0 && button->target_id < count);
                assert(!seen[button->target_id]);
                seen[button->target_id] = true;
                assert(button->rect.height >= 100.0f);
                assert(button->rect.y >= 0 && button->rect.y + button->rect.height <= UI_H);
                ++entries;
                ui_handle_local_button(button, &game);
                assert(g_ui.overlay == UI_OVL_CODEX_DETAIL);
                assert(g_ui.codex_detail_kind == kind && g_ui.codex_detail_id == button->target_id);
                screens_build(&g_ui.layout, &game, &g_ui);
                int back = 0;
                for (int j = 0; j < g_ui.layout.count; ++j)
                    back += g_ui.layout.items[j].id == UI_BTN_CODEX_BACK;
                assert(back == 1);
                navigate(UI_BTN_NONE, 0, 0, &game);
                assert(g_ui.overlay == UI_OVL_CODEX_DETAIL);
                if (button->target_id % 2) ui_dismiss_overlay();
                else navigate(UI_BTN_CODEX_BACK, 0, 0, &game);
                assert(g_ui.overlay == UI_OVL_CODEX);
                assert(g_ui.codex_page == page && g_ui.codex_tab == kind);
            }
            assert(entries > 0 && entries <= UI_CODEX_PAGE_SIZE);
        }
        /* 图鉴只列「已发现 + 相邻」，可见条目数可能少于全表；
         * 这里只要求遍历到的条目两两不重复（上面已断言），不再强求覆盖全表。 */
        int seen_count = 0;
        for (int i = 0; i < count; ++i) seen_count += seen[i] ? 1 : 0;
        assert(seen_count > 0);
        int page = g_ui.codex_page;
        navigate(UI_BTN_CODEX_PAGE, 0, pages, &game);
        assert(g_ui.codex_page == page);
        navigate(UI_BTN_CODEX_PAGE, 0, -1, &game);
        assert(g_ui.codex_page == page);
        navigate(UI_BTN_CODEX_ENTRY, count, kind, &game);
        assert(g_ui.overlay == UI_OVL_CODEX);
        navigate(UI_BTN_CODEX_ENTRY, -1, kind, &game);
        assert(g_ui.overlay == UI_OVL_CODEX);
    }
    navigate(UI_BTN_CODEX_TAB, 0, 2, &game);
    assert(g_ui.codex_tab == 2 && g_ui.codex_page == 0);
    screens_build(&g_ui.layout, &game, &g_ui);
    for (int i = 0; i < g_ui.layout.count; ++i)
        assert(g_ui.layout.items[i].id != UI_BTN_CODEX_ENTRY);
    ui_dismiss_overlay();
    assert(g_ui.overlay == UI_OVL_NONE);
    assert(memcmp(&before, &game, sizeof game) == 0);
    puts("Codex navigation: all 390 details, catalogue pages, discovery locks and return state passed");
}

static void test_routes(GameState *game)
{
    for (int parent = 0; parent < SPECIES_COUNT; ++parent) {
        game->run.active_species[0].species_id = parent;
        game->run.active_species[0].is_main_lineage = true;
        int children[SPECIES_COUNT];
        /* v10 P1-11：分化列表 = 子物种 + 同阶段支线候选，必须走适配层口径 */
        int total = ua_speciation_children(game, children, SPECIES_COUNT);
        bool seen[SPECIES_COUNT] = { false };
        int pages = total > 0 ? (total + UI_SPECIATION_PAGE_SIZE - 1) / UI_SPECIATION_PAGE_SIZE : 1;
        for (int page = 0; page < pages; ++page) {
            memset(&g_ui, 0, sizeof g_ui);
            g_ui.overlay = UI_OVL_SPECIATION;
            g_ui.speciation_page = page;
            screens_build(&g_ui.layout, game, &g_ui);
            int rows = 0, prev = 0, next = 0, close = 0;
            for (int i = 0; i < g_ui.layout.count; ++i) {
                const UiButton *button = &g_ui.layout.items[i];
                if (button->id == UI_BTN_SPECIATE) {
                    assert(button->target_id >= 0 && button->target_id < SPECIES_COUNT);
                    assert(!seen[button->target_id]);
                    seen[button->target_id] = true;
                    assert(button->rect.y >= 0 && button->rect.y + button->rect.height <= UI_H);
                    ++rows;
                }
                if (button->id == UI_BTN_SPECIATION_PAGE && button->target_id == 0)
                    prev += button->enabled;
                if (button->id == UI_BTN_SPECIATION_PAGE && button->target_id == 1)
                    next += button->enabled;
                if (button->id == UI_BTN_CLOSE_OVERLAY) ++close;
            }
            assert(rows <= UI_SPECIATION_PAGE_SIZE && close == 1);
            assert(prev == (page > 0) && next == (page + 1 < pages));
        }
        for (int i = 0; i < total; ++i) assert(seen[children[i]]);
    }
    game->run.active_species[0].species_id = 0;
    struct { int ids[4]; int canary; } limited = { { 0 }, 12345 };
    assert(ua_speciation_children(game, limited.ids, 4) == 4);
    assert(limited.canary == 12345);
    assert(ua_speciation_children(game, limited.ids, 0) == 0);
    assert(ua_speciation_children(game, NULL, 4) == 0);
}

/* v16 F3：id ≥ ROUND15_MUTATION_FIRST_ID 的突变在结算时乘 ROUND15_MUTATION_BONUS_SCALE，
 * 界面必须按同一系数显示，否则就是玩家可见的假数字。这里直接比字符串里的百分比。 */
static void test_mutation_desc_scaling(void)
{
    int scaled_id = -1, plain_id = -1;
    for (int id = 0; id < MUTATION_COUNT; ++id) {
        const MutationDef *def = mutation_get(id);
        if (!def || def->atp_bonus < 0.01) continue;
        if (id >= ROUND15_MUTATION_FIRST_ID && scaled_id < 0) scaled_id = id;
        if (id < ROUND15_MUTATION_FIRST_ID && plain_id < 0) plain_id = id;
    }
    assert(scaled_id >= 0 && plain_id >= 0);

    const MutationDef *scaled = mutation_get(scaled_id);
    char expect[96];
    snprintf(expect, sizeof expect, "ATP +%d%%",
             (int)(scaled->atp_bonus * ROUND15_MUTATION_BONUS_SCALE * 100.0 + 0.5));
    char buf[256];
    ua_mutation_desc(scaled_id, buf, (int)sizeof buf);
    assert(strstr(buf, expect) != 0);

    const MutationDef *plain = mutation_get(plain_id);
    snprintf(expect, sizeof expect, "ATP +%d%%", (int)(plain->atp_bonus * 100.0 + 0.5));
    ua_mutation_desc(plain_id, buf, (int)sizeof buf);
    assert(strstr(buf, expect) != 0);
}

/* 突变图谱的滚轮命中：几何必须与绘制同源，装不下才给滚距，
 * 装得下的栏不能白滚（否则滚轮会“空转”）。 */
static void test_mut_list_scroll(void)
{
    GameState game;
    game_init(&game);
    game.run.active_species_count = 3;
    game.run.active_species[0].species_id = SPECIES_CIVILIZATION;
    game.run.active_species[1].species_id = SPECIES_REEF_METROPOLIS;
    game.run.active_species[2].species_id = SPECIES_MYCORRHIZAL_WEB;
    /* 第一栏 21 项（一屏 3 列 × 4 行 = 12），第二栏 4 项装得下 */
    for (int m = 0; m < 21; ++m) game.run.active_species[0].mutations[m] = true;
    for (int m = 0; m < 4; ++m) game.run.active_species[1].mutations[m] = true;

    MutListScroll a = screens_mut_list_scroll_target(&game, (Vector2){ 150.0f, 300.0f });
    assert(a.slot == 0);
    assert(a.row_h > 0.0f);
    assert(a.max_scroll > 0.0f);
    assert(fabsf(a.max_scroll - (float)(7 - 4) * a.row_h) < 0.5f);   /* 7 行内容 - 4 行可视 */

    MutListScroll b = screens_mut_list_scroll_target(&game, (Vector2){ 700.0f, 300.0f });
    assert(b.slot == 1 && b.max_scroll == 0.0f);

    /* 面板表头与面板外都不该命中，避免滚轮在空白处改掉某一栏 */
    MutListScroll c = screens_mut_list_scroll_target(&game, (Vector2){ 640.0f, 70.0f });
    assert(c.slot < 0);
    MutListScroll d = screens_mut_list_scroll_target(&game, (Vector2){ 20.0f, 300.0f });
    assert(d.slot < 0);
    puts("Mutation list: wheel hit-test, scroll range and no-scroll case passed");
}

/* 突变是双币：DNA 主价（offer 里）+ ATP 门票（随已购张数涨）。
 * 卡面若只判 DNA，DNA 够而 ATP 不够时会显示“可点”，点下去只弹一句「ATP 不足」，
 * 玩家看不到还差多少。这里同时锁两件事：报价必须 1:1 转发规则层、ATP 必须参与按钮可用性。 */
static void test_mutation_atp_ticket(void)
{
    GameState game;
    game_init(&game);
    game.run.active_species_count = 1;
    game.run.active_species[0].species_id = SPECIES_CYANOBACTERIUM;
    game.run.active_species[0].population = 120.0;

    int mid = -1;
    for (int id = 0; id < MUTATION_COUNT; ++id) {
        const MutationDef *def = mutation_get(id);
        if (def && def->min_phase >= MUTATION_ATP_MIN_PHASE) { mid = id; break; }
    }
    assert(mid >= 0);
    double ticket = ua_mutation_atp_cost(&game, mid);
    assert(ticket > 0.0);
    /* 转发而不是重算：与规则层必须逐值相同（k 的口径一改就露馅） */
    assert(fabs(ticket - evolution_mutation_atp_cost(&game, mid)) < 1e-9);

    game.run.mutation_offer.active = true;
    game.run.mutation_offer.species_slot = 0;
    game.run.mutation_offer.choice_count = 1;
    game.run.mutation_offer.mutation_ids[0] = mid;
    game.run.mutation_offer.dna_costs[0] = 1.0;

    memset(&g_ui, 0, sizeof g_ui);
    g_ui.screen = UI_SCREEN_MAIN;
    g_ui.overlay = UI_OVL_MUTATION;

    /* DNA 远远够、ATP 为 0 → 必须不可点 */
    game.run.dna = 1e9;
    game.run.atp = 0.0;
    screens_build(&g_ui.layout, &game, &g_ui);
    int seen = 0;
    for (int i = 0; i < g_ui.layout.count; ++i) {
        const UiButton *b = &g_ui.layout.items[i];
        if (b->id != UI_BTN_CHOOSE_MUTATION) continue;
        ++seen;
        assert(!b->enabled);
    }
    assert(seen == 1);

    /* ATP 刚好够 → 可点；差 1 点 → 又不可点（边界不能含糊） */
    game.run.atp = ticket;
    screens_build(&g_ui.layout, &game, &g_ui);
    for (int i = 0; i < g_ui.layout.count; ++i) {
        const UiButton *b = &g_ui.layout.items[i];
        if (b->id == UI_BTN_CHOOSE_MUTATION) assert(b->enabled);
    }
    game.run.atp = ticket - 1.0;
    screens_build(&g_ui.layout, &game, &g_ui);
    for (int i = 0; i < g_ui.layout.count; ++i) {
        const UiButton *b = &g_ui.layout.items[i];
        if (b->id == UI_BTN_CHOOSE_MUTATION) assert(!b->enabled);
    }
    puts("Mutation ATP ticket: forwarded 1:1 and gates the card together with DNA");
}

/* v8 P0-17 的纪元阶段封顶：界面靠 ua_speciation_era_capped 把「本纪元封顶」
 * 从一堆「前置不足」里分出来。这里锁住三件事：
 *   1) 上限随纪元走（1/2/3 = 8/10/13），且转发自规则层；
 *   2) 分类结果与 evolution_validate_speciation 不矛盾
 *      （封顶检查排在所有前置之前，越界的目标不可能判 OK）；
 *   3) 到顶后分化弹层的按钮全部不可点 —— 玩家点不出来才是真的拒绝。 */
static void test_era_stage_cap(void)
{
    GameState game;
    game_init(&game);
    memset(&g_ui, 0, sizeof g_ui);

    int ph8 = -1, ph9 = -1, ph13 = -1;
    for (int i = 0; i < SPECIES_COUNT; ++i) {
        int st = ua_species_stage(i);
        if (st == 8 && ph8 < 0) ph8 = i;
        if (st == 9 && ph9 < 0) ph9 = i;
        if (st == 13 && ph13 < 0) ph13 = i;
    }
    assert(ph8 >= 0 && ph9 >= 0 && ph13 >= 0);

    game.run.era = 1;
    assert(ua_era_stage_cap(&game) == 8);
    assert(ua_era_stage_cap(&game) == evolution_era_stage_cap(&game));
    assert(!ua_speciation_era_capped(&game, ph8));
    assert(ua_speciation_era_capped(&game, ph9));

    game.run.era = 2;
    assert(ua_era_stage_cap(&game) == 10);
    assert(!ua_speciation_era_capped(&game, ph9));

    /* 纪元 3 起上限 13 = 全表最高阶段，等于不再限制 */
    game.run.era = 3;
    assert(ua_era_stage_cap(&game) == 13);
    assert(!ua_speciation_era_capped(&game, ph13));

    /* 与规则层判定一致：任何被判 OK 的目标都没有越界 */
    game.run.era = 1;
    for (int i = 0; i < SPECIES_COUNT; ++i) {
        bool capped = ua_speciation_era_capped(&game, i);
        ActionCode code = ua_speciation_check(&game, i);
        if (capped) assert(code != ACTION_OK);
        if (code == ACTION_OK) assert(!capped);
    }

    /* 到顶：主链停在阶段 8，四张候选全是阶段 9，按钮必须全灰 */
    game.run.active_species_count = 1;
    game.run.active_species[0].species_id = SPECIES_CIVILIZATION;   /* 阶段 8 */
    game.run.biomass = 1e12;
    g_ui.screen = UI_SCREEN_MAIN;
    g_ui.overlay = UI_OVL_SPECIATION;
    screens_build(&g_ui.layout, &game, &g_ui);
    int seen = 0;
    for (int i = 0; i < g_ui.layout.count; ++i) {
        const UiButton *b = &g_ui.layout.items[i];
        if (b->id != UI_BTN_SPECIATE) continue;
        ++seen;
        assert(!b->enabled);
        assert(ua_speciation_era_capped(&game, b->target_id));
    }
    assert(seen > 0);

    /* 同一局面换到纪元 2（上限 10），阶段 9 的候选就该恢复可点 */
    game.run.era = 2;
    screens_build(&g_ui.layout, &game, &g_ui);
    for (int i = 0; i < g_ui.layout.count; ++i) {
        const UiButton *b = &g_ui.layout.items[i];
        if (b->id != UI_BTN_SPECIATE) continue;
        assert(!ua_speciation_era_capped(&game, b->target_id));
    }
    puts("Era stage cap: forwarded 1:1, classified apart from prerequisites, gates every card");
}

/* 星图同阶段旁支：绘制循环以前用 `parent phase >= child phase 就跳过` 把整类抹掉了。
 * 规则层里这类“同一时代分出去的一支”有 35 对（藻类真核生物 → 光合菌席、
 * 简单多细胞群落 → 几丁质礁……），这里锁住它们确实是同阶段亲本，
 * 免得以后又有人把同阶段边当成“反向边”一起过滤掉。 */
static void test_tree_same_phase_edges(void)
{
    assert(ua_species_stage(SPECIES_CHITIN_REEF) ==
           ua_species_stage(SPECIES_MULTICELLULAR_COLONY));

    int ps[8];
    int n = ua_species_parents(SPECIES_CHITIN_REEF, ps, 8);
    assert(n > 0);
    bool has_same_phase = false;
    for (int k = 0; k < n; ++k) {
        if (ua_species_stage(ps[k]) == ua_species_stage(SPECIES_CHITIN_REEF)) has_same_phase = true;
    }
    assert(has_same_phase);

    /* 全表统计：同阶段父子对不能是 0（是 0 说明规则表变了，注释与图例要跟着改） */
    int pairs = 0;
    for (int child = 1; child < SPECIES_COUNT; ++child) {
        int list[8];
        int m = ua_species_parents(child, list, 8);
        for (int k = 0; k < m; ++k) {
            if (ua_species_stage(list[k]) == ua_species_stage(child)) ++pairs;
        }
    }
    assert(pairs >= 20);
    printf("Tree same-phase branches: %d pairs kept drawable\n", pairs);
}

int main(void)
{
    GameState game;
    game_init(&game);
    test_routes(&game);
    test_codex_navigation();
    test_mutation_desc_scaling();
    test_mut_list_scroll();
    test_mutation_atp_ticket();
    test_era_stage_cap();
    test_tree_same_phase_edges();
    memset(&g_ui, 0, sizeof g_ui);
    g_ui.overlay = UI_OVL_TREE;
    screens_build(&g_ui.layout, &game, &g_ui);
    int nodes = 0, close = 0;
    bool node_seen[SPECIES_COUNT] = { false };
    for (int i = 0; i < g_ui.layout.count; ++i) {
        const UiButton *b = &g_ui.layout.items[i];
        if (b->id == UI_BTN_TREE_NODE) {
            ++nodes;
            assert(b->target_id >= 0 && b->target_id < SPECIES_COUNT);
            node_seen[b->target_id] = true;
        }
        close += b->id == UI_BTN_CLOSE_OVERLAY;
    }
    /* v14：星图只给“已发现 + 相邻”的节点入表，不再全量 264 个。 */
    assert(nodes > 0 && nodes <= SPECIES_COUNT && close == 1);
    for (int i = 0; i < SPECIES_COUNT; ++i) {
        if (game.run.discovered_species[i] || game.legacy.codex_species[i]) assert(node_seen[i]);
    }
    memset(&g_ui, 0, sizeof g_ui);
    game.run.buff_visible = true;
    game.run.knowledge = ua_insight_cost(&game);
    screens_build(&g_ui.layout, &game, &g_ui);
    int collect = 0, insight = 0;
    for (int i = 0; i < g_ui.layout.count; ++i) {
        const UiButton *button = &g_ui.layout.items[i];
        collect += button->id == UI_BTN_COLLECT_BUFF && button->enabled;
        insight += button->id == UI_BTN_BUY_INSIGHT && button->enabled;
    }
    assert(collect == 1 && insight == 1);
    puts("UI layout: every route page, all tree nodes, reward and insight controls passed");
    return 0;
}
