/*
 * Member B · species.c
 * 物种数据表、阶段、适应特征与分化前置条件
 *
 * 数据来源：02-数据与计算规则 §5.3、§5.4、§7.1
 * 本文件只提供数据与查询，不做资源结算，也不包含 raylib.h。
 */

#include "evolution.h"
#include "balance.h"

typedef struct {
    int target;
    int parents[3];      /* -1 结束 */
    int required[4];     /* 必须已拥有的突变，-1 结束 */
    int req_branch;      /* 需要的永久路线，-1 表示无 */
    int req_level;
} SpeciationRule;

static const SpeciesDef k_species[SPECIES_COUNT] = {
    /* 0 原始细胞 */
    { "Protocell", "A membrane-bound primitive cell. The seed of every lineage.",
      0, ECO_PRIMITIVE, 0.80, 0.25, 0.08, 0.0, 0.010, 80.0, 0.0,
      { 0, 0, 0, 0, 0 } },
    /* 1 光合细菌（第十轮：80 → 55，把首次分化压进 180s） */
    { "Cyanobacterium", "Harvests light. Strong under low light, fragile in heat.",
      1, ECO_PHOTOSYNTHETIC, 1.30, 0.38, 0.10, 0.0, 0.012, 160.0, 55.0,
      { 1, 0, -1, 0, 0 } },
    /* 2 化能细菌（第十轮：同上） */
    { "Chemolithotroph", "Feeds on chemical gradients. Tolerates toxins and darkness.",
      1, ECO_CHEMOSYNTHETIC, 1.10, 0.48, 0.10, 0.0, 0.011, 150.0, 55.0,
      { 1, 0, 2, 0, 1 } },
    /* 3 藻类真核生物 */
    { "Algal Eukaryote", "Photosynthetic eukaryote with a nucleus and mitochondria.",
      2, ECO_PHOTOSYNTHETIC, 2.00, 0.72, 0.16, 0.0, 0.013, 260.0, 350.0,
      { 0, 0, -1, 1, 0 } },
    /* 4 原生捕食者 */
    { "Protozoan Predator", "Hunts other cells. Thrives in oxygen-rich water.",
      2, ECO_HETEROTROPHIC, 1.75, 0.68, 0.20, 0.0, 0.014, 240.0, 350.0,
      { 0, 0, 0, 2, 0 } },
    /* 5 简单多细胞群落 */
    { "Multicellular Colony", "Divided cells working as one organism.",
      3, ECO_MULTICELLULAR, 3.00, 1.20, 0.26, 0.0, 0.012, 450.0, 1200.0,
      { 0, 1, 0, 1, 0 } },
    /* 6 极端环境群落 */
    { "Extremophile Colony", "Survives heat, cold and poison where others collapse.",
      3, ECO_EXTREMOPHILE, 2.50, 1.00, 0.28, 0.0, 0.010, 380.0, 1000.0,
      { 2, 2, 1, 0, 2 } },
    /* 7 阶段 4：分化组织体 */
    { "Differentiated Tissue", "Specialised tissues divide labour across the colony.",
      4, ECO_MULTICELLULAR, 3.40, 1.40, 0.34, 0.0, 0.011, 600.0, 3000.0,
      { 1, 1, 0, 1, 0 } },
    /* 8 阶段 4：共生超级个体 */
    { "Symbiotic Superorganism", "Partner species trade energy and protection.",
      4, ECO_EXTREMOPHILE, 3.20, 1.35, 0.36, 0.0, 0.010, 560.0, 2800.0,
      { 2, 1, 0, 0, 2 } },
    /* 9 阶段 5：认知生物圈 */
    { "Cognitive Biosphere", "A planetary network of tissues that senses and adapts.",
      5, ECO_MULTICELLULAR, 4.20, 2.00, 0.48, 0.0, 0.010, 900.0, 8000.0,
      { 2, 2, 1, 1, 2 } },
    /* 10 阶段 4：真菌网络 */
    { "Fungal Network", "Thread-like cells digest detritus and share nutrients.",
      4, ECO_HETEROTROPHIC, 3.10, 1.30, 0.42, 0.0, 0.011, 520.0, 2800.0,
      { 0, 0, 0, 0, 2 } },
    /* 11 阶段 4：珊瑚共生体 */
    { "Coral Symbiont", "A colonial host farmed by photosynthetic partners.",
      4, ECO_PHOTOSYNTHETIC, 3.30, 1.50, 0.30, 0.0, 0.012, 640.0, 3200.0,
      { 0, 0, 1, 0, 0 } },
    /* 12 阶段 5：菌根网络 */
    { "Mycorrhizal Web", "A root-fungus network trades nutrients across the colony.",
      5, ECO_HETEROTROPHIC, 4.00, 1.70, 0.55, 0.0, 0.010, 780.0, 6000.0,
      { 0, 1, 0, 0, 1 } },
    /* 13 阶段 5：礁城 */
    { "Reef Metropolis", "A layered reef city built by photosynthetic partners.",
      5, ECO_PHOTOSYNTHETIC, 4.40, 1.90, 0.42, 0.0, 0.011, 860.0, 6500.0,
      { 0, 0, 1, 0, 0 } },
    /* 14 阶段 6：智慧生命（起产理念） */
    { "Sapient Life", "A biosphere that models itself and plans ahead.",
      6, ECO_MULTICELLULAR, 5.20, 2.60, 0.70, 0.30, 0.010, 1200.0, 12000.0,
      { 2, 2, 1, 2, 1 } },
    /* 15 阶段 7：技术圈 */
    { "Technosphere", "Industry and information wrap the whole planet.",
      7, ECO_HETEROTROPHIC, 6.00, 3.00, 0.85, 0.40, 0.010, 1500.0, 45000.0,
      { 2, 2, 1, 2, 2 } },
    /* 16 阶段 8：文明 */
    { "Civilization", "Cities, language and shared institutions span the planet.",
      8, ECO_HETEROTROPHIC, 7.50, 3.80, 1.00, 0.55, 0.010, 2000.0, 300000.0,
      { 2, 2, 2, 2, 2 } },
    /* 17 阶段 9：太空时代 */
    { "Space Age", "Orbital habitats and industry leave the home world.",
      9, ECO_EXTREMOPHILE, 9.00, 4.80, 1.20, 0.75, 0.010, 2800.0, 1000000.0,
      { 3, 3, 2, 2, 2 } },
    /* 18 阶段 10：奇点 */
    { "Singularity", "Self-improving intelligence folds the biosphere into itself.",
      10, ECO_MULTICELLULAR, 12.00, 6.00, 1.60, 1.00, 0.010, 4000.0, 3000000.0,
      { 3, 3, 3, 3, 3 } },
    /* 19 阶段 11：超越（第十二轮） */
    { "Transcendence", "Mind and matter merge into a single self-designing process.",
      11, ECO_MULTICELLULAR, 18.00, 9.00, 2.40, 1.60, 0.010, 6000.0, 4000000.0,
      { 3, 3, 3, 3, 3 } },
    /* 第十三轮：阶段 2 侧支 */
    { "Photosynthetic Mat", "Partner algae turn sunlight into a shared energy pool.",
      2, ECO_PHOTOSYNTHETIC, 2.20, 0.90, 0.18, 0.0, 0.012, 300.0, 450.0,
      { 0, 0, -1, 1, 0 } },
    { "Vent Swarm", "Heat-loving cells swarm the chemical vents in relays.",
      2, ECO_CHEMOSYNTHETIC, 1.50, 0.95, 0.22, 0.0, 0.012, 260.0, 450.0,
      { 1, 0, 2, 0, 2 } },
    /* 第十三轮：阶段 3 侧支 */
    { "Chitin Reef", "A hard chitin lattice shelters a growing colony.",
      3, ECO_HETEROTROPHIC, 2.60, 1.50, 0.28, 0.0, 0.011, 620.0, 1400.0,
      { 1, 1, 0, 0, 1 } },
    /* 第十三轮：阶段 5 侧支 */
    { "Saprotrophic Forest", "A forest of decomposers recycles everything.",
      5, ECO_HETEROTROPHIC, 3.80, 1.90, 0.60, 0.0, 0.010, 820.0, 7000.0,
      { 1, 1, 0, 0, 2 } },
    /* 第十三轮：阶段 6 侧支 */
    { "Hive Polity", "A quorum-driven hive coordinates every caste at once.",
      6, ECO_MULTICELLULAR, 5.40, 2.70, 0.72, 0.35, 0.010, 1300.0, 14000.0,
      { 2, 2, 1, 2, 2 } },
    /* 第十三轮：阶段 9 侧支 */
    { "Orbital Swarm", "Thousands of orbital habitats act as one industry.",
      9, ECO_EXTREMOPHILE, 9.50, 4.60, 1.25, 0.85, 0.010, 3000.0, 900000.0,
      { 3, 3, 2, 2, 2 } },
};

