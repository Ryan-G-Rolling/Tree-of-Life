/*
 * selftest.c  ——  UI 层动作打包自检（临时，随 tests/mock 一起删除）
 *
 * 目的：验证“界面提交的 GameAction 字段填得对不对”。
 * 三个字段 target_species_slot / target_id / option 只要错一个，
 * 动作就会落到错误的目标上，而画面看起来完全正常——所以必须自动检查。
 *
 * 运行： ./treeoflife.exe --selftest
 */
#include <stdio.h>
#include <string.h>

#include "mock_game.h"
#include "mock_tables.h"
#include "ui_internal.h"

static int g_fail = 0;
static int g_pass = 0;

static void check(const char *name, int ok)
{
    if (ok) {
        g_pass++;
        printf("  [ok]   %s\n", name);
    } else {
        g_fail++;
        printf("  [FAIL] %s\n", name);
    }
}

static ActionResult act(GameState *g, ActionType type, int slot, int target, int option)
{
    GameAction a;
    memset(&a, 0, sizeof(a));
    a.type = type;
    a.target_species_slot = slot;
    a.target_id = target;
    a.option = option;
    return game_apply_action(g, a);
}

int mock_selftest(void)
{
    GameState g;
    game_init(&g);
    ActionResult r;

    printf("Tree of Life · UI action selftest\n");

    printf("[0] 按钮 → GameAction 字段映射\n");
    {
        GameAction a;
        check("点击生命体", ui_debug_map_button(UI_BTN_CLICK, 0, 0, &a) && a.type == ACTION_CLICK_ATP);
        check("升级按钮带上 target_id",
              ui_debug_map_button(UI_BTN_UPGRADE, UPGRADE_GENOME, 0, &a)
              && a.type == ACTION_BUY_RUN_UPGRADE && a.target_id == UPGRADE_GENOME);
        check("选择突变带上 mutation_id",
              ui_debug_map_button(UI_BTN_CHOOSE_MUTATION, MUTATION_NUCLEUS, 2, &a)
              && a.type == ACTION_CHOOSE_MUTATION && a.target_id == MUTATION_NUCLEUS);
        check("分化同时带 target 与 keep_parent",
              ui_debug_map_button(UI_BTN_SPECIATE, SPECIES_PROTOZOAN, 1, &a)
              && a.type == ACTION_SPECIATE && a.target_id == SPECIES_PROTOZOAN
              && a.option == 1 && a.target_species_slot == 0);
        check("永久路线带上 branch",
              ui_debug_map_button(UI_BTN_BUY_LEGACY, LEGACY_ADAPTATION, 0, &a)
              && a.type == ACTION_BUY_LEGACY && a.target_id == LEGACY_ADAPTATION);
        check("结算三连动作",
              ui_debug_map_button(UI_BTN_OPEN_END_ERA, 0, 0, &a) && a.type == ACTION_OPEN_END_ERA
              && ui_debug_map_button(UI_BTN_CANCEL_END_ERA, 0, 0, &a) && a.type == ACTION_CANCEL_END_ERA
              && ui_debug_map_button(UI_BTN_CONFIRM_END_ERA, 0, 0, &a) && a.type == ACTION_CONFIRM_END_ERA);
        check("纯 UI 按钮不产生动作", !ui_debug_map_button(UI_BTN_CLOSE_OVERLAY, 0, 0, &a));
    }
    printf("[1] 点击与升级\n");
    for (int i = 0; i < 25; ++i) r = act(&g, ACTION_CLICK_ATP, 0, 0, 0);
    check("点击 25 次累加 ATP（够买第一次升级）", r.code == ACTION_OK && g.run.atp >= 20.0);
    check("点击返回值带 ATP 增量", r.amount_spent > 0.0);

    r = act(&g, ACTION_BUY_RUN_UPGRADE, 0, UPGRADE_METABOLISM, 0);
    check("购买代谢升级 level=1", r.code == ACTION_OK && g.run.run_upgrade_levels[UPGRADE_METABOLISM] == 1);
    r = act(&g, ACTION_BUY_RUN_UPGRADE, 0, UPGRADE_GENOME, 0);
    check("生物量不足时拒绝基因升级", r.code == ACTION_NOT_ENOUGH_BIOMASS);

    printf("[2] 突变候选不刷新\n");
    r = act(&g, ACTION_GENERATE_MUTATION_OFFER, 0, 0, 0);
    check("生成候选", r.code == ACTION_OK && g.run.mutation_offer.active);
    int snapshot[MAX_MUTATION_CHOICES];
    memcpy(snapshot, g.run.mutation_offer.mutation_ids, sizeof(snapshot));
    int count_before = g.run.mutation_offer.choice_count;

    r = act(&g, ACTION_GENERATE_MUTATION_OFFER, 0, 0, 0);
    check("再次打开返回同一组候选",
          g.run.mutation_offer.choice_count == count_before
          && memcmp(snapshot, g.run.mutation_offer.mutation_ids, sizeof(snapshot)) == 0);

    printf("[3] 选择突变\n");
    g.run.dna = 300.0;
    int want = snapshot[0];
    r = act(&g, ACTION_CHOOSE_MUTATION, 0, want, 0);
    check("扣除 DNA 并写入物种突变",
          r.code == ACTION_OK && g.run.active_species[0].mutations[want]);
    check("选择后候选清空", !g.run.mutation_offer.active);
    check("突变计入本轮发现与图鉴", g.run.discovered_mutations[want] && g.legacy.codex_mutations[want]);

    printf("[4] 物种分化\n");
    g.run.active_species[0].mutations[MUTATION_PHOTOSYNTHESIS] = true;
    g.run.biomass = 500.0;
    double biomass_before = g.run.biomass;
    r = act(&g, ACTION_SPECIATE, 0, SPECIES_CYANOBACTERIUM, 0);
    check("分化成功", r.code == ACTION_OK && g.run.active_species[0].species_id == SPECIES_CYANOBACTERIUM);
    check("扣除生物量", g.run.biomass < biomass_before);
    check("写入物种发现", g.run.discovered_species[SPECIES_CYANOBACTERIUM]);
    check("后代继承亲本突变", g.run.active_species[0].mutations[MUTATION_PHOTOSYNTHESIS]);

    printf("[5] 纪元结算\n");
    r = act(&g, ACTION_OPEN_END_ERA, 0, 0, 0);
    check("进入预览并暂停", r.code == ACTION_OK && g.phase == PHASE_END_ERA_PREVIEW);
    double atp_frozen = g.run.atp;
    game_update(&g, 1.0);
    check("预览阶段不推进模拟", g.run.atp == atp_frozen);
    r = act(&g, ACTION_CANCEL_END_ERA, 0, 0, 0);
    check("取消后回到 RUNNING", r.code == ACTION_OK && g.phase == PHASE_RUNNING);

    act(&g, ACTION_OPEN_END_ERA, 0, 0, 0);
    r = act(&g, ACTION_CONFIRM_END_ERA, 0, 0, 0);
    int ep_gain = (int)r.amount_spent;
    check("确认后进入 LEGACY 并发放 EP",
          r.code == ACTION_OK && g.phase == PHASE_LEGACY && g.legacy.evolution_points == ep_gain && ep_gain > 0);
    r = act(&g, ACTION_CONFIRM_END_ERA, 0, 0, 0);
    check("二次确认被拒绝", r.code != ACTION_OK);

    printf("[6] 永久强化与新轮\n");
    int ep_before = g.legacy.evolution_points;
    r = act(&g, ACTION_BUY_LEGACY, 0, LEGACY_MUTATION, 0);
    check("购买永久强化", r.code == ACTION_OK && g.legacy.levels[LEGACY_MUTATION] == 1);
    check("扣除 EP", g.legacy.evolution_points == ep_before - 1);

    int era_before = g.run.era;
    r = act(&g, ACTION_START_NEW_RUN, 0, 0, 0);
    check("开始新轮回到 RUNNING", r.code == ACTION_OK && g.phase == PHASE_RUNNING);
    check("新轮纪元 +1", g.run.era == era_before + 1);
    check("资源与突变清零", g.run.atp == 0.0 && g.run.dna == 0.0 && !g.run.active_species[0].mutations[want]);
    check("图鉴与强化保留",
          g.legacy.levels[LEGACY_MUTATION] == 1 && g.legacy.codex_species[SPECIES_CYANOBACTERIUM]);

    printf("[7] 存档往返\n");
    save_write("saves/selftest.sav", &g);
    GameState loaded;
    memset(&loaded, 0, sizeof(loaded));
    SaveResult sr = save_read("saves/selftest.sav", &loaded);
    check("读档成功", sr == SAVE_OK);
    check("状态一致",
          loaded.phase == g.phase
          && loaded.run.era == g.run.era
          && loaded.run.active_species[0].species_id == g.run.active_species[0].species_id
          && loaded.legacy.evolution_points == g.legacy.evolution_points);

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
