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
import sys
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

    # TURN.C status_draw(): the expanded era reads ~62px from x2 (the era
    # strings themselves are plain mapping rows on TURN.C:670, see the
    # row_over note in make-mapping.py), so the research and warmth blurb
    # tiles move from 48/56 to 64/72 - still inside the 80px status panel.
    ("TURN.C",
     b"    blit(tile_page, clamp(a, 0, 3) * 8 + 160, 120, 8, 8, scrbuf, 48, 80);\r\n    if (warm_levels) {\r\n"
     b"        a = clamp(warm_levels / 4, 0, 3);\r\n"
     b"        blit(tile_page, a * 8 + 160, 128, 8, 8, scrbuf, 56, 80);",
     b'    /* reciv-ru: format_year now writes \xc2\xab\xe2\x96\x91\xd0\xb4\xd0'
     b'\xbe \xd0\xbd.\xd1\x8d.\xc2\xbb (62px max at x2), so the\r\n     * rese'
     b'arch/warmth blurb tiles move from 48/56 to 64/72'
     b' - still inside\r\n     * the 80px status panel */'
     b'\r\n    blit(tile_page, clamp(a, 0, 3) * 8 + 160, '
     b'120, 8, 8, scrbuf, 64, 80);\r\n    if (warm_levels'
     b') {\r\n        a = clamp(warm_levels / 4, 0, 3);\r\n'
     b'        blit(tile_page, a * 8 + 160, 128, 8, 8, '
     b'scrbuf, 72, 80);'),

    # DIPLOMAT.C city_founded(): the founding banner.  The Russian pack
    # renders it in the 20h..EFh serif font (its FONTS.CV entry 7 at 278Ch;
    # the driver indexes the table at 630h + 2n, RU MGRAPHIC.EXE 1558h).
    # Entry 6 is the Cyrillic-only 80h..EFh font at 1a74h, whose range check
    # skips the ASCII year digits (the banner drew no 4000).  Draw in font 7;
    # the period marker is dropped (the expanded era ends in '.' already).
    ("DIPLOMAT.C",
     b"format_year();\r\n"
     b"    strcat(msgtext, \"\\xC0\"); /* reciv-ru */\r\n"
     b"    scrbuf->font = 6;",
     b'format_year();\r\n    /* reciv-ru: format_year now'
     b" ends with \xc2\xab\xe2\x96\x91\xd0\xb4\xd0\xbe \xd0\xbd.\xd1\x8d.\xc2\xbb; the pack's \xc2\xab\xe2\x94\x94\xc2"
     b'\xbb period\r\n     * marker (drawn as a dot after \xc2\xab'
     b'\xe2\x96\x91\xd0\xb4\xd0\xbd\xd1\x8d\xc2\xbb) is dropped - the expanded era\r\n    '
     b" * already ends in a '.' (it printed \xc2\xab\xd0\xbd.\xd1\x8d..\xc2\xbb"
     b') */\r\n    scrbuf->font = 7;   /* reciv-ru: the p'
     b'ack banner draws in the 20h..EFh serif\r\n        '
     b'                 * font (Russian FONTS.CV entry '
     b'7 at 278Ch); entry 6 is\r\n                       '
     b'  * the Cyrillic-only 80h..EFh font at 1a74h, wh'
     b'ose range\r\n                         * swallows t'
     b'he ASCII year digits */'),

    # CITYVIEW.C city_view(): the name/year header over the city zoom view
    # uses the same font choice as the founding banner (see above).
    ("CITYVIEW.C",
     b"scrbuf->number = 1;\r\n    scrbuf->font = 6;",
     b'scrbuf->number = 1;\r\n    scrbuf->font = 7;   /* '
     b'reciv-ru: same as the founding banner - the pack'
     b'\r\n                         * draws this name/yea'
     b'r header in the 20h..EFh serif\r\n                '
     b'         * (entry 7, 278Ch); entry 6 (1a74h) is '
     b'Cyrillic-only\r\n                         * and wo'
     b'uld swallow the ASCII year digits */'),

    # CONTROL.C uinit(): the end-of-turn panel asked twice - the English
    # "Press Enter" (its pack slot is blanked; the row is in make-mapping's
    # drop set) plus the RU prompt at 1387.  Merge into the one prompt from
    # row_over ("CONTROL.C", 1387) and draw it at the pack's prompt line.
    ("CONTROL.C",
     b"        draw_string(\"Press Enter\", 4, 136, 0);\r\n"
     b"        draw_string(\"\\x8D\\xA0\\xA6\\xAC\\xA8 Enter\", 4, 144, 0); /* reciv-ru */",
     b"        /* reciv-ru: the pack blanks the English prompt here; its panel\r\n"
     b"         * shows the RU prompt only.  The EN and RU prompts said the same\r\n"
     b"         * thing, so the merged prompt (mapping row 1387) stays at the\r\n"
     b"         * pack's prompt position (144) */\r\n"
     b"        draw_string(\"\\x8D\\xA0\\xA6\\xAC\\xA8 Enter\", 4, 144, 0); /* reciv-ru */"),

    # TURN.C draw_banner(): the RU menu labels are wider than the English
    # 64px slots - "A)..." ran into "W)..." and "C)..." past the 320px edge.
    # Re-space x by the measured RU widths (36/53/61/32/75 px).
    ("TURN.C",
     b"    draw_string(\"G)\\x88\\xA3\\xE0\\xA0\", 8, 1, 15); /* reciv-ru */\r\n"
     b"    draw_string(\"O)\\x8A\\xAE\\xAC\\xA0\\xAD\\xA4\\xEB\", 64, 1, 15); /* reciv-ru */\r\n"
     b"    draw_string(\"A)\\x91\\xAE\\xA2\\xA5\\xE2\\xAD\\xA8\\xAA\\xA8\", 128, 1, 15); /* reciv-ru */\r\n"
     b"    draw_string(\"W)\\x8C\\xA8\\xE0\", 192, 1, 15); /* reciv-ru */\r\n"
     b"    draw_string(\"C)\\x96\\xA8\\xA2\\xA8\\xAB\\xAE\\xAF\\xA5\\xA4\\xA8\\xEF\", 240, 1, 15); /* reciv-ru */",
     b"    /* reciv-ru: re-spaced for the proportional RU labels (widths\r\n"
     b"     * 36/53/61/32/75 px): 8 52 113 182 222, last ends at 297 < 320 */\r\n"
     b"    draw_string(\"G)\\x88\\xA3\\xE0\\xA0\", 8, 1, 15); /* reciv-ru */\r\n"
     b"    draw_string(\"O)\\x8A\\xAE\\xAC\\xA0\\xAD\\xA4\\xEB\", 52, 1, 15); /* reciv-ru */\r\n"
     b"    draw_string(\"A)\\x91\\xAE\\xA2\\xA5\\xE2\\xAD\\xA8\\xAA\\xA8\", 113, 1, 15); /* reciv-ru */\r\n"
     b"    draw_string(\"W)\\x8C\\xA8\\xE0\", 182, 1, 15); /* reciv-ru */\r\n"
     b"    draw_string(\"C)\\x96\\xA8\\xA2\\xA8\\xAB\\xAE\\xAF\\xA5\\xA4\\xA8\\xEF\", 222, 1, 15); /* reciv-ru */"),

    # MENUS.C world_menu(): gray out "Wonders of the World" (menu item 0)
    # until the first wonder exists - the menu_core() dialog_dim bits draw the
    # row dimmed AND refuse selection, exactly how the game already dims the
    # ship item (bit 5 when !(game_flags & 0xfe00)).
    ("MENUS.C",
     b"    if (!(game_flags & 0xfe00))\r\n        dialog_dim = 0x20;\r\n",
     b"    if (!(game_flags & 0xfe00))\r\n        dialog_dim = 0x20;\r\n"
     b"    /* reciv-ru: dim the wonders item (bit 0) while no wonder exists;\r\n"
     b"     * dimmed menu rows are drawn dark and cannot be chosen (menu_core),\r\n"
     b"     * the same rule the ship item above uses */\r\n"
     b"    for (unused_word_c = 1; unused_word_c <= 21; unused_word_c++)\r\n"
     b"        if (marvel_site[unused_word_c] != -1)\r\n"
     b"            break;\r\n"
     b"    if (unused_word_c > 21)\r\n"
     b"        dialog_dim |= 0x01;\r\n"),
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

    # Rows are looked up by line number in the English civd tree, and rows with
    # an empty English literal (the civt plural fills) fall back to scanning
    # the file - running this twice patches them into the wrong lines.  Only
    # apply to a freshly reset base:  git checkout c6ea308 -- src/game
    for p in sorted(Path(args.src).glob("*.C")):
        if b"reciv-ru" in p.read_bytes():
            sys.exit(f"{p} already has reciv-ru lines - reset the base first: "
                     "git checkout c6ea308 -- src/game")

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

    out = [f"applied={applied} moved={moved} skipped={len(skipped)} "
           f"warnings={len(warnings)} codefixes={fixed}"]
    out += ["SKIP " + repr(s) for s in skipped]
    out += ["WARN " + w for w in warnings]
    sys.stdout.buffer.write(("\n".join(out) + "\n").encode("utf-8", "replace"))


if __name__ == "__main__":
    main()
