#!/usr/bin/env python3
"""Apply the EN -> RU mapping (mapping.csv) to the display literals in src/game/*.C.

Replaces each mapped C string literal with the Russian bytes written as C escapes
(\\xNN, \\n), so src/game stays plain ASCII and the period build sees the exact
CP866 bytes. Changed lines get a /* reciv-ru */ marker. Files keep CRLF endings.

Usage:
  python tools/localization/apply-translation.py
      [--csv tools/localization/data/mapping.csv]
      [--src src/game]
      [--methods table,gap]
      [--dry-run]
"""

import argparse
import csv
from collections import defaultdict
from pathlib import Path

COMMENT = b" /* reciv-ru */"

# One-off code-semantic fixes kept in sync with the literal mapping, so a
# regenerated src/game gets them too. Each entry: file, old bytes, new bytes.
CODE_FIXES = [
    # SAVE.C choose_drive(): drive_choice is 'unsigned int' = unsigned short
    # with a 0xFFFF init; MSC (16-bit int) compared it with -1 as 0xFFFF.
    # Modern C promotes both sides to 32-bit int, making the sentinel test
    # always false, so _dos_getdrive() was skipped and the dialog's drive
    # letter slot stayed blank. Force the 16-bit comparison MSC did.
    ("SAVE.C", b"if (drive_choice == -1) {",
     b"if (drive_choice == (unsigned short)-1) { /* reciv: MSC 16-bit "
     b"compare: 0xFFFF sentinel equals -1 */"),
    ("SAVE.C", b"if (drive_choice != -1 && !disk_ready(drive_choice)) {",
     b"if (drive_choice != (unsigned short)-1 && !disk_ready(drive_choice)) "
     b"{ /* reciv: same 16-bit sentinel */"),
]


def c_escape(data):
    """C string literal for raw bytes: printable ASCII as-is, else escapes.
    Adjacent literals are split so a \\xNN cannot swallow a following hex digit."""
    parts = []
    seg = []
    last_esc = False
    for b in data:
        plain = None
        if b == 0x0A:
            plain = "\\n"
        elif b == 0x22:
            plain = '\\"'
        elif b == 0x5C:
            plain = "\\\\"
        elif 0x20 <= b <= 0x7E:
            plain = chr(b)
        if plain is not None and len(plain) == 1 and last_esc and plain in "0123456789abcdefABCDEF":
            parts.append('"' + "".join(seg) + '"')
            seg = []
        if plain is not None:
            seg.append(plain)
            last_esc = False
        else:
            seg.append("\\x%02X" % b)
            last_esc = True
    if seg or not parts:
        parts.append('"' + "".join(seg) + '"')
    return " ".join(parts)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", default="tools/localization/data/mapping.csv")
    ap.add_argument("--src", default="src/game")
    ap.add_argument("--methods", default="table,gap")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    methods = set(m.strip() for m in args.methods.split(","))
    seen = set()
    by_file = defaultdict(list)
    for r in csv.DictReader(open(args.csv, encoding="utf-8-sig")):
        key = (r["file"], r["line"], r["english"])
        if r["method"] in methods and key not in seen:
            seen.add(key)
            by_file[r["file"]].append(r)

    applied = moved = 0
    skipped = []
    warnings = []
    dirty = set()
    for fname, frows in sorted(by_file.items()):
        path = Path(args.src) / fname
        if not path.exists():
            skipped.append((fname, 0, "file missing"))
            continue
        lines = path.read_bytes().split(b"\n")

        def patch_line(i, old, new):
            line = lines[i]
            n = line.count(old)
            if n == 0:
                return 0
            line = line.replace(old, new)
            if COMMENT not in line:
                stripped = line.rstrip(b"\r\n")
                line = stripped + COMMENT + line[len(stripped):]
            lines[i] = line
            return n

        for r in frows:
            lineno = int(r["line"])
            old = ('"' + r["english"] + '"').encode("latin1")
            ru = bytes.fromhex(r["ru_hex"])
            if r["method"] == "table":
                # table records carry field padding; C arrays pad with NULs anyway.
                # Keep trailing spaces when the English literal has them: the
                # pack's slot really ends in a space (": ", "    ").
                ru = ru.rstrip(b"\x00")
                if not r["english"].endswith(" "):
                    ru = ru.rstrip(b" ")
            new = c_escape(ru).encode("latin1")
            if len(ru) > len(old) - 2:
                warnings.append(f"{fname}:{lineno} longer than original: "
                                f"{r['english']!r} -> {r['russian']!r}")
            n = 0
            if 1 <= lineno <= len(lines):
                n = patch_line(lineno - 1, old, new)
            if n:
                applied += n
                dirty.add(path)
                continue
            n = 0
            for i in range(len(lines)):
                n = patch_line(i, old, new)
                if n:
                    break
            if n:
                moved += 1
                dirty.add(path)
            else:
                skipped.append((fname, lineno, r["english"]))

        if dirty and not args.dry_run and path in dirty:
            path.write_bytes(b"\n".join(lines))

    fixed = 0
    for fname, old, new in CODE_FIXES:
        path = Path(args.src) / fname
        data = path.read_bytes()
        if old not in data:
            if new not in data:
                skipped.append((fname, 0, "code fix: " + old.decode("latin1")))
            continue
        if not args.dry_run:
            path.write_bytes(data.replace(old, new, 1))
        fixed += 1

    import sys
    out = [f"applied={applied} moved={moved} skipped={len(skipped)} "
           f"warnings={len(warnings)} codefixes={fixed}"]
    out += ["SKIP " + repr(s) for s in skipped]
    out += ["WARN " + w for w in warnings]
    sys.stdout.buffer.write(("\n".join(out) + "\n").encode("utf-8", "replace"))


if __name__ == "__main__":
    main()
