"""第十五轮：14 个阶段各 +10 物种与 +10 突变（物种、突变各 +140）。

脚本只做一次性数据落地：把生成的行插入 game_types.h / species.c / evolution.c，
并把同样的内容追加到 docs/02 的物种、分化与突变表。重复运行会因锚点缺失而失败，
避免重复插入。生成后的表与手写内容格式一致，运行期不依赖本脚本。
"""

import os
import re
import stat

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
GS = os.path.join(ROOT, "include", "game_types.h")
SP = os.path.join(ROOT, "src", "species.c")
EV = os.path.join(ROOT, "src", "evolution.c")
DOC = os.path.join(ROOT, "..", "docs", "02-数据与计算规则.md")

# ---- 每阶段 10 个物种名 ----
SPECIES_NAMES = {
    0: ["Lipid Vesicle", "RNA Droplet", "Coacervate", "Iron-Sulfur Bead", "Porphyrin Bubble",
        "Clay Scaffold Cell", "Thermal Microdroplet", "Vesicle Colony", "Nucleobase Film",
        "Early Replicator"],
    1: ["Purple Sulfur Bacterium", "Green Sulfur Bacterium", "Methanogen", "Halophile",
        "Thermoacidophile", "Iron Oxidizer", "Hydrogenotroph", "Anoxygenic Phototroph",
        "Nitrifying Bacterium", "Sulfate Reducer"],
    2: ["Diatom", "Dinoflagellate", "Ciliate", "Amoeba", "Euglenid", "Foraminifer",
        "Radiolarian Protozoan", "Green Alga", "Red Alga", "Choanoflagellate"],
    3: ["Sponge Primitive", "Hydra Polyp", "Volvox Sphere", "Slime Mold Aggregate",
        "Biofilm Tower", "Stromatolite Builder", "Colonial Flagellate", "Placozoan",
        "Trichoplax", "Early Metazoan"],
    4: ["Flatworm", "Jellyfish", "Coral Polyp", "Sea Anemone", "Comb Jelly", "Sponge Reef",
        "Bryozoan", "Brachiopod", "Segmented Worm", "Early Mollusc"],
    5: ["Trilobite", "Sea Scorpion", "Jawless Fish", "Ammonite", "Crinoid", "Colonial Coral",
        "Giant Clam", "Nautilus", "Horseshoe Crab", "Lamprey"],
    6: ["Lobe-Finned Fish", "Amphibian", "Early Reptile", "Dimetrodon", "Dragonfly Giant",
        "Carboniferous Forest", "Seed Fern", "Cycad", "Tree Fern", "Swamp Conifer"],
    7: ["Dinosaur Herd", "Pterosaur", "Ichthyosaur", "Plesiosaur", "Feathered Raptor",
        "Early Mammal", "Marsupial", "Monotreme", "Flowering Plant", "Grassland"],
    8: ["Primate Troop", "Early Hominin", "Megalith Culture", "River Civilization",
        "Bronze Empire", "Writing City", "Sea Trade League", "Mounted Nomad",
        "Mountain Kingdom", "Island Chiefdom"],
    9: ["Industrial Nation", "Rail Network", "Steam Metropolis", "Telegraph Empire",
        "Colonial Fleet", "Oil Economy", "Electric Grid", "Airship Age", "Radio Culture",
        "Mass Society"],
    10: ["Orbital Station", "Lunar Colony", "Mars Settlement", "Asteroid Miner",
         "Fusion Economy", "Space Elevator", "Solar Sail Fleet", "Generation Ship",
         "Terraforming Project", "Interplanetary Union"],
    11: ["Neural Archive", "Mind Upload", "Machine Congress", "Dyson Ring", "Star Lifter",
         "Matrioshka Brain", "Galactic Library", "Stellar Engine", "Black Hole Farm",
         "Quantum Network"],
    12: ["Simulation World", "Dream Engine", "Memory Ocean", "Concept Mesh", "Idea Garden",
         "Noetic City", "Thought Reef", "Logic Forest", "Paradigm Swarm", "Cognition Tide"],
    13: ["Cosmic Loom", "Entropy Garden", "Multiverse Bridge", "Omega Archive", "First Cause",
         "Final Observer", "Time Weave", "Law Forge", "Void Bloom", "Absolute Mind"],
}

