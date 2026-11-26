"""Generate assets/data.js for the Tree of Life Member B report from demo CSV."""

import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
CSV_PATH = os.path.join(HERE, "demo_run.csv")
OUT_PATH = os.path.join(HERE, "..", "..", "tree-of-life-b", "assets", "data.js")

rows = {"1": [], "2": []}
events = []
matrix = []
crisis = []
mut_impact = []
batch = []
bmut = []
avail = []
cand_counts = {}
cand_avg = {}
tree_edges = []
mtraits = []
dual = []

MUTATION_NAMES = [
    "Cell Membrane", "Photosynthesis", "Chemosynthesis", "Cell Wall",
    "Heat Resistance", "Cold Resistance", "Nucleus", "Mitochondria",
    "Multicellularity", "Toxin Resistance", "Efficient Respiration",
    "Adaptive Hypermutation",
]

with open(CSV_PATH, "r", encoding="utf-8") as handle:
    for line in handle:
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(",")
        if parts[0] == "ROW":
            (_, era, t, stage, species, pop, cap, atp, bio, dna, biomass, atp_bal, env,
             metab, repro, genome, disc_mut, disc_spe) = parts
            rows[era].append({
                "t": float(t),
                "stage": int(stage),
                "species": species,
                "pop": float(pop),
                "cap": float(cap),
                "atpRate": float(atp),
                "bioRate": float(bio),
                "dnaRate": float(dna),
                "biomass": float(biomass),
                "atp": float(atp_bal),
                "env": env,
                "metab": int(metab),
                "repro": int(repro),
                "genome": int(genome),
                "discMut": int(disc_mut),
                "discSpe": int(disc_spe),
            })
        elif parts[0] == "EVENT":
            _, era, t, kind, detail = parts
            events.append({"era": int(era), "t": float(t), "kind": kind, "detail": detail})
        elif parts[0] == "MATRIX":
            _, species_id, env_id, value = parts
            matrix.append({"s": int(species_id), "e": int(env_id), "m": float(value)})
        elif parts[0] == "CRISIS":
            _, era, env_id, start, end, threshold, margin, passed = parts
            crisis.append({
                "era": int(era),
                "env": int(env_id),
                "start": float(start),
                "end": float(end),
                "threshold": float(threshold),
                "margin": float(margin),
                "passed": passed == "1",
            })
        elif parts[0] == "MUTIMPACT":
            _, mid, atp, bio, dna, cap, growth, cost = parts
            mut_impact.append({
                "id": int(mid),
                "atp": float(atp),
                "bio": float(bio),
                "dna": float(dna),
                "cap": float(cap),
                "growth": float(growth),
                "cost": float(cost),
            })
        elif parts[0] == "BATCH":
            (_, index, seed, ep, stages, mutations, species, crises, max_biomass,
             first_speciation, first_species) = parts
            batch.append({
                "i": int(index),
                "seed": int(seed),
                "ep": int(ep),
                "stages": int(stages),
                "mutations": int(mutations),
                "species": int(species),
                "crises": int(crises),
                "maxBiomass": float(max_biomass),
                "firstSpe": float(first_speciation),
                "firstSpecies": int(first_species),
            })
        elif parts[0] == "BMUT":
            _, index, mid, when = parts
            bmut.append({"i": int(index), "id": int(mid), "t": float(when)})
        elif parts[0] == "AVAIL":
            _, species_id, mutation_id, available = parts
            avail.append({"s": int(species_id), "m": int(mutation_id), "a": int(available)})
        elif parts[0] == "CAND":
            _, level, mid, count = parts
            cand_counts.setdefault(int(level), [0] * len(MUTATION_NAMES))[int(mid)] = int(count)
        elif parts[0] == "CANDAVG":
            _, level, average = parts
            cand_avg[int(level)] = float(average)
        elif parts[0] == "TREE":
            _, parent, child = parts
            tree_edges.append({"p": int(parent), "c": int(child)})
        elif parts[0] == "MTRAIT":
            _, mid, heat, cold, light, oxygen, toxin = parts
            mtraits.append({
                "id": int(mid),
                "v": [int(heat), int(cold), int(light), int(oxygen), int(toxin)],
            })
        elif parts[0] == "DROW":
            (_, t, pop_main, pop_extra, atp_main, atp_extra, atp_total, bio_total,
             synergy) = parts
            dual.append({
                "t": float(t),
                "popMain": float(pop_main),
                "popExtra": float(pop_extra),
                "atpMain": float(atp_main),
                "atpExtra": float(atp_extra),
                "atpTotal": float(atp_total),
                "bioTotal": float(bio_total),
                "synergy": synergy == "1",
            })


def era_events(era):
    return [e for e in events if e["era"] == era]


