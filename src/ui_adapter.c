#include <stdio.h>
#include <string.h>
#include <math.h>
#include "ui_adapter.h"
#include "economy.h"
#include "evolution.h"
#include "environment.h"
#include "legacy.h"
#include "balance.h"

double ua_run_upgrade_cost(const GameState *g, RunUpgradeType t) { return g->run.run_upgrade_levels[t] >= RUN_UPGRADE_MAX_LEVEL ? INFINITY : economy_upgrade_cost(t,g->run.run_upgrade_levels[t]); }
const char *ua_run_upgrade_title(RunUpgradeType t) { static const char *n[]={"代谢效率","繁殖能力","基因复制"}; return n[t]; }
const char *ua_run_upgrade_effect(const GameState *g,RunUpgradeType t,char *b,int n) { snprintf(b,n,"产出效果 ×%.2f / Lv.%d",economy_run_multiplier(t,g->run.run_upgrade_levels[t]),RUN_UPGRADE_MAX_LEVEL); return b; }
void ua_total_rates(const GameState *g,double out[3]) { out[0]=out[1]=out[2]=0; for(int i=0;i<g->run.active_species_count;i++){SpeciesRates r=evolution_calculate_species_rates(g,i);out[0]+=r.atp_per_second;out[1]+=r.biomass_per_second;out[2]+=r.dna_per_second;} }
double ua_click_atp(const GameState *g) { return economy_click_atp(g); }
const char *ua_species_name(int id) { return species_name(id); }
const char *ua_ecology_name(int id) { static const char *n[]={"原始生命","光合","化能","异养","多细胞","极端环境"}; int e=species_ecology(id); return e>=0&&e<6?n[e]:"未知"; }
const char *ua_stage_name(int s) { static const char *n[]={"原始生命","原核生物","真核生物","多细胞","组织分化","认知生态","智慧生命","技术圈","文明","太空时代","奇点","超越"};return s>=0&&s<12?n[s]:"未知"; }
int ua_species_stage(int id) { return species_phase(id); }
void ua_species_rates(const GameState *g,int slot,SpeciesRates *out) { *out=evolution_calculate_species_rates(g,slot); }
const char *ua_fitness_breakdown(const GameState *g,int slot,char *b,int n) { int t=environment_damage_trait(g->run.current_environment_id); if(t<0) snprintf(b,n,"环境倍率 ×%.2f",environment_multiplier_for_slot(g,slot));else snprintf(b,n,"特征 %d / 难度 %d · 环境 ×%.2f",evolution_slot_trait_total(g,slot,t),environment_difficulty(g->run.current_environment_id),environment_multiplier_for_slot(g,slot));return b; }
int ua_speciation_children(const GameState *g,int *out,int n) { return species_children(g->run.active_species[evolution_main_slot(g)].species_id,out,n); }
int ua_species_parents(int id,int *out,int max) { int n=0;for(int p=0;p<SPECIES_COUNT&&n<max;p++)if(species_has_parent(id,p))out[n++]=p;return n; }
double ua_speciation_cost(const GameState *g,int id) { return evolution_speciation_cost(g,id); }
ActionCode ua_speciation_check(const GameState *g,int id) { return evolution_validate_speciation(g,evolution_main_slot(g),id); }
const char *ua_speciation_requirement(int id,char *b,int n) { int ids[MAX_MUTATIONS],k=species_required_mutations(id,ids,MAX_MUTATIONS),used=snprintf(b,n,"需要：");for(int i=0;i<k&&used<n;i++)used+=snprintf(b+used,n-used,"%s%s",i?" + ":"",mutation_get(ids[i])->name);int branch,level;if(species_requires_legacy(id,&branch,&level)&&used<n)snprintf(b+used,n-used," · %s Lv.%d",ua_legacy_name(branch),level);return b; }
const char *ua_mutation_name(int id) { const MutationDef *d=mutation_get(id);return d?d->name:"未知突变"; }
const char *ua_mutation_desc(int id,char *b,int n) { const MutationDef *d=mutation_get(id);snprintf(b,n,"%s",d?d->description:"-");return b; }
bool ua_mutation_is_rare(int id) { const MutationDef *d=mutation_get(id);return d&&d->rarity>0; }
const char *ua_environment_name(int id) { return environment_name(id); }
const char *ua_environment_desc(int id,char *b,int n) { snprintf(b,n,"%s",environment_description(id));return b; }
int ua_environment_trait(int id) { return environment_damage_trait(id); }
void ua_era_breakdown(const GameState *g,int out[5]) { prestige_run_stats(g,out,out+1,out+2,out+3);out[4]=prestige_biomass_milestone_level(g->run.max_biomass); }
void ua_era_reward(const GameState *g,EraReward *out) { *out=prestige_preview_reward(g); }
double ua_milestone_threshold(int level) { return prestige_biomass_milestone_threshold(level); }
const char *ua_legacy_name(int branch) { static const char *n[]={"Metabolism 代谢","Mutation 突变","Adaptation 适应","Diversification 多样性"};return branch>=0&&branch<4?n[branch]:"未知"; }
int ua_legacy_cost(int branch,int level) { (void)branch;return legacy_upgrade_cost(level); }
const char *ua_legacy_effect(const GameState *g,int branch,int level,char *b,int n) {
    (void)g;
    static const char *text[4][8]={
        {"ATP 产出 ×1.08","生物量 ×1.08","新轮 +50 ATP，代谢 Lv.1","全部产出 ×1.12","生物量 ×1.05，解锁高效呼吸","点击收益 ×1.5","新轮 +100 生物量","全部产出 ×1.10"},
        {"突变 DNA -6%","突变 DNA -12%","三个突变候选","提高稀有突变权重","关键突变更早保底","四个突变候选","突变 DNA -20%","进一步提高稀有权重"},
        {"减轻环境惩罚","危机持续时间 ×0.85","提前预览下一事件","提高正面环境收益","每轮一次危机保护","危机伤害 -15%","正面事件时间 ×1.5","危机伤害 -30%，强化护盾"},
        {"种群上限 ×1.10","额外物种槽","开启多物种协同","分化成本 -15%","第三物种槽与极端分支","种群上限再 ×1.15","生态协同 ×1.15","分化进一步优惠"}
    };
    snprintf(b,n,"%s",branch>=0&&branch<4&&level>=1&&level<=8?text[branch][level-1]:"尚未强化");return b;
}
int ua_achievement_count(void) { return 0; }
const char *ua_achievement_name(int i) { (void)i;return ""; }
const char *ua_achievement_desc(int i) { (void)i;return ""; }
void ua_register_font_strings(void) {
    for(int i=0;i<SPECIES_COUNT;i++){ui_font_register(species_name(i));ui_font_register(species_description(i));ui_font_register(ua_ecology_name(i));}
    for(int i=0;i<MUTATION_COUNT;i++){ui_font_register(ua_mutation_name(i));ui_font_register(mutation_get(i)->description);}
    for(int i=0;i<ENV_COUNT;i++){ui_font_register(environment_name(i));ui_font_register(environment_description(i));}
    for(int i=0;i<12;i++)ui_font_register(ua_stage_name(i));
    for(int b=0;b<4;b++){ui_font_register(ua_legacy_name(b));for(int l=1;l<=8;l++){char text[192];ua_legacy_effect(NULL,b,l,text,sizeof text);ui_font_register(text);}}
    for(int i=0;i<evolution_sub_upgrade_count();i++){const SubUpgradeDef *d=evolution_sub_upgrade_get(i);ui_font_register(d->name);ui_font_register(d->description);}
    ui_font_register("需要特征难度环境倍率产出效果容量扩张应急储备理念科技已满级页滚轮切换分支");
}