/* 物种基石：让每条谱系有独立的成长手感，全部为只读加成 */
static const SpeciesKeystone k_keystones[SPECIES_COUNT] = {
    { "Primordial Spark", "The first cell divides faster than anything else.",
      0.0, 0.0, 0.0, 0.0, KEYSTONE_PROTOCELL_GROWTH, 0.0, 0.0, 0.0 },
    { "Light Harvester", "Extra output during low-light periods.",
      0.0, 0.0, 0.0, 0.0, 0.0, KEYSTONE_CYANO_LOW_LIGHT, 0.0, 0.0 },
    { "Vent Specialist", "Extra output during toxic blooms.",
      0.0, 0.0, 0.0, 0.0, 0.0, 0.0, KEYSTONE_CHEMO_TOXIN, 0.0 },
    { "Eukaryotic Core", "A compartmentalised cell lifts ATP output.",
      KEYSTONE_ALGAL_ATP, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },
    { "Predator's Edge", "Fast replication yields more DNA.",
      0.0, 0.0, KEYSTONE_PROTOZOAN_DNA, 0.0, 0.0, 0.0, 0.0, 0.0 },
    { "Colonial Body", "Division of labour raises the carrying capacity.",
      0.0, 0.0, 0.0, KEYSTONE_MULTICELL_CAP, KEYSTONE_MULTICELL_GROWTH, 0.0, 0.0, 0.0 },
    { "Extreme Survivor", "Harsh events hurt this colony less.",
      0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, KEYSTONE_EXTREMOPHILE_DAMAGE_REDUCTION },
    { "Tissue Economy", "Specialised tissues convert food more efficiently.",
      0.0, KEYSTONE_TISSUE_BIOMASS, 0.0, KEYSTONE_TISSUE_CAP, 0.0, 0.0, 0.0, 0.0 },
    { "Symbiotic Loop", "Partners recycle energy and genetic material.",
      KEYSTONE_SYMBIOSIS_ATP, 0.0, KEYSTONE_SYMBIOSIS_DNA, 0.0, 0.0, 0.0, 0.0, 0.0 },
    { "Planetary Mind", "A biosphere-wide network lifts every process.",
      KEYSTONE_BIOSPHERE_PRODUCTION, KEYSTONE_BIOSPHERE_PRODUCTION,
      KEYSTONE_BIOSPHERE_PRODUCTION, KEYSTONE_BIOSPHERE_CAP,
      KEYSTONE_BIOSPHERE_GROWTH, 0.0, 0.0, 0.0 },
    { "Saprotrophic Web", "Detritus is recycled into DNA and biomass.",
      0.0, KEYSTONE_FUNGAL_BIOMASS, KEYSTONE_FUNGAL_DNA, 0.0, 0.0, 0.0, 0.0, 0.0 },
    { "Reef Builder", "A mineral skeleton lifts ATP and carrying capacity.",
      KEYSTONE_CORAL_ATP, 0.0, 0.0, KEYSTONE_CORAL_CAP, 0.0, 0.0, 0.0, 0.0 },
    { "Root Network", "Fungal threads lift DNA output and colony growth.",
      0.0, 0.0, KEYSTONE_MYCORRHIZAL_DNA, 0.0, KEYSTONE_MYCORRHIZAL_GROWTH, 0.0, 0.0, 0.0 },
    { "Reef Skyline", "Layered structure lifts biomass and the population ceiling.",
      0.0, KEYSTONE_REEF_BIOMASS, 0.0, KEYSTONE_REEF_CAP, 0.0, 0.0, 0.0, 0.0 },
    { "Self Model", "A mind that models its own biosphere lifts ATP and growth.",
      KEYSTONE_SAPIENT_ATP, 0.0, 0.0, 0.0, KEYSTONE_SAPIENT_GROWTH, 0.0, 0.0, 0.0 },
    { "Industrial Loop", "Industry lifts every process and the carrying capacity.",
      KEYSTONE_TECHNO_PRODUCTION, KEYSTONE_TECHNO_PRODUCTION, KEYSTONE_TECHNO_PRODUCTION,
      KEYSTONE_TECHNO_CAP, 0.0, 0.0, 0.0, 0.0 },
    { "Urban Engine", "Dense cities lift biomass and the population ceiling.",
      0.0, KEYSTONE_CIV_BIOMASS, 0.0, KEYSTONE_CIV_CAP, 0.0, 0.0, 0.0, 0.0 },
    { "Orbital Economy", "Orbit-side industry lifts ATP and DNA output.",
      KEYSTONE_SPACE_ATP, 0.0, KEYSTONE_SPACE_DNA, 0.0, 0.0, 0.0, 0.0, 0.0 },
    { "Recursive Mind", "Self-improvement accelerates every process.",
      KEYSTONE_SINGULARITY_PRODUCTION, KEYSTONE_SINGULARITY_PRODUCTION,
      KEYSTONE_SINGULARITY_PRODUCTION, KEYSTONE_SINGULARITY_CAP, 0.0, 0.0, 0.0, 0.0 },
    { "Omega Process", "A self-designing process lifts every output and the ceiling.",
      KEYSTONE_TRANSCENDENCE_PRODUCTION, KEYSTONE_TRANSCENDENCE_PRODUCTION,
      KEYSTONE_TRANSCENDENCE_PRODUCTION, KEYSTONE_TRANSCENDENCE_CAP, 0.0, 0.0, 0.0, 0.0 },
    { "Sun Pool", "A shared photosynthetic pool lifts ATP in dim water.",
      KEYSTONE_MAT_ATP, 0.0, 0.0, 0.0, 0.0, KEYSTONE_MAT_LOW_LIGHT, 0.0, 0.0 },
    { "Sulfur Relay", "Chemical relays lift biomass and shrug off toxins.",
      0.0, KEYSTONE_VENT_BIOMASS, 0.0, 0.0, 0.0, 0.0, KEYSTONE_VENT_TOXIN, 0.0 },
    { "Chitin Lattice", "A rigid lattice lifts biomass and the ceiling.",
      0.0, KEYSTONE_CHITIN_BIOMASS, 0.0, KEYSTONE_CHITIN_CAP, 0.0, 0.0, 0.0, 0.0 },
    { "Detritus Engine", "Recycling detritus lifts DNA and biomass.",
      0.0, KEYSTONE_SAPRO_BIOMASS, KEYSTONE_SAPRO_DNA, 0.0, 0.0, 0.0, 0.0, 0.0 },
    { "Caste System", "Specialised castes lift every process and the ceiling.",
      KEYSTONE_HIVE_PRODUCTION, KEYSTONE_HIVE_PRODUCTION, KEYSTONE_HIVE_PRODUCTION,
      KEYSTONE_HIVE_CAP, 0.0, 0.0, 0.0, 0.0 },
    { "Orbital Grid", "A grid of habitats lifts ATP and the ceiling.",
      KEYSTONE_ORBITAL_ATP, 0.0, 0.0, KEYSTONE_ORBITAL_CAP, 0.0, 0.0, 0.0, 0.0 },
};