def build_windows(era):
    windows = []
    open_event = None
    for event in era_events(era):
        if event["kind"] == "env_start":
            name, duration, multiplier, instant = event["detail"].split(";")
            open_event = {
                "name": name,
                "start": event["t"],
                "duration": float(duration),
                "multiplier": float(multiplier),
                "instant": float(instant),
            }
        elif event["kind"] == "env_end" and open_event is not None:
            name, survived = event["detail"].split(";")
            open_event["end"] = event["t"]
            open_event["survived"] = survived == "1"
            windows.append(open_event)
            open_event = None
    if open_event is not None:
        open_event["end"] = rows[str(era)][-1]["t"]
        open_event["survived"] = None
        windows.append(open_event)
    return windows


def pick(era, kind):
    return [e for e in era_events(era) if e["kind"] == kind]


def parse_mutations(era):
    result = []
    for event in pick(era, "mutation"):
        name, cost = event["detail"].split(";")
        result.append({"t": event["t"], "name": name, "cost": float(cost)})
    return result


def parse_speciations(era):
    result = []
    for event in pick(era, "speciation"):
        path, cost, old_pop, new_pop = event["detail"].split(";")
        parent, child = path.split(">")
        result.append({
            "t": event["t"],
            "from": parent,
            "to": child,
            "cost": float(cost),
            "oldPop": float(old_pop),
            "newPop": float(new_pop),
        })
    return result


def parse_era_end(era):
    for event in pick(era, "era_end"):
        ep, stages, mutations, species, crises, milestone, max_biomass = event["detail"].split(";")
        return {
            "ep": int(ep),
            "stages": int(stages),
            "mutations": int(mutations),
            "species": int(species),
            "crises": int(crises),
            "milestone": int(milestone),
            "maxBiomass": float(max_biomass),
        }
    return None


def parse_legacy(era):
    result = []
    for event in pick(era, "legacy"):
        branch, level, cost = event["detail"].split(";")
        result.append({"branch": int(branch), "level": int(level), "cost": int(cost)})
    return result


def rate_at(era, t):
    for row in rows[str(era)]:
        if abs(row["t"] - t) < 0.01:
            return row
    return None


def cumulative_dna(mutations):
    total = 0.0
    result = []
    for item in mutations:
        total += item["cost"]
        result.append({"t": item["t"], "name": item["name"], "cost": item["cost"], "total": total})
    return result


last = rows["1"][-1]

mutation_freq = [0] * len(MUTATION_NAMES)
for item in bmut:
    if 0 <= item["id"] < len(mutation_freq):
        mutation_freq[item["id"]] += 1

mutation_times = {}
for item in bmut:
    mutation_times.setdefault(item["id"], []).append(item["t"])

data = {
    "era1": rows["1"],
    "era2": rows["2"],
    "windows1": build_windows(1),
    "windows2": build_windows(2),
    "mutations1": parse_mutations(1),
    "mutations2": parse_mutations(2),
    "dnaSpend": cumulative_dna(parse_mutations(1)),
    "speciations1": parse_speciations(1),
    "ep": parse_era_end(1),
    "legacy": parse_legacy(1),
    "matrix": matrix,
    "crisis": [c for c in crisis if c["era"] == 1],
    "mutImpact": mut_impact,
    "mutationNames": MUTATION_NAMES,
    "mutationFreq": mutation_freq,
    "mutationTimes": [{"id": mid, "times": times} for mid, times in sorted(mutation_times.items())],
    "batch": batch,
    "avail": avail,
    "candidates": [
        {"level": level, "avg": cand_avg[level], "counts": cand_counts[level]}
        for level in sorted(cand_counts)
    ],
    "tree": tree_edges,
    "mtraits": mtraits,
    "dual": dual,
    "codex": {
        "species": last["discSpe"],
        "speciesTotal": 6,
        "mutations": last["discMut"],
        "mutationsTotal": 12,
    },
    "rate60": {
        "era1": rate_at(1, 60),
        "era2": rate_at(2, 60),
    },
}

os.makedirs(os.path.dirname(OUT_PATH), exist_ok=True)
with open(OUT_PATH, "w", encoding="utf-8") as handle:
    handle.write("window.DEMO = ")
    json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
    handle.write(";\n")

print("era1 rows:", len(data["era1"]), "era2 rows:", len(data["era2"]), "matrix:", len(matrix))
print("batch runs:", len(batch), "crises:", len(data["crisis"]), "mutimpact:", len(mut_impact))
print("avail:", len(avail), "tree edges:", len(tree_edges), "dual rows:", len(dual))
print("cand avg:", cand_avg)
print("cand level 0 counts:", cand_counts.get(0))
print("EP:", data["ep"])
print("batch EP range:", min(b["ep"] for b in batch), "-", max(b["ep"] for b in batch))
print("batch first speciation:", min(b["firstSpe"] for b in batch), "-", max(b["firstSpe"] for b in batch))
print("mutation freq:", list(zip(MUTATION_NAMES, mutation_freq)))
