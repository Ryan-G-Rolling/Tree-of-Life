"""Build assets/expansion.js from demo_exp.csv (playability expansion export)."""

import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
CSV_PATH = os.path.join(HERE, "demo_exp.csv")
OUT_PATH = os.path.join(HERE, "..", "..", "tree-of-life-b", "assets", "expansion.js")

synergies = {}
synergy_names = {}
keystones = {}
keystone_names = {}
codex_m = []
codex_s = []
resilience = []

with open(CSV_PATH, "r", encoding="utf-8") as handle:
    for line in handle:
        parts = line.strip().split(",")
        kind = parts[0]
        if kind == "SYN":
            index = int(parts[1])
            synergies[index] = {
                "i": index,
                "a": int(parts[2]),
                "b": int(parts[3]),
                "atp": float(parts[4]),
                "bio": float(parts[5]),
                "dna": float(parts[6]),
                "cap": float(parts[7]),
                "growth": float(parts[8]),
            }
        elif kind == "SYNNAME":
            synergy_names[int(parts[1])] = parts[2]
        elif kind == "KEY":
            keystones[int(parts[1])] = {
                "id": int(parts[1]),
                "atp": float(parts[2]),
                "bio": float(parts[3]),
                "dna": float(parts[4]),
                "cap": float(parts[5]),
                "growth": float(parts[6]),
                "lowlight": float(parts[7]),
                "toxin": float(parts[8]),
                "crisis": float(parts[9]),
            }
        elif kind == "KEYNAME":
            keystone_names[int(parts[1])] = parts[2]
        elif kind == "CODEXM":
            codex_m.append({
                "n": int(parts[1]),
                "atp": float(parts[2]),
                "bio": float(parts[3]),
                "dna": float(parts[4]),
            })
        elif kind == "CODEXS":
            codex_s.append({"n": int(parts[1]), "bio": float(parts[2])})
        elif kind == "RESIL":
            resilience.append({"n": int(parts[1]), "mult": float(parts[2])})

data = {
    "synergies": [dict(item, name=synergy_names.get(index, ""))
                  for index, item in sorted(synergies.items())],
    "keystones": [dict(item, name=keystone_names.get(index, ""))
                  for index, item in sorted(keystones.items())],
    "codexM": codex_m,
    "codexS": codex_s,
    "resilience": resilience,
}

os.makedirs(os.path.dirname(OUT_PATH), exist_ok=True)
with open(OUT_PATH, "w", encoding="utf-8") as handle:
    handle.write("window.EXP = ")
    json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
    handle.write(";\n")

print("synergies:", len(data["synergies"]))
print("keystones:", len(data["keystones"]))
print("codexM:", len(codex_m), "codexS:", len(codex_s), "resilience:", len(resilience))
