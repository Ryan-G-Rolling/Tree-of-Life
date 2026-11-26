/*
 * test_integration.c  ——  三方集成冒烟测试
 *
 * 与 A/B 各自的单测不同，这里只验证“接线正确”：
 *   A 的 game_apply_action 是否真的驱动了 B 的突变/结算实现，
 *   以及存档能否在真实 GameState 上往返。
 *
 * 规则正确性由 tests/test_b.c（36,498 项）覆盖，这里不重复。
 */
#include <stdio.h>
#include <string.h>

#include "game.h"
#include "save.h"
#include "balance.h"

static int passed = 0;
static int failed = 0;

#define CHECK(cond)                                                      \
    do {                                                                 \
        if (cond) {                                                      \
            ++passed;                                                    \
        } else {                                                         \
            ++failed;                                                    \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);       \
        }                                                                \
    } while (0)

static GameAction act(ActionType type, int slot, int id, int option)
{
    GameAction a;
    a.type = type;
    a.target_species_slot = slot;
    a.target_id = id;
    a.option = option;
    return a;
}

int main(void)
{
    GameState g;
    game_init(&g);

    /* ---- 初始化：A 建立 RunState，B 预抽首个事件 ---- */
    CHECK(g.phase == PHASE_RUNNING);
    CHECK(g.run.active_species_count == 1);
    CHECK(g.run.active_species[0].species_id == SPECIES_PROTOCELL);
    CHECK(g.run.rng_state != 0u);
    CHECK(g.save_version == SAVE_VERSION);

    /* ---- 固定步推进：A 累积资源，B 计算产率与环境 ---- */
    for (int i = 0; i < 1200; ++i) game_update(&g, 0.1);   /* 120 s */
    CHECK(g.run.elapsed_time > 119.0);
    CHECK(g.run.dna > 0.0);
    CHECK(g.run.biomass > 0.0);

    /* ---- 点击 ATP（A 的动作入口） ---- */
    ActionResult r = game_apply_action(&g, act(ACTION_CLICK_ATP, 0, 0, 0));
    CHECK(r.code == ACTION_OK);
    CHECK(g.run.atp > 0.0);

    /* ---- 买一次代谢升级（A 的价格来自 B 的平衡表） ---- */
    if (g.run.atp >= 20.0) {
        int before = g.run.run_upgrade_levels[UPGRADE_METABOLISM];
        r = game_apply_action(&g, act(ACTION_BUY_RUN_UPGRADE, 0, UPGRADE_METABOLISM, 0));
        CHECK(r.code == ACTION_OK);
        CHECK(g.run.run_upgrade_levels[UPGRADE_METABOLISM] == before + 1);
    }

    /* ---- 开突变候选：A 调 B 的 evolution_open_mutation_offer ---- */
    r = game_apply_action(&g, act(ACTION_GENERATE_MUTATION_OFFER, 0, 0, 0));
    CHECK(r.code == ACTION_OK);
    CHECK(g.run.mutation_offer.active);
    CHECK(g.run.mutation_offer.choice_count >= 1);

    /* ---- 选突变：A 调 B 的 evolution_apply_mutation（B 内部扣 DNA） ---- */
    if (g.run.mutation_offer.active && g.run.mutation_offer.choice_count > 0) {
        int mid = g.run.mutation_offer.mutation_ids[0];
        double cost = g.run.mutation_offer.dna_costs[0];
        g.run.dna = cost + 1.0;   /* 测试补足资源，不依赖时间累积 */
        double before = g.run.dna;
        r = game_apply_action(&g, act(ACTION_CHOOSE_MUTATION, 0, mid, 0));
        CHECK(r.code == ACTION_OK);
        CHECK(g.run.active_species[0].mutations[mid]);
        CHECK(!g.run.mutation_offer.active);          /* B 在提交时清空候选 */
        CHECK(g.run.dna < before);                    /* 且扣了款 */
        CHECK(g.run.discovered_mutations[mid]);
    }

    /* ---- 存档往返：A 的 save.c 在真实结构上工作 ---- */
    CHECK(save_write("build/_integration.sav", &g) == SAVE_OK);
    GameState loaded;
    memset(&loaded, 0, sizeof loaded);
    CHECK(save_read("build/_integration.sav", &loaded) == SAVE_OK);
    CHECK(loaded.run.active_species[0].species_id == g.run.active_species[0].species_id);
    CHECK(loaded.run.atp == g.run.atp);
    CHECK(loaded.run.dna == g.run.dna);

    /* ---- 结算：A 开预览，B 入账 EP 并切 Legacy ---- */
    /* 阶段 0 的公式结算为 0 EP（2×0），所以先模拟“已分化到阶段 2”，
       否则测不到入账逻辑。这不是绕过规则，是构造一个有产出的局面。 */
    g.run.active_species[0].species_id = SPECIES_ALGAL_EUKARYOTE;
    g.run.discovered_species[SPECIES_ALGAL_EUKARYOTE] = true;
    g.legacy.codex_species[SPECIES_ALGAL_EUKARYOTE] = true;

    r = game_apply_action(&g, act(ACTION_OPEN_END_ERA, 0, 0, 0));
    CHECK(r.code == ACTION_OK);
    CHECK(g.phase == PHASE_END_ERA_PREVIEW);
    r = game_apply_action(&g, act(ACTION_CONFIRM_END_ERA, 0, 0, 0));
    CHECK(r.code == ACTION_OK);
    CHECK(g.phase == PHASE_LEGACY);
    CHECK(g.run.reward_claimed);
    CHECK(g.legacy.evolution_points > 0);

    /* ---- 重复结算应被 B 拒绝 ---- */
    r = game_apply_action(&g, act(ACTION_CONFIRM_END_ERA, 0, 0, 0));
    CHECK(r.code != ACTION_OK);

    /* ---- 开始在轮：A 重置 RunState，B 重设环境与永久奖励 ---- */
    r = game_apply_action(&g, act(ACTION_START_NEW_RUN, 0, 0, 0));
    CHECK(r.code == ACTION_OK);
    CHECK(g.phase == PHASE_RUNNING);
    CHECK(g.run.active_species[0].species_id == SPECIES_PROTOCELL);
    CHECK(g.legacy.evolution_points > 0);   /* 永久 EP 不随新轮清空 */

    printf("%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
