/* Regression: displayed click gain must equal actual ATP added, not the zero cost. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "ui.h"
#include "ui_internal.h"

static void check_click(int run_level, int legacy_level)
{
    GameState game;
    game_init(&game);
    game.run.run_upgrade_levels[UPGRADE_METABOLISM] = run_level;
    game.legacy.levels[LEGACY_METABOLISM] = legacy_level;
    memset(&g_ui, 0, sizeof g_ui);
    GameAction click = { ACTION_CLICK_ATP, 0, 0, 0 };
    for (int i = 0; i < 5; ++i) {
        double before = game.run.atp;
        ActionResult result = game_apply_action(&game, click);
        assert(result.code == ACTION_OK);
        assert(result.amount_spent == 0.0);
        double actual_gain = game.run.atp - before;
        assert(actual_gain > 0.0);
        ui_notify_result(&game, click, result);
        int slot = (g_ui.gain_slot - 1) % 3;
        assert(g_ui.gain_t[slot] > 0.0f);
        assert(fabs(g_ui.gain_val[slot] - actual_gain) < 1e-9);
    }
}

int main(void)
{
    check_click(0, 0);
    check_click(3, 0);
    check_click(0, 4);
    check_click(7, 8);
    puts("Click feedback: 20 clicks across base/run/legacy/combined bonuses match actual ATP gains");
    return 0;
}