# ---- 每阶段 10 个突变名 ----
MUTATION_NAMES = {
    0: ["Lipid Repair", "RNA Proofreading", "Mineral Catalysis", "Proton Gradient",
        "Vesicle Fission", "Surface Adhesion", "Osmotic Control", "UV Screening",
        "Thermal Buffering", "Prebiotic Cycling"],
    1: ["Bacteriorhodopsin", "Sulfur Oxidation", "Methanogenesis", "Salt Pump", "Acid Shield",
        "Iron Chelation", "Hydrogenase", "Photosystem I", "Nitrite Oxidase", "Sulfate Reduction"],
    2: ["Silica Frustule", "Cellulose Plate", "Ciliary Beat", "Pseudopod Flow",
        "Chloroplast Endosymbiosis", "Calcium Shell", "Vacuole Storage", "Mixotrophy",
        "Sexual Cycle", "Colonial Glue"],
    3: ["Collagen Matrix", "Nematocyst", "Body Axis", "Gastrulation", "Spicule Skeleton",
        "Canal System", "Basement Membrane", "Stem Cell Pool", "Regeneration",
        "Colonial Division"],
    4: ["Bilateral Symmetry", "Coelom", "Circulatory Tube", "Nerve Cord", "Gill Slit",
        "Shell Gland", "Segmentation", "Exoskeleton", "Jointed Limb", "Filter Feeding"],
    5: ["Vertebra Column", "Jaw", "Swim Bladder", "Lateral Line", "Chitin Armor",
        "Coral Symbiosis", "Bivalve Siphon", "Cephalopod Jet", "Compound Eye",
        "Cambrian Predation"],
    6: ["Lung", "Tetrapod Limb", "Amniotic Egg", "Keratin Scale", "Wing Membrane",
        "Seed Coat", "Vascular Cambium", "Stomata Control", "Herbivore Gut", "Parental Care"],
    7: ["Endothermy", "Feather Insulation", "Mammary Gland", "Live Birth", "Flower Symbiosis",
        "Grass Teeth", "Migratory Navigation", "Social Roost", "Burrowing", "Cursorial Leg"],
    8: ["Agriculture", "Pottery", "Metallurgy", "Writing System", "Calendar", "Currency",
        "Road Network", "Siege Engineering", "Oral Epic", "Law Code"],
    9: ["Steam Engine", "Railway", "Telegraph", "Electric Motor", "Vaccination", "Fertilizer",
        "Assembly Line", "Radio", "Aviation", "Mass Printing"],
    10: ["Ion Drive", "Closed Life Support", "Asteroid Refining", "Fusion Reactor",
         "Space Elevator Cable", "Solar Sail", "Cryo Sleep", "Terraforming Bacteria",
         "Orbital Ring", "Interstellar Beacon"],
    11: ["Neural Interface", "Whole-Brain Emulation", "Machine Ethics", "Stellar Lifter",
         "Matrioshka Shell", "Galactic Relay", "Stellar Engine", "Black Hole Generator",
         "Quantum Entanglement Net", "Mind Backup"],
    12: ["Simulation Ethics", "Dream Compression", "Memory Crystal", "Concept Algebra",
         "Idea Pollination", "Noetic Architecture", "Thought Ecology", "Logic Grafting",
         "Paradigm Shift", "Cognition Field"],
    13: ["Entropy Accounting", "Multiverse Coupling", "Omega Recursion", "First-Cause Access",
         "Observer Collapse", "Time Braiding", "Law Rewriting", "Void Gardening",
         "Absolute Logic", "Final Integration"],
}

ECOLOGIES = ["ECO_PRIMITIVE", "ECO_PHOTOSYNTHETIC", "ECO_CHEMOSYNTHETIC",
             "ECO_HETEROTROPHIC", "ECO_MULTICELLULAR", "ECO_EXTREMOPHILE"]
ECO_LABEL = {"ECO_PRIMITIVE": "Primitive", "ECO_PHOTOSYNTHETIC": "Photosynthetic",
             "ECO_CHEMOSYNTHETIC": "Chemosynthetic", "ECO_HETEROTROPHIC": "Heterotrophic",
             "ECO_MULTICELLULAR": "Multicellular", "ECO_EXTREMOPHILE": "Extremophile"}
