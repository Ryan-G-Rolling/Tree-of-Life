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
    # v19 D9：候选池按 (物种, 已购前置位掩码) 缓存后，探索模式恢复到 v16 之前的量级，
    # 覆盖率回到 1000 局口径（此前为了绕开 9 倍耗时降到 300，两套口径不可比）。
    explore_n = os.environ.get("TOL_EXPLORE_N", "1000")
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
