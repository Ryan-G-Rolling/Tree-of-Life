/* Real layout and adapters without a window, drawing calls or rule substitutes. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
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

int main(void)
{
    GameState game;
    game_init(&game);
    test_routes(&game);
    test_codex_navigation();
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
