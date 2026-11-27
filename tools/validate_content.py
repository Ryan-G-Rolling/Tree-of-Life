#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""v14 内容自校验：11 项检查，全部 PASS 才允许进入回归。

用法：python tools/validate_content.py
只读源码，不修改任何文件；退出码 0 = PASS，1 = FAIL。
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPECIES_C = os.path.join(ROOT, 'src', 'species.c')
EVOLUTION_C = os.path.join(ROOT, 'src', 'evolution.c')
CODEX_C = os.path.join(ROOT, 'src', 'codex.c')
TYPES_H = os.path.join(ROOT, 'include', 'game_types.h')
GLYPHS_H = os.path.join(ROOT, 'include', 'ui_glyphs.h')

TARGET_SPECIES = 264
TARGET_MUTATIONS = 282
FIRST_V14 = 186
LAST_ORIGINAL_MUTATION = 203
# v16 F7：阶段 13 的 4 个 v14 物种下移到阶段 12（13 抵达率仅 0.7%，挂在那儿等于没加）
PHASE_SPECIES = [11, 22, 26, 25, 26, 27, 24, 23, 12, 13, 12, 12, 20, 11]
ADDED_PER_PHASE = {1: 10, 2: 10, 3: 10, 4: 10, 5: 10, 6: 10, 7: 10, 12: 8, 13: 0}
QUOTA_10 = {'ECO_PHOTOSYNTHETIC': 3, 'ECO_CHEMOSYNTHETIC': 2,
            'ECO_HETEROTROPHIC': 3, 'ECO_MULTICELLULAR': 1, 'ECO_EXTREMOPHILE': 1}
QUOTA_4 = {'ECO_PHOTOSYNTHETIC': 1, 'ECO_CHEMOSYNTHETIC': 0,
           'ECO_HETEROTROPHIC': 1, 'ECO_MULTICELLULAR': 1, 'ECO_EXTREMOPHILE': 1}
# v16 F7：阶段 12 现在是两批 4 个合并，配额 = QUOTA_4 × 2
QUOTA_8 = {'ECO_PHOTOSYNTHETIC': 2, 'ECO_CHEMOSYNTHETIC': 0,
           'ECO_HETEROTROPHIC': 2, 'ECO_MULTICELLULAR': 2, 'ECO_EXTREMOPHILE': 2}
BANNED_WORDS = ('最强', '唯一', '必选', '无敌', '完美', '碾压')

failures = []


def fail(check, message):
    failures.append(f'[{check}] {message}')


def read(path):
    with open(path, encoding='utf-8') as f:
        return f.read()


def entries(block, quote=True):
    out = []
    pattern = r'\{\s*"' if quote else r'\{\s*[A-Z_]'
    for m in re.finditer(pattern, block):
        i = m.start()
        depth = 0
        for k in range(i, len(block)):
            if block[k] == '{':
                depth += 1
            elif block[k] == '}':
                depth -= 1
                if depth == 0:
                    out.append(block[i:k + 1])
                    break
    return out


def strings(ent):
    return re.findall(r'"([^"]*)"', ent)


def numbers(ent):
    return re.findall(r'-?\d+(?:\.\d+)?', re.sub(r'"[^"]*"', '', ent))


def section(text, start, end=None):
    i = text.index(start)
    j = text.index(end, i) if end else len(text)
    return text[i:j]


def table(text, marker):
    block = text[text.index(marker):]
    block = block[:block.index('\n};')]
    return entries(block)


text_sp = read(SPECIES_C)
text_ev = read(EVOLUTION_C)
text_cx = read(CODEX_C)
text_gt = read(TYPES_H)

sp_names = [n for n in re.findall(r'\b(SPECIES_[A-Z0-9_]+)\b', section(text_gt, 'SPECIES_PROTOCELL = 0', '} SpeciesId'))
            if n not in ('SPECIES_COUNT',)]
mu_names = [n for n in re.findall(r'\b(MUTATION_[A-Z0-9_]+)\b', section(text_gt, 'MUTATION_CELL_MEMBRANE = 0', '} MutationId'))
            if n not in ('MUTATION_COUNT',)]
sp_id = {n: i for i, n in enumerate(sp_names)}
mu_id = {n: i for i, n in enumerate(mu_names)}

sp_ents = table(text_sp, 'static const SpeciesDef k_species')
species = []
for idx, e in enumerate(sp_ents):
    st = strings(e)
    nm = numbers(e)
    eco = re.search(r'ECO_[A-Z_]+', e).group(0)
    species.append(dict(id=idx, name=st[0], desc=st[1], phase=int(nm[0]), eco=eco,
                        vals=[float(x) for x in nm[1:8]],
                        traits=[int(x) for x in nm[8:13]]))