/* 第九轮：子升级（科技分层）。阶段 >= 6 的物种各有 3 层命名升级，
 * 第十轮：乘数统一为 ×1.35，逐层成本 ×3，使乘算总额停在 ×2.5 量级。 */
static const SubUpgradeDef k_sub_upgrades[] = {
    { "Agriculture", "Farming frees minds to think.",
      SPECIES_SAPIENT_LIFE, 0, 1.35, 300.0 },
    { "Writing", "Ideas outlive their thinkers.",
      SPECIES_SAPIENT_LIFE, 1, 1.35, 300.0 },
    { "Philosophy", "Asking why doubles down on how.",
      SPECIES_SAPIENT_LIFE, 2, 1.35, 300.0 },
    { "Steam Power", "Engines multiply every hand.",
      SPECIES_TECHNOSPHERE, 0, 1.35, 600.0 },
    { "Electricity", "Power flows to every process.",
      SPECIES_TECHNOSPHERE, 1, 1.35, 600.0 },
    { "Computing", "Calculation becomes a utility.",
      SPECIES_TECHNOSPHERE, 2, 1.35, 600.0 },
    { "Printing Press", "Knowledge replicates faster than cells.",
      SPECIES_CIVILIZATION, 0, 1.35, 1200.0 },
    { "Scientific Method", "Discovery becomes a system.",
      SPECIES_CIVILIZATION, 1, 1.35, 1200.0 },
    { "Mass Media", "One signal reaches every mind.",
      SPECIES_CIVILIZATION, 2, 1.35, 1200.0 },
    { "Internet", "Every mind joins one network.",
      SPECIES_SPACE_AGE, 0, 1.35, 2400.0 },
    { "Automation", "Machines run the world machine.",
      SPECIES_SPACE_AGE, 1, 1.35, 2400.0 },
    { "Fusion Power", "Star-fire bottled for industry.",
      SPECIES_SPACE_AGE, 2, 1.35, 2400.0 },
    { "Nanotechnology", "Matter itself becomes a tool.",
      SPECIES_SINGULARITY, 0, 1.35, 4800.0 },
    { "Quantum Computing", "Thought outruns chemistry.",
      SPECIES_SINGULARITY, 1, 1.35, 4800.0 },
    { "Self-Replication", "Growth designs its own acceleration.",
      SPECIES_SINGULARITY, 2, 1.35, 4800.0 },
    { "Stellar Engineering", "Stars become components of the process.",
      SPECIES_TRANSCENDENCE, 0, 1.35, 9600.0 },
    { "Galactic Network", "Every star hosts a node of the same mind.",
      SPECIES_TRANSCENDENCE, 1, 1.35, 9600.0 },
    { "Omega Point", "The process becomes the purpose.",
      SPECIES_TRANSCENDENCE, 2, 1.35, 9600.0 },
    /* 第十三轮：两条阶段 6/9 侧支各有 3 层 */
    { "Division of Labour", "Each caste perfects one task.",
      SPECIES_HIVE_POLITY, 0, 1.35, 300.0 },
    { "Distributed Ledger", "The hive remembers everything at once.",
      SPECIES_HIVE_POLITY, 1, 1.35, 300.0 },
    { "Global Brain", "The polity thinks as a single mind.",
      SPECIES_HIVE_POLITY, 2, 1.35, 300.0 },
    { "Orbital Yards", "Habitats build habitats.",
      SPECIES_ORBITAL_SWARM, 0, 1.35, 2400.0 },
    { "Asteroid Mining", "Every rock becomes feedstock.",
      SPECIES_ORBITAL_SWARM, 1, 1.35, 2400.0 },
    { "Stellar Logistics", "Supplies cross the system on schedule.",
      SPECIES_ORBITAL_SWARM, 2, 1.35, 2400.0 },
};