TRAIT_NAMES = ["Heat", "Cold", "Low Light", "Oxygen", "Toxin"]
TRAIT_PATTERNS = [
    [2, 0, 0, 0, 1], [0, 2, 0, 0, 1], [0, 0, 2, 0, 1], [0, 0, 0, 2, 1], [1, 1, 0, 0, 1],
    [1, 0, 1, 0, 0], [0, 1, 0, 1, 0], [0, 0, 1, 1, 0], [2, 0, 0, 0, 0], [0, 0, 0, 0, 2],
]
ANCHORS = {
    0: ("SPECIES_PROTOCELL", "Protocell"),
    1: ("SPECIES_PROTOCELL", "Protocell"),
    2: ("SPECIES_CYANOBACTERIUM", "Cyanobacterium"),
    3: ("SPECIES_ALGAL_EUKARYOTE", "Algal Eukaryote"),
    4: ("SPECIES_MULTICELLULAR_COLONY", "Multicellular Colony"),
    5: ("SPECIES_DIFFERENTIATED_TISSUE", "Differentiated Tissue"),
    6: ("SPECIES_COGNITIVE_BIOSPHERE", "Cognitive Biosphere"),
    7: ("SPECIES_SAPIENT_LIFE", "Sapient Life"),
    8: ("SPECIES_TECHNOSPHERE", "Technosphere"),
    9: ("SPECIES_CIVILIZATION", "Civilization"),
    10: ("SPECIES_SPACE_AGE", "Space Age"),
    11: ("SPECIES_SINGULARITY", "Singularity"),
    12: ("SPECIES_TRANSCENDENCE", "Transcendence"),
    13: ("SPECIES_NOETIC_FIELD", "Noetic Field"),
}

SPECIES_ATP = [0.6, 1.1, 1.8, 2.6, 3.2, 4.0, 5.0, 5.8, 7.2, 8.8, 11.5, 17.0, 26.0, 40.0]
SPECIES_BIO = [0.2, 0.4, 0.65, 1.05, 1.35, 1.8, 2.5, 2.9, 3.6, 4.5, 5.8, 8.5, 13.0, 20.0]
SPECIES_DNA = [0.06, 0.09, 0.15, 0.24, 0.32, 0.45, 0.68, 0.82, 0.98, 1.18, 1.55, 2.3, 3.5, 5.4]
SPECIES_KNOW = [0, 0, 0, 0, 0, 0, 0.28, 0.38, 0.52, 0.70, 0.92, 1.5, 2.2, 3.2]
SPECIES_CAP = [60, 140, 240, 420, 570, 800, 1150, 1450, 1900, 2700, 3900, 5800, 9000, 13500]
SPECIES_COST = [40, 70, 300, 1100, 2800, 6500, 12000, 45000, 300000, 1000000, 3000000,
                4000000, 6500000, 9500000]
MUTATION_COST = [15, 25, 40, 70, 120, 200, 320, 500, 800, 1400, 2400, 4000, 6500, 10000]
ROLES = ["light capture", "chemical energy", "detritus recycling", "armour", "speed",
         "symbiosis", "colony coordination", "storage", "defence", "reproduction"]


def ident(prefix, name):
    return prefix + re.sub(r"[^A-Za-z0-9]+", "_", name).upper().strip("_")


def fmt(value, digits=2):
    return f"{value:.{digits}f}"