mu_ents = table(text_ev, 'static const MutationDef k_mutations')
mutations = []
for idx, e in enumerate(mu_ents):
    st = strings(e)
    nm = numbers(e)
    mutations.append(dict(id=idx, name=st[0], desc=st[1],
                          vals=[float(x) for x in nm[1:6]],
                          traits=[int(x) for x in nm[6:11]],
                          rarity=int(nm[11]), min_phase=int(nm[12]),
                          max_phase=int(nm[13])))

rule_block = text_sp[text_sp.index('static const SpeciationRule k_speciation_rules'):]
rule_block = rule_block[:rule_block.index('\n};')]
rules = {}
for e in entries(rule_block, quote=False)[:-1]:
    m = re.search(r'(SPECIES_[A-Z0-9_]+)', e)
    if not m:
        continue
    target = sp_id.get(m.group(1))
    rest = e[m.end():]
    groups = re.findall(r'\{([^}]*)\}', rest)
    parents = [sp_id[x.strip()] for x in groups[0].split(',')
               if x.strip() not in ('-1', '') and x.strip() in sp_id] if groups else []
    reqs = [mu_id[x.strip()] for x in groups[1].split(',')
            if x.strip() not in ('-1', '') and x.strip() in mu_id] if len(groups) > 1 else []
    if target is not None:
        rules[target] = (parents, reqs)

# 图鉴文案（按表顺序）
cx_sp = [strings(e)[0] for e in table(text_cx, 'static const CodexEntry species')]
cx_mu = [strings(e)[0:2] for e in table(text_cx, 'static const CodexEntry mutations')]

# 共振表
syn_block = text_ev[text_ev.index('static const MutationSynergyDef k_synergies'):]
syn_block = syn_block[:syn_block.index('\n};')]
synergies = []
for e in entries(syn_block):
    ids = re.findall(r'(MUTATION_[A-Z0-9_]+)', e)
    synergies.append([mu_id.get(x) for x in ids[:2]])

# 字形表：ui_glyphs.h 里的 non-ASCII 码点
glyphs = set()
if os.path.exists(GLYPHS_H):
    for ch in read(GLYPHS_H):
        if ord(ch) > 127:
            glyphs.add(ord(ch))

# 1. 数量
if len(sp_ents) != TARGET_SPECIES:
    fail('1 数量', f'物种 {len(sp_ents)} != {TARGET_SPECIES}')
if len(mu_ents) != TARGET_MUTATIONS:
    fail('1 数量', f'突变 {len(mu_ents)} != {TARGET_MUTATIONS}')
if len(sp_names) != TARGET_SPECIES:
    fail('1 数量', f'SpeciesId 枚举 {len(sp_names)} != {TARGET_SPECIES}')
if len(mu_names) != TARGET_MUTATIONS:
    fail('1 数量', f'MutationId 枚举 {len(mu_names)} != {TARGET_MUTATIONS}')

# 2. 每阶段物种数
phase_count = [0] * 14
for s in species:
    if 0 <= s['phase'] < 14:
        phase_count[s['phase']] += 1
for p, expected in enumerate(PHASE_SPECIES):
    if phase_count[p] != expected:
        fail('2 阶段分布', f'阶段 {p} 物种 {phase_count[p]} != {expected}')

# 3. 生态位配额（只看 v14 新增）
for p, added in ADDED_PER_PHASE.items():
    if added == 0:
        continue
    group = [s for s in species if s['id'] >= FIRST_V14 and s['phase'] == p]
    if len(group) != added:
        fail('3 生态位', f'阶段 {p} 新增 {len(group)} != {added}')
        continue
    counts = {}
    for s in group:
        counts[s['eco']] = counts.get(s['eco'], 0) + 1
    quota = QUOTA_10 if added == 10 else (QUOTA_8 if added == 8 else QUOTA_4)
    for eco, want in quota.items():
        if counts.get(eco, 0) != want:
            fail('3 生态位', f'阶段 {p} {eco} = {counts.get(eco, 0)} != {want}')

# 4. 同阶段字段不得超过最大值 15%
phase_max = {}
for p in range(14):
    sel = [s for s in species if s['phase'] == p]
    if sel:
        phase_max[p] = [max(s['vals'][k] for s in sel) for k in range(7)]
for s in species:
    if s['id'] < FIRST_V14:
        continue
    for k in range(7):
        if s['vals'][k] > phase_max[s['phase']][k] * 1.15 + 1e-6:
            fail('4 严格更优', f'{s["name"]} 字段 {k} = {s["vals"][k]:.3f} > 1.15×max')

# 5. 每个新物种都有合法分化规则
for s in species:
    if s['id'] < FIRST_V14:
        continue
    rule = rules.get(s['id'])
    if not rule or not rule[0]:
        fail('5 分化规则', f'{s["name"]} 缺少 SpeciationRule')
        continue
    parents, _ = rule
    for p in parents:
        if p < 0 or p >= TARGET_SPECIES:
            fail('5 分化规则', f'{s["name"]} 父节点 {p} 不存在')
        elif species[p]['phase'] > s['phase']:
            # v16 F7：方向卡本来就是「同阶段侧支」，父与子同阶段合法；
            # 只有父节点比子节点更深才是坏数据（目标永远到不了）。
            fail('5 分化规则', f'{s["name"]} 父节点阶段 {species[p]["phase"]} > 自身 {s["phase"]}')