#define SUB_UPGRADE_COUNT ((int)(sizeof(k_sub_upgrades) / sizeof(k_sub_upgrades[0])))

static const SpeciationRule k_speciation_rules[] = {
    { SPECIES_CYANOBACTERIUM,
      { SPECIES_PROTOCELL, -1, -1 },
      { MUTATION_PHOTOSYNTHESIS, -1 },
      -1, 0 },
    { SPECIES_CHEMOLITHOTROPH,
      { SPECIES_PROTOCELL, -1, -1 },
      { MUTATION_CHEMOSYNTHESIS, -1 },
      -1, 0 },
    { SPECIES_ALGAL_EUKARYOTE,
      { SPECIES_CYANOBACTERIUM, -1, -1 },
      { MUTATION_NUCLEUS, MUTATION_PHOTOSYNTHESIS, -1 },
      -1, 0 },
    { SPECIES_PROTOZOAN,
      { SPECIES_CHEMOLITHOTROPH, -1, -1 },
      { MUTATION_NUCLEUS, MUTATION_MITOCHONDRIA, -1 },
      -1, 0 },
    { SPECIES_MULTICELLULAR_COLONY,
      { SPECIES_ALGAL_EUKARYOTE, SPECIES_PROTOZOAN, -1 },
      { MUTATION_MULTICELLULARITY, -1 },
      -1, 0 },
    { SPECIES_EXTREMOPHILE_COLONY,
      { SPECIES_CHEMOLITHOTROPH, SPECIES_PROTOZOAN, -1 },
      { MUTATION_HEAT_RESISTANCE, MUTATION_TOXIN_RESISTANCE, -1 },
      LEGACY_DIVERSIFICATION, 5 },
    { SPECIES_DIFFERENTIATED_TISSUE,
      { SPECIES_MULTICELLULAR_COLONY, -1, -1 },
      { MUTATION_CELL_DIFFERENTIATION, -1 },
      -1, 0 },
    { SPECIES_SYMBIOTIC_SUPERORGANISM,
      { SPECIES_MULTICELLULAR_COLONY, SPECIES_EXTREMOPHILE_COLONY, SPECIES_FUNGAL_NETWORK },
      { MUTATION_SYMBIOTIC_NITROGEN_FIXATION, -1 },
      -1, 0 },
    { SPECIES_COGNITIVE_BIOSPHERE,
      { SPECIES_DIFFERENTIATED_TISSUE, SPECIES_SYMBIOTIC_SUPERORGANISM, -1 },
      { MUTATION_NEURAL_SIGNALING, MUTATION_QUORUM_SENSING, -1 },
      -1, 0 },
    /* 第四轮侧支：真菌网络与珊瑚共生体，给第三槽提供落点 */
    { SPECIES_FUNGAL_NETWORK,
      { SPECIES_PROTOZOAN, SPECIES_MULTICELLULAR_COLONY, -1 },
      { MUTATION_DECOMPOSER_ENZYMES, -1 },
      -1, 0 },
    { SPECIES_CORAL_SYMBIONT,
      { SPECIES_ALGAL_EUKARYOTE, SPECIES_MULTICELLULAR_COLONY, -1 },
      { MUTATION_PHOTOSYMBIOSIS, -1 },
      -1, 0 },
    /* 第六轮：两条侧支的阶段 5 终点 */
    { SPECIES_MYCORRHIZAL_WEB,
      { SPECIES_FUNGAL_NETWORK, SPECIES_SYMBIOTIC_SUPERORGANISM, -1 },
      { MUTATION_MYCORRHIZAL_SYMBIOSIS, -1 },
      -1, 0 },
    { SPECIES_REEF_METROPOLIS,
      { SPECIES_CORAL_SYMBIONT, SPECIES_ALGAL_EUKARYOTE, -1 },
      { MUTATION_REEF_ARCHITECTURE, -1 },
      -1, 0 },
    /* 第七轮：阶段 6/7 主线 */
    { SPECIES_SAPIENT_LIFE,
      { SPECIES_COGNITIVE_BIOSPHERE, -1, -1 },
      { MUTATION_ABSTRACT_THOUGHT, -1 },
      -1, 0 },
    { SPECIES_TECHNOSPHERE,
      { SPECIES_SAPIENT_LIFE, -1, -1 },
      { MUTATION_PLANETARY_INDUSTRY, -1 },
      -1, 0 },
    /* 第八轮：阶段 8/9/10 长线 */
    { SPECIES_CIVILIZATION,
      { SPECIES_TECHNOSPHERE, -1, -1 },
      { MUTATION_LANGUAGE, -1 },
      -1, 0 },
    { SPECIES_SPACE_AGE,
      { SPECIES_CIVILIZATION, -1, -1 },
      { MUTATION_ROCKETRY, -1 },
      -1, 0 },
    { SPECIES_SINGULARITY,
      { SPECIES_SPACE_AGE, -1, -1 },
      { MUTATION_RECURSIVE_SELF_IMPROVEMENT, -1 },
      -1, 0 },
    /* 第十二轮：阶段 11 */
    { SPECIES_TRANSCENDENCE,
      { SPECIES_SINGULARITY, -1, -1 },
      { MUTATION_OMEGA_POINT, -1 },
      -1, 0 },
    /* 第十三轮：6 条侧支，每条都要求一个原先偏装饰的突变 */
    { SPECIES_PHOTOSYNTHETIC_MAT,
      { SPECIES_ALGAL_EUKARYOTE, -1, -1 },
      { MUTATION_PHOTOSYMBIOSIS, -1 },
      -1, 0 },
    { SPECIES_VENT_SWARM,
      { SPECIES_CHEMOLITHOTROPH, -1, -1 },
      { MUTATION_SULFUR_METABOLISM, -1 },
      -1, 0 },
    { SPECIES_CHITIN_REEF,
      { SPECIES_MULTICELLULAR_COLONY, -1, -1 },
      { MUTATION_CHITIN_WALL, -1 },
      -1, 0 },
    { SPECIES_SAPROTROPHIC_FOREST,
      { SPECIES_FUNGAL_NETWORK, -1, -1 },
      { MUTATION_DECOMPOSER_ENZYMES, MUTATION_MYCORRHIZAL_SYMBIOSIS, -1 },
      -1, 0 },
    { SPECIES_HIVE_POLITY,
      { SPECIES_SAPIENT_LIFE, -1, -1 },
      { MUTATION_QUORUM_SENSING, MUTATION_LANGUAGE, -1 },
      -1, 0 },
    { SPECIES_ORBITAL_SWARM,
      { SPECIES_SPACE_AGE, -1, -1 },
      { MUTATION_ORBITAL_HABITATS, -1 },
      -1, 0 },
    { -1, { -1, -1, -1 }, { -1, -1, -1, -1 }, -1, 0 },
};