def build_species_rows():
    rows, enum_lines, doc_rows, rules, keystones, doc_rules = [], [], [], [], [], []
    species_ids = {}
    next_id = 46
    for stage in range(14):
        for i, name in enumerate(SPECIES_NAMES[stage]):
            sid = next_id
            next_id += 1
            sid_name = ident("SPECIES_", name)
            if sid_name in species_ids:
                raise SystemExit(f"物种标识重复：{sid_name}")
            species_ids[sid_name] = sid
            scale = 0.88 + 0.024 * i
            atp = SPECIES_ATP[stage] * scale
            bio = SPECIES_BIO[stage] * scale
            dna = SPECIES_DNA[stage] * scale
            know = SPECIES_KNOW[stage] * scale
            cap = SPECIES_CAP[stage] * scale
            cost = SPECIES_COST[stage] * (0.90 + 0.03 * i)
            eco = ECOLOGIES[(stage + i) % len(ECOLOGIES)]
            traits = TRAIT_PATTERNS[i]
            desc = f"A stage-{stage} {name} lineage specialising in {ROLES[i]}."
            rows.append(
                f"    /* 第十五轮：阶段 {stage} · 第 {i + 1} 支 */\n"
                f"    {{ \"{name}\", \"{desc}\",\n"
                f"      {stage}, {eco}, {fmt(atp)}, {fmt(bio)}, {fmt(dna)}, {fmt(know)}, "
                f"0.010, {cap:.0f}, {cost:.0f},\n"
                f"      {{ {traits[0]}, {traits[1]}, {traits[2]}, {traits[3]}, {traits[4]} }} }},")
            prod = 0.04 + 0.006 * stage + 0.002 * i
            cap_b = 0.04 + 0.008 * stage + 0.002 * i
            growth_b = 0.01 + 0.002 * stage
            ll = 0.05 + 0.005 * stage if i % 3 == 0 else 0.0
            tox = 0.05 + 0.005 * stage if i % 3 == 1 else 0.0
            dmg = 0.05 + 0.005 * stage if i % 3 == 2 else 0.0
            ka = prod if i % 5 == 0 else 0.0
            kb = prod if i % 5 == 1 else 0.0
            kd = prod if i % 5 == 2 else 0.0
            kc = cap_b if i % 5 == 3 else 0.0
            kg = growth_b if i % 5 == 4 else 0.0
            keystones.append(
                f"    {{ \"{name} Lineage\", \"A stage-{stage} lineage with a distinct survival trick.\",\n"
                f"      {fmt(ka, 3)}, {fmt(kb, 3)}, {fmt(kd, 3)}, {fmt(kc, 3)}, {fmt(kg, 3)}, "
                f"{fmt(ll, 3)}, {fmt(tox, 3)}, {fmt(dmg, 3)} }},")
            enum_lines.append(f"    {sid_name},")
            eco_label = ECO_LABEL[eco]
            doc_rows.append(
                f"| {sid} | {name} | {stage} | {eco_label} | {fmt(atp)} | {fmt(bio)} | "
                f"{fmt(dna)} | {'—' if know == 0 else fmt(know)} | 0.010 | {cap:.0f} | "
                f"{cost:.0f} Biomass |")
            ticket_stage = max(0, stage - 1)
            ticket = ident("MUTATION_", MUTATION_NAMES[ticket_stage][i])
            anchor, anchor_label = ANCHORS[stage]
            rules.append(
                f"    {{ {sid_name},\n"
                f"      {{ {anchor}, -1, -1 }},\n"
                f"      {{ {ticket}, -1 }},\n"
                f"      -1, 0 }},")
            doc_rules.append(
                f"| {name} | {anchor_label} | {MUTATION_NAMES[ticket_stage][i]} | 无 |")
    return rows, enum_lines, doc_rows, rules, keystones, doc_rules