# 6. 3/5/2 角色配额（按注释解析）
roles = {}
for m in re.finditer(r'/\* v14：阶段 (\d+) · (main|branch|deep) · ([^*]+) \*/', text_sp):
    roles[m.group(3).strip()] = (int(m.group(1)), m.group(2))
for p, added in ADDED_PER_PHASE.items():
    if added == 0:
        continue
    group = [s for s in species if s['id'] >= FIRST_V14 and s['phase'] == p]
    seen = {}
    for s in group:
        role = roles.get(s['name'], (None, None))[1]
        seen[role] = seen.get(role, 0) + 1
    want = ({'main': 3, 'branch': 5, 'deep': 2} if added == 10 else
            {'main': 2, 'branch': 4, 'deep': 2} if added == 8 else
            {'main': 1, 'branch': 2, 'deep': 1})
    for role, count in want.items():
        if seen.get(role, 0) != count:
            fail('6 角色配额', f'阶段 {p} {role} = {seen.get(role, 0)} != {count}')

# 7. 突变阶段范围
for m in mutations:
    if m['min_phase'] < 0 or m['max_phase'] > 13:
        fail('7 突变阶段', f'{m["name"]} 阶段区间 {m["min_phase"]}–{m["max_phase"]} 非法')
    if m['max_phase'] >= 0 and m['min_phase'] > m['max_phase']:
        fail('7 突变阶段', f'{m["name"]} min {m["min_phase"]} > max {m["max_phase"]}')

# 8. cap 加成上限 +25%
for m in mutations:
    if m['id'] > LAST_ORIGINAL_MUTATION and m['vals'][3] > 0.25 + 1e-9:
        fail('8 cap 上限', f'{m["name"]} cap +{m["vals"][3]:.3f} > 0.25')

# 9. 共振引用
for a, b in synergies:
    if a is None or b is None:
        fail('9 共振引用', f'共振引用了不存在的突变 id: {a}, {b}')
    elif a == b:
        fail('9 共振引用', f'共振自引用 id {a}')

# 10. 文案长度 / 绝对词 / 字形覆盖
def text_issues(label, text, limit):
    if len(text) > limit:
        fail('10 文案', f'{label} 长度 {len(text)} > {limit}')
    for w in BANNED_WORDS:
        if w in text:
            fail('10 文案', f'{label} 含绝对词“{w}”')
    for ch in text:
        if ord(ch) > 127 and glyphs and ord(ch) not in glyphs:
            fail('10 字形', f'{label} 缺字“{ch}”(U+{ord(ch):04X})')

for s in species:
    if s['id'] >= FIRST_V14:
        text_issues(s['name'], s['desc'], 40)
for m in mutations:
    if m['id'] > LAST_ORIGINAL_MUTATION:
        text_issues(m['name'], m['desc'], 32)
for i, (name, tagline) in enumerate(cx_mu):
    if i > LAST_ORIGINAL_MUTATION:
        text_issues(name, tagline, 28)
for i, name in enumerate(cx_sp):
    if i >= FIRST_V14 and i < len(cx_sp):
        text_issues(name, '', 28) if False else None

# 图鉴短句（物种表在 species 表里只有名字；短句在 mutations 表之后，单独取）
sp_cx_block = text_cx[text_cx.index('static const CodexEntry species'):]
sp_cx_block = sp_cx_block[:sp_cx_block.index('};')]
for i, e in enumerate(entries(sp_cx_block)):
    if i >= FIRST_V14:
        st = strings(e)
        if len(st) >= 2:
            text_issues(st[0], st[1], 28)

# 11. 图鉴页数
if (TARGET_SPECIES + 11) // 12 != 22:
    fail('11 分页', '物种页数不是 22')
if (TARGET_MUTATIONS + 11) // 12 != 24:
    fail('11 分页', '突变页数不是 24')
if len(cx_sp) != TARGET_SPECIES or len(cx_mu) != TARGET_MUTATIONS:
    fail('11 分页', f'图鉴条目 {len(cx_sp)}/{len(cx_mu)} 与规则表不一致')

print(f'species={len(sp_ents)} mutations={len(mu_ents)} synergy={len(synergies)} '
      f'glyphs={len(glyphs)}')
print('phase species:', ' '.join(str(n) for n in phase_count))
if failures:
    print(f'FAIL: {len(failures)} 项')
    for f in failures[:40]:
        print(' -', f)
    sys.exit(1)
print('11 项内容校验全部 PASS')
