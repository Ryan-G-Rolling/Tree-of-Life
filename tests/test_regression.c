/* Real A+B integration: actions, deterministic stepping, crisis commit, save and eras. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "evolution.h"
#include "environment.h"
#include "legacy.h"
#include "save.h"
#include "balance.h"
#include "buffs.h"
static ActionResult act(GameState *g,ActionType type,int id,int option) {
    return game_apply_action(g,(GameAction){type,0,id,option});
}
static void seed(GameState *g,unsigned int rng) { game_init(g);g->run.rng_state=rng;environment_init_run(g);buffs_init_run(g); }
static void test_actions_and_save(void) {
    GameState g;seed(&g,77);
    assert(g.run.era==1 && g.run.next_environment_id>0);
    assert(act(&g,ACTION_GENERATE_MUTATION_OFFER,0,0).code==ACTION_OK);
    GameState same=g;
    assert(act(&g,ACTION_GENERATE_MUTATION_OFFER,0,0).code==ACTION_OK);
    assert(memcmp(&g,&same,sizeof g)==0);
    int id=g.run.mutation_offer.mutation_ids[0];
    assert(act(&g,ACTION_CHOOSE_MUTATION,id,0).code==ACTION_NOT_ENOUGH_DNA);
    assert(memcmp(&g,&same,sizeof g)==0);
    g.run.dna=1000000.;g.run.biomass=g.run.max_biomass=1000000.;g.run.atp=1000000.;
    assert(act(&g,ACTION_CHOOSE_MUTATION,id,0).code==ACTION_OK);
    assert(g.legacy.codex_mutations[id] && !g.run.mutation_offer.active);
    /* Funded fixture isolates the speciation commit from random offer selection. */
    g.run.active_species[0].mutations[MUTATION_PHOTOSYNTHESIS]=true;
    g.legacy.levels[LEGACY_DIVERSIFICATION]=5;
    assert(act(&g,ACTION_SPECIATE,SPECIES_CYANOBACTERIUM,1).code==ACTION_OK);
    assert(g.run.active_species_count==2 && g.run.active_species[0].species_id==SPECIES_CYANOBACTERIUM);
    g.run.active_species[0].mutations[MUTATION_NUCLEUS]=true;
    assert(act(&g,ACTION_SPECIATE,SPECIES_ALGAL_EUKARYOTE,1).code==ACTION_OK);
    assert(g.run.active_species_count==3);
    for(int i=0;i<4;i++) assert(act(&g,ACTION_BUY_CAPACITY,0,0).code==ACTION_OK);
    double atp=g.run.atp,bio=g.run.biomass;
    double ca=evolution_capacity_atp_cost(&g), cb=evolution_capacity_biomass_cost(&g);
    assert(ca>0 && cb==0); /* v8：容量为纯 ATP，Biomass 份额为 0 */
    assert(act(&g,ACTION_BUY_CAPACITY,0,0).code==ACTION_OK);
    assert(fabs(atp-g.run.atp-ca)<1e-7 && fabs(bio-g.run.biomass-cb)<1e-7);
    assert(act(&g,ACTION_BUY_RESERVE,0,0).code==ACTION_OK);
    g.run.active_species[0].species_id=SPECIES_SAPIENT_LIFE;g.run.knowledge=100000.;
    assert(act(&g,ACTION_BUY_SUB_UPGRADE,0,0).code==ACTION_OK);
    game_update(&g,1.0); assert(g.run.knowledge>0);
    g.legacy.levels[LEGACY_MUTATION]=8;
    assert(act(&g,ACTION_GENERATE_MUTATION_OFFER,0,0).code==ACTION_OK);
    assert(g.run.mutation_offer.choice_count==4);
    assert(save_write("build/integration-save.bin",&g)==SAVE_OK);
    GameState restored; assert(save_read("build/integration-save.bin",&restored)==SAVE_OK);
    assert(memcmp(&g,&restored,sizeof g)==0);
    /* Save round trip preserves the next event and random offer continuation. */
    GameState before=g;
    assert(act(&g,ACTION_OPEN_END_ERA,0,0).code==ACTION_OK);
    EraReward reward=prestige_preview_reward(&g);int ep=g.legacy.evolution_points;
    assert(act(&g,ACTION_CONFIRM_END_ERA,0,0).code==ACTION_OK);
    assert(g.legacy.evolution_points==ep+reward.total_points);
    same=g;assert(act(&g,ACTION_CONFIRM_END_ERA,0,0).code!=ACTION_OK);assert(memcmp(&same,&g,sizeof g)==0);
    g.legacy.evolution_points=100;
    while(g.legacy.levels[0]<8)assert(act(&g,ACTION_BUY_LEGACY,0,0).code==ACTION_OK);
    assert(act(&g,ACTION_BUY_LEGACY,0,0).code==ACTION_LEVEL_MAXED);
    LegacyState legacy=g.legacy;
    assert(act(&g,ACTION_START_NEW_RUN,0,0).code==ACTION_OK);
    assert(g.run.era==2 && g.run.dna==LEGACY_STARTING_DNA && g.run.atp==50 && g.run.biomass==100);
    assert(memcmp(&legacy,&g.legacy,sizeof legacy)==0 && g.run.rng_state!=before.run.rng_state);
    assert(save_write("build/integration-save.bin",&g)==SAVE_OK);
    g.run.knowledge=NAN;assert(save_write("build/integration-save.bin",&g)==SAVE_INVALID_DATA);
    assert(save_read("build/integration-save.bin",&restored)==SAVE_OK && isfinite(restored.run.knowledge));
    puts("Integration: mutations, three lineages, mixed costs, knowledge, legacy Lv.8 and saves passed");
}
static void test_crisis_and_chunks(void) {
    GameState g;seed(&g,91);g.run.active_species[0].species_id=SPECIES_MULTICELLULAR_COLONY;
    g.run.active_species[0].population=100.;g.run.next_environment_id=ENV_METEOR_IMPACT;g.run.event_interval_left=0.1;
    double expected=fmin(evolution_calculate_species_rates(&g,0).final_population_cap,
                         100.+evolution_calculate_species_rates(&g,0).population_delta_per_second*0.1);
    game_update(&g,0.1);
    assert(g.run.current_environment_id==ENV_METEOR_IMPACT);
    double loss=fmin(METEOR_INSTANT_LOSS_RATIO+INSTANT_LOSS_STAGE_SCALE*3.0,INSTANT_LOSS_MAX);
    assert(fabs(g.run.active_species[0].population-expected*(1.-loss))<1e-7);
    assert(fabs(g.run.crisis_start_population-expected*(1.-loss))<1e-7);
    assert(g.run.crisis_min_population==g.run.active_species[0].population);
    for(int i=0;i<600;i++)game_update(&g,0.1);
    assert(g.run.next_environment_id>0 || g.run.current_environment_id>0);
    GameState a,b;seed(&a,13);game_update(&a,120.0);seed(&b,13);for(int i=0;i<1200;i++)game_update(&b,0.1);
    assert(memcmp(&a,&b,sizeof a)==0);
    puts("Integration: meteor applied once, event cycle and deterministic time chunks passed");
}
static void test_natural_playthrough(void) {
    GameState g; seed(&g,20261126u);
    int buys=0, mutations=0, splits=0;
    for(int second=0;second<3600;second++) {
        for(int c=0;c<5;c++)assert(act(&g,ACTION_CLICK_ATP,0,0).code==ACTION_OK);
        for(int u=0;u<3;u++) {
            if(g.run.run_upgrade_levels[u]<10 && (u!=UPGRADE_GENOME || g.run.biomass>1000))
                if(act(&g,ACTION_BUY_RUN_UPGRADE,u,0).code==ACTION_OK)buys++;
        }
        if(!g.run.mutation_offer.active)act(&g,ACTION_GENERATE_MUTATION_OFFER,0,0);
        /* v9 P1-11：先攒主线门票（否则便宜的侧支突变会把 DNA 花光，
           而突变价格随已拥有数量指数上涨，主线门票反而永远买不起），
           再退到任意买得起的候选。主线后继不再从 species_children 里挑，
           所以这里不需要子代数组（远端把越界写改成 SPECIES_COUNT，一并保留在别处）。 */
        int selected=-1,main_next=species_main_child(g.run.active_species[0].species_id);
        int main_req[4],main_req_n=main_next>=0?species_required_mutations(main_next,main_req,4):0;
        for(int i=0;i<g.run.mutation_offer.choice_count && selected<0;i++) {
            int id=g.run.mutation_offer.mutation_ids[i];
            if(g.run.dna<g.run.mutation_offer.dna_costs[i])continue;
            for(int r=0;r<main_req_n;r++)if(main_req[r]==id)selected=id;
        }
        for(int i=0;i<g.run.mutation_offer.choice_count && selected<0;i++) {
            int id=g.run.mutation_offer.mutation_ids[i];
            if(g.run.dna<g.run.mutation_offer.dna_costs[i])continue;
            selected=id;
        }
        if(selected>=0 && act(&g,ACTION_CHOOSE_MUTATION,selected,0).code==ACTION_OK)mutations++;
        /* v9 P1-11：只沿主线后继分化。186 个物种里有 156 个是终点物种，
           随口挑 children[0] 会把演示局带进“阶段 0 死胡同”，
           侧支走法由 test_b 的可达性与 1000 种子批量覆盖。 */
        if(main_next>=0 && act(&g,ACTION_SPECIATE,main_next,0).code==ACTION_OK)splits++;
        game_update(&g,1.0);
        assert(fabs(g.run.elapsed_time-(second+1.0))<1e-6);
        assert(isfinite(g.run.atp) && isfinite(g.run.dna) && isfinite(g.run.biomass));
        if(second==599) assert(save_write("build/playthrough.sav",&g)==SAVE_OK);
    }
    /* v8 后规则更贵，简单策略在 60 分钟内通常只完成 1 次分化；
       接线的目标是“动作能驱动 B”，多次分化由 test_b 覆盖。 */
    assert(mutations>=3 && splits>=1 && buys>=3);
    assert(save_write("build/playthrough-final.sav",&g)==SAVE_OK);
    EraReward r=prestige_preview_reward(&g);
    printf("Natural 60-minute run: stage %d, %d mutations, %d speciations, %d upgrades, reward %d EP\n",evolution_main_stage(&g),mutations,splits,buys,r.total_points);
}
int main(void) { test_actions_and_save();test_crisis_and_chunks();test_natural_playthrough();puts("All real A+B integration checks passed");return 0; }

