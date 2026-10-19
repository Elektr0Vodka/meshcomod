#!/usr/bin/env python3
"""Companion boards this fork releases, read from companion_boards.ini.

companion_boards.ini is the one list of released companion boards. This script
derives everything else from it:

  envs                 release envs, one per line (build.sh, release workflow)
  pr-envs [--json]     envs the PR build check compiles
  readme [--write]     the README device table; without --write, print it
  check                fail if the README table is stale, an env is listed twice,
                       or an env has no [env:...] section under variants/

Run from the repo root (build.sh does). Standard library only.
"""

import configparser
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANIFEST = os.path.join(ROOT, "companion_boards.ini")
README = os.path.join(ROOT, "README.md")
TABLE_START = "<!-- companion-boards:start (generated from companion_boards.ini, do not edit by hand) -->"
TABLE_END = "<!-- companion-boards:end -->"
CI_SECTION = "ci"
REQUIRED = ("name", "mcu", "display", "envs", "status")


def load():
    cp = configparser.ConfigParser(interpolation=None, comment_prefixes=(";", "#"))
    with open(MANIFEST, encoding="utf-8") as f:
        cp.read_file(f)
    boards = []
    for section in cp.sections():
        if section == CI_SECTION:
            continue
        b = dict(cp[section])
        missing = [k for k in REQUIRED if not b.get(k)]
        if missing:
            sys.exit(f"companion_boards.ini [{section}]: missing {', '.join(missing)}")
        b["id"] = section
        b["envs"] = b["envs"].split()
        b["pr_check"] = b.get("pr_check", "").split()
        unknown = [e for e in b["pr_check"] if e not in b["envs"]]
        if unknown:
            sys.exit(f"companion_boards.ini [{section}]: pr_check names envs not in envs: {' '.join(unknown)}")
        boards.append(b)
    ci = cp[CI_SECTION] if cp.has_section(CI_SECTION) else {}
    pr_extra = ci.get("pr_check_extra", "").split()
    return boards, pr_extra


def release_envs(boards):
    return [e for b in boards for e in b["envs"]]


def pr_envs(boards, pr_extra):
    return [e for b in boards for e in b["pr_check"]] + pr_extra


def readme_table(boards):
    rows = [
        "| Device | MCU | Display | Build env | Fork build |",
        "|---|---|---|---|---|",
    ]
    for b in boards:
        device = f"**{b['name']}**" + (f" {b['note']}" if b.get("note") else "")
        envs = ", ".join(f"`{e}`" for e in b["envs"])
        if b.get("env_note"):
            envs += f" {b['env_note']}"
        rows.append(f"| {device} | {b['mcu']} | {b['display']} | {envs} | {b['status']} |")
    return "\n".join(rows)


def splice_readme(text, table):
    pattern = re.compile(re.escape(TABLE_START) + r"\n.*?\n" + re.escape(TABLE_END), re.S)
    if not pattern.search(text):
        sys.exit("README.md: companion-boards markers not found")
    # Blank lines keep the markers out of the table (a line right after a table row
    # would continue the table).
    return pattern.sub(lambda _: f"{TABLE_START}\n\n{table}\n\n{TABLE_END}", text, count=1)


def defined_envs():
    names = set()
    for path in [os.path.join(ROOT, "platformio.ini")] + glob.glob(os.path.join(ROOT, "variants", "*", "*.ini")):
        with open(path, encoding="utf-8", errors="replace") as f:
            names.update(re.findall(r"^\[env:([^\]]+)\]", f.read(), re.M))
    return names


def read_readme():
    with open(README, encoding="utf-8", newline="") as f:
        return f.read()


def check(boards, pr_extra):
    problems = []
    envs = release_envs(boards)
    dupes = sorted({e for e in envs if envs.count(e) > 1})
    if dupes:
        problems.append("listed more than once: " + " ".join(dupes))
    known = defined_envs()
    missing = [e for e in envs + pr_extra if e not in known]
    if missing:
        problems.append("no [env:...] section under variants/: " + " ".join(missing))
    text = read_readme().replace("\r\n", "\n")
    if splice_readme(text, readme_table(boards)) != text:
        problems.append("README device table is stale, run: python3 scripts/companion_boards.py readme --write")
    for p in problems:
        print(f"companion_boards: {p}", file=sys.stderr)
    return 1 if problems else 0


def main(argv):
    sys.stdout.reconfigure(encoding="utf-8", newline="\n")
    cmd = argv[1] if len(argv) > 1 else ""
    boards, pr_extra = load()
    if cmd == "envs":
        print("\n".join(release_envs(boards)))
    elif cmd == "pr-envs":
        envs = pr_envs(boards, pr_extra)
        print(json.dumps(envs) if "--json" in argv else "\n".join(envs))
    elif cmd == "readme":
        table = readme_table(boards)
        if "--write" in argv:
            text = read_readme()
            eol = "\r\n" if "\r\n" in text else "\n"
            new = splice_readme(text.replace("\r\n", "\n"), table).replace("\n", eol)
            with open(README, "w", encoding="utf-8", newline="") as f:
                f.write(new)
        else:
            print(table)
    elif cmd == "check":
        return check(boards, pr_extra)
    else:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
