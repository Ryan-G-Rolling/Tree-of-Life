#!/usr/bin/env python3
"""从源码里的中文字符串生成字体字形集头文件。

为什么需要：Raylib 的 LoadFontEx 只加载你显式给出的码点，
默认只有 ASCII。中文必须提前登记，否则界面上全是方框。

用法：
    python tools/gen_glyphs.py
    （在 TreeOfLife/ 目录下执行；ui_init 前会自动读取 include/ui_glyphs.h）

什么时候重跑：加了新的中文字面量，或 B 把真实物种/突变表接进来之后。
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCES = ["src", "include", "tests/mock"]
OUT = os.path.join(ROOT, "include", "ui_glyphs.h")

STRING_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')


def collect():
    chars = set()
    for folder in SOURCES:
        base = os.path.join(ROOT, folder)
        if not os.path.isdir(base):
            continue
        for name in sorted(os.listdir(base)):
            if not name.endswith((".c", ".h")):
                continue
            with open(os.path.join(base, name), encoding="utf-8") as f:
                text = f.read()
            for literal in STRING_RE.findall(text):
                for ch in literal:
                    if ord(ch) > 0x7F:
                        chars.add(ch)
    return "".join(sorted(chars, key=ord))


def escape(text):
    out = []
    for ch in text:
        if ch in ('"', "\\"):
            out.append("\\" + ch)
        elif ch == "?":
            out.append("\\?")
        else:
            out.append(ch)
    return "".join(out)


def main():
    text = collect()
    chunks = [text[i:i + 400] for i in range(0, len(text), 400)] or [""]
    lines = [
        "/* 由 tools/gen_glyphs.py 自动生成，请勿手改。 */",
        "#ifndef UI_GLYPHS_H",
        "#define UI_GLYPHS_H",
        "",
        "#define UI_GLYPH_PART_COUNT %d" % len(chunks),
        "",
        "static const char *const UI_GLYPH_PARTS[UI_GLYPH_PART_COUNT] = {",
    ]
    for c in chunks:
        lines.append('    "%s",' % escape(c))
    lines += ["};", "", "#endif /* UI_GLYPHS_H */", ""]
    with open(OUT, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print("glyphs: %d unique chars -> %s" % (len(text), os.path.relpath(OUT, ROOT)))


if __name__ == "__main__":
    sys.exit(main())