static const SpeciationRule *find_rule(int target_species_id)
{
    for (int i = 0; k_speciation_rules[i].target >= 0; ++i) {
        if (k_speciation_rules[i].target == target_species_id) {
            return &k_speciation_rules[i];
        }
    }
    return 0;
}

static bool valid_species(int species_id)
{
    return species_id >= 0 && species_id < SPECIES_COUNT;
}

const SpeciesDef *species_get(int species_id)
{
    return valid_species(species_id) ? &k_species[species_id] : 0;
}

const char *species_name(int species_id)
{
    const SpeciesDef *def = species_get(species_id);
    return def ? def->name : "Unknown";
}

const char *species_description(int species_id)
{
    const SpeciesDef *def = species_get(species_id);
    return def ? def->description : "";
}

int species_phase(int species_id)
{
    const SpeciesDef *def = species_get(species_id);
    return def ? def->phase : 0;
}

EcologyTag species_ecology(int species_id)
{
    const SpeciesDef *def = species_get(species_id);
    return def ? def->ecology : ECO_PRIMITIVE;
}

int species_base_trait(int species_id, EnvironmentTrait trait)
{
    const SpeciesDef *def = species_get(species_id);
    if (!def || trait < 0 || trait >= TRAIT_COUNT) {
        return 0;
    }
    return def->traits[trait];
}

