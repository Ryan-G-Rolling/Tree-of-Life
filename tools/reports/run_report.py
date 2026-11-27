# -*- coding: utf-8 -*-
"""Run the 1000-seed B regression and regenerate report assets.

用 Python 捕获 demo 的标准输出再落盘，避免 make recipe 里的 `>` 重定向
在某些 Windows shell 包装层下被吞掉。
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
DEMO = os.path.join(ROOT, "build", "demo_b.exe")


def capture(args):
    proc = subprocess.run(args, capture_output=True, text=True, encoding="utf-8",
                          errors="replace")
    if proc.returncode != 0:
        sys.stderr.write(proc.stderr)
        raise SystemExit("demo failed: %s" % " ".join(args))
    return proc.stdout


def main():
    # v16 F1 之后候选池从几十条涨到两百多条，探索模式（每步都可能开一次候选）
    # 慢了约 9 倍；探索只用于“覆盖率”这一记录项，采样 300 局足够，其余批次仍 1000。
    explore_n = os.environ.get("TOL_EXPLORE_N", "300")
    jobs = [
        ("batch2_scan_round15.csv", ["--batch2", "1000"]),
        ("batch3_scan_round15.csv", ["--batch3", "1000"]),
        ("batch3_explore_round15.csv", ["--batch3", explore_n, "--explore"]),
    ]
    for name, args in jobs:
        text = capture([DEMO] + args)
        with open(os.path.join(HERE, name), "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text)
        print("wrote %s (%d lines)" % (name, text.count("\n")))
    for script in ("gen_round15.py", "make_report_table.py"):
        proc = subprocess.run([sys.executable, os.path.join(HERE, script)], text=True)
        if proc.returncode != 0:
            raise SystemExit("%s failed" % script)
    return 0


if __name__ == "__main__":
    sys.exit(main())
