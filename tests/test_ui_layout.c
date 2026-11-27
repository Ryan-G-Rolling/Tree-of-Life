/* Real layout and adapters without a window, drawing calls or rule substitutes. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "evolution.h"
#include "ui_adapter.h"
#include "ui_internal.h"

static void test_routes(GameState *game)
{
    for (int parent = 0; parent < SPECIES_COUNT; ++parent) {
        game->run.active_species[0].species_id = parent;
        int children[SPECIES_COUNT];
        int total = species_children(parent, children, SPECIES_COUNT);
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
    memset(&g_ui, 0, sizeof g_ui);
    g_ui.overlay = UI_OVL_TREE;
    screens_build(&g_ui.layout, &game, &g_ui);
    int nodes = 0, close = 0;
    for (int i = 0; i < g_ui.layout.count; ++i) {
        nodes += g_ui.layout.items[i].id == UI_BTN_TREE_NODE;
        close += g_ui.layout.items[i].id == UI_BTN_CLOSE_OVERLAY;
    }
    assert(nodes == SPECIES_COUNT && close == 1);
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