double species_base_speciation_cost(int species_id)
{
    const SpeciesDef *def = species_get(species_id);
    if (!def) {
        return 0.0;
    }
    double cost = def->base_speciation_cost;
    if (species_id >= SPECIES_DIFFERENTIATED_TISSUE) {
        cost *= EXPANSION_SPECIATION_COST_SCALE;
    }
    return cost;
}

const SpeciesKeystone *species_keystone(int species_id)
{
    return valid_species(species_id) ? &k_keystones[species_id] : 0;
}

int species_children(int parent_id, int *out_ids, int max_ids)
{
    int count = 0;
    for (int i = 0; k_speciation_rules[i].target >= 0; ++i) {
        const SpeciationRule *rule = &k_speciation_rules[i];
        for (int p = 0; p < 3 && rule->parents[p] >= 0; ++p) {
            if (rule->parents[p] == parent_id) {
                if (out_ids && count < max_ids) {
                    out_ids[count] = rule->target;
                }
                ++count;
                break;
            }
        }
    }
    return count;
}

bool species_has_parent(int target_species_id, int parent_id)
{
    const SpeciationRule *rule = find_rule(target_species_id);
    if (!rule) {
        return false;
    }
    for (int p = 0; p < 3 && rule->parents[p] >= 0; ++p) {
        if (rule->parents[p] == parent_id) {
            return true;
        }
    }
    return false;
}