def build_mutation_rows():
    rows, enum_lines, doc_rows = [], [], []
    next_id = 64
    for stage in range(14):
        for i, name in enumerate(MUTATION_NAMES[stage]):
            mid = next_id
            next_id += 1
            mid_name = ident("MUTATION_", name)
            atp = 0.04 + 0.015 * stage + 0.008 * (i % 4)
            bio = 0.05 + 0.012 * stage + 0.006 * ((i + 1) % 4)
            dna = 0.03 + 0.010 * stage + 0.006 * ((i + 2) % 4)
            cap = 0.05 + 0.015 * stage + 0.010 * ((i + 3) % 3)
            growth = 0.01 + 0.002 * stage + 0.003 * (i % 3)
            traits = TRAIT_PATTERNS[i]
            cost = MUTATION_COST[stage] * (0.90 + 0.02 * i)
            rarity = 0 if i < 5 else (1 if i < 8 else 2)
            desc = f"A stage-{stage} adaptation that improves {ROLES[i]}."
            rows.append(
                f"    /* 第十五轮：阶段 {stage} · 第 {i + 1} 支 */\n"
                f"    {{ \"{name}\", \"{desc}\",\n"
                f"      {cost:.1f}, {fmt(atp)}, {fmt(bio)}, {fmt(dna)}, {fmt(cap)}, "
                f"{fmt(growth)}, {{ {traits[0]}, {traits[1]}, {traits[2]}, {traits[3]}, "
                f"{traits[4]} }},\n"
                f"      {rarity}, {stage}, -1, -1, -1, -1 }},")
            enum_lines.append(f"    {mid_name},")
            effects = []
            if atp >= 0.005:
                effects.append(f"ATP +{round(atp * 100)}%")
            if bio >= 0.005:
                effects.append(f"Biomass +{round(bio * 100)}%")
            if dna >= 0.005:
                effects.append(f"DNA +{round(dna * 100)}%")
            if cap >= 0.005:
                effects.append(f"Cap +{round(cap * 100)}%")
            if growth >= 0.005:
                effects.append(f"增长 +{round(growth * 100)}%")
            trait_text = "、".join(f"{TRAIT_NAMES[t]} +{traits[t]}" for t in range(5) if traits[t])
            rarity_text = ["普通", "稀有", "特殊"][rarity]
            doc_rows.append(
                f"| {mid} | {name} | {cost:.0f} | {'、'.join(effects)} | "
                f"{trait_text if trait_text else '—'} | {rarity_text}，阶段 {stage} |")
    return rows, enum_lines, doc_rows


def insert_before(path, anchor, text, label):
    content = read_text(path)
    if content.count(anchor) != 1:
        raise SystemExit(f"{label} 锚点数量异常：{content.count(anchor)}")
    content = content.replace(anchor, text + "\n" + anchor)
    write_text(path, content)


def read_text(path):
    with open(path, encoding="utf-8") as handle:
        return handle.read()


def write_text(path, text):
    os.chmod(path, stat.S_IWRITE)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)


def main():
    srows, senum, sdoc, srules, skeys, sdoc_rules = build_species_rows()
    mrows, menum, mdoc = build_mutation_rows()

    insert_before(GS, "    SPECIES_COUNT", "\n".join(senum), "物种枚举")
    insert_before(GS, "    MUTATION_COUNT", "\n".join(menum), "突变枚举")
    insert_before(SP, "};\n\n/* 物种基石", "\n".join(srows), "物种表")
    insert_before(SP, "};\n\n/* 第九轮：子升级", "\n".join(skeys), "基石表")
    insert_before(SP, "    { -1, { -1, -1, -1 }, { -1, -1, -1, -1 }, -1, 0 },",
                  "\n".join(srules), "分化规则")
    insert_before(EV, "};\n\n/* 突变共振", "\n".join(mrows), "突变表")

    doc = read_text(DOC)
    markers = [
        ("| 45 | 宇宙织锦 |", "\n".join(sdoc)),
        ("| 宇宙织锦 | 心智场 | Cosmic Awareness、Entropy Reversal | 无 |",
         "\n".join(sdoc_rules)),
        ("| 63 | Entropy Reversal |", "\n".join(mdoc)),
    ]
    for anchor, text in markers:
        if doc.count(anchor) != 1:
            raise SystemExit(f"文档锚点异常：{anchor} × {doc.count(anchor)}")
        doc = doc.replace(anchor, anchor + "\n" + text, 1)
    doc = doc.replace(
        "当前内容（阶段 13、64 个突变、45 个非初始物种）的理论上限为\n"
        "`2×13 + 21 + 22 + 6 + 10 = 85`。",
        "当前内容（阶段 13、204 个突变、185 个非初始物种）的理论上限为\n"
        "`2×13 + 68 + 92 + 6 + 10 = 202`。")
    write_text(DOC, doc)

    print(f"物种 +{len(srows)}（到 {46 + len(srows) - 1}），文档物种行 {len(sdoc)}")
    print(f"突变 +{len(mrows)}（到 {64 + len(mrows) - 1}），文档突变行 {len(mdoc)}")
    print(f"分化规则 +{len(srules)}，基石 +{len(skeys)}")


if __name__ == "__main__":
    main()