int species_required_mutations(int target_species_id, int *out_ids, int max_ids)
{
    const SpeciationRule *rule = find_rule(target_species_id);
    int count = 0;
    if (!rule) {
        return 0;
    }
    for (int i = 0; i < 4 && rule->required[i] >= 0; ++i) {
        if (out_ids && count < max_ids) {
            out_ids[count] = rule->required[i];
        }
        ++count;
    }
    return count;
}

bool species_requires_legacy(int target_species_id, int *branch, int *level)
{
    const SpeciationRule *rule = find_rule(target_species_id);
    if (!rule || rule->req_branch < 0) {
        return false;
    }
    if (branch) {
        *branch = rule->req_branch;
    }
    if (level) {
        *level = rule->req_level;
    }
    return true;
}

int species_min_child_cost(int parent_id)
{
    int ids[SPECIES_COUNT];
    int count = species_children(parent_id, ids, SPECIES_COUNT);
    double best = -1.0;
    for (int i = 0; i < count; ++i) {
        double cost = species_base_speciation_cost(ids[i]);
        if (best < 0.0 || cost < best) {
            best = cost;
        }
    }
    return best < 0.0 ? 0 : (int)best;
}

/* ---------------- 第九轮：子升级数据访问 ---------------- */

int evolution_sub_upgrade_count(void)
{
    return SUB_UPGRADE_COUNT;
}

const SubUpgradeDef *evolution_sub_upgrade_get(int index)
{
    return index >= 0 && index < SUB_UPGRADE_COUNT ? &k_sub_upgrades[index] : 0;
}
