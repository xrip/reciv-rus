#!/usr/bin/env python3
"""Apply the EN -> RU mapping (mapping.csv) to the display literals in src/game/*.C.

Each mapped C string literal becomes  TRANSLATE("english", "RU")  - readable in
the source.  The Russian text goes to data/ru.lang (tab-separated: FILE.C, line,
english literal body, russian text; the russian column is plain UTF-8 text made
of CP866 characters, with \\n \\t \\r \" \\\\ \\xNN escape texts where the text
cannot show a byte).  The build tool tools/localization/langgen.c expands the
calls back into escaped CP866 literals, so the period code itself stays plain
ASCII.  Changed lines get a /* reciv-ru */ marker.  Files keep CRLF endings.

ru.lang is regenerated from the FINAL wrapped tree (after the code fixes that
add lines, so the recorded line numbers match what langgen will see).  Manual
edits of the russian column are kept while their key (file, line, english)
still exists; stale manual keys are reported and dropped.

Usage:
  python tools/localization/apply-translation.py
      [--csv tools/localization/data/mapping.csv]
      [--lang tools/localization/data/ru.lang]
      [--src src/game]
      [--methods table,gap]
      [--dry-run]
"""

import argparse
import csv
import re
import sys
from collections import defaultdict
from pathlib import Path

COMMENT = b" /* reciv-ru */"

TRANSLATE_RE = re.compile(rb'TRANSLATE\("((?:\\.|[^"\\])*)", "RU"\)')

# One-off code-semantic fixes kept in sync with the literal mapping, so a
# regenerated src/game gets them too.  Each entry: file, old bytes, new bytes.
# They run after the TRANSLATE wrapping, so their old/new match the wrapped
# text where a wrapped literal is involved.
CODE_FIXES = [
    # SAVE.C choose_drive(): drive_choice is 'unsigned int' = unsigned short
    # with a 0xFFFF init; MSC (16-bit int) compared it with -1 as 0xFFFF.
    # Modern C promotes both sides to 32-bit int, making the sentinel test
    # always false, so _dos_getdrive() was skipped and the dialog's drive
    # letter slot stayed blank.  Force the 16-bit comparison MSC did.
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
     b"    strcat(msgtext, \".\");\r\n"
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
     b"        draw_string(TRANSLATE(\"to continue\", \"RU\"), 4, 144, 0); /* reciv-ru */",
     b"        /* reciv-ru: the pack blanks the English prompt here; its panel\r\n"
     b"         * shows the RU prompt only.  The EN and RU prompts said the same\r\n"
     b"         * thing, so the merged prompt (mapping row 1387) stays at the\r\n"
     b"         * pack's prompt position (144) */\r\n"
     b"        draw_string(TRANSLATE(\"to continue\", \"RU\"), 4, 144, 0); /* reciv-ru */"),

    # TURN.C draw_banner(): the RU menu labels are wider than the English
    # 64px slots - "A)..." ran into "W)..." and "C)..." past the 320px edge.
    # Re-space x by the measured RU widths (36/53/61/32/75 px).
    ("TURN.C",
     b"    draw_string(TRANSLATE(\"G\\207AME\", \"RU\"), 8, 1, 15); /* reciv-ru */\r\n"
     b"    draw_string(TRANSLATE(\"O\\207RDERS\", \"RU\"), 64, 1, 15); /* reciv-ru */\r\n"
     b"    draw_string(TRANSLATE(\"A\\207DVISORS\", \"RU\"), 128, 1, 15); /* reciv-ru */\r\n"
     b"    draw_string(TRANSLATE(\"W\\207ORLD\", \"RU\"), 192, 1, 15); /* reciv-ru */\r\n"
     b"    draw_string(TRANSLATE(\"C\\207IVILOPEDIA\", \"RU\"), 240, 1, 15); /* reciv-ru */",
     b"    /* reciv-ru: re-spaced for the proportional RU labels (widths\r\n"
     b"     * 36/53/61/32/75 px): 8 52 113 182 222, last ends at 297 < 320 */\r\n"
     b"    draw_string(TRANSLATE(\"G\\207AME\", \"RU\"), 8, 1, 15); /* reciv-ru */\r\n"
     b"    draw_string(TRANSLATE(\"O\\207RDERS\", \"RU\"), 52, 1, 15); /* reciv-ru */\r\n"
     b"    draw_string(TRANSLATE(\"A\\207DVISORS\", \"RU\"), 113, 1, 15); /* reciv-ru */\r\n"
     b"    draw_string(TRANSLATE(\"W\\207ORLD\", \"RU\"), 182, 1, 15); /* reciv-ru */\r\n"
     b"    draw_string(TRANSLATE(\"C\\207IVILOPEDIA\", \"RU\"), 222, 1, 15); /* reciv-ru */"),

    # DIALOG.C menu_core(): the menu pre-selects item 0.  With the wonders
    # row dimmed (MENUS.C CODE_FIX below), the first selection change runs
    # the un-highlight recolour on its rectangle, and that pass repaints the
    # dimmed colour-3 text to 22 - the row goes blank.  Pre-select item 0
    # only when it is not dimmed.
    ("DIALOG.C",
     b"    if (!has_mouse && selected_e == -1 && !need_ok_key && !box_info_flags)\r\n"
     b"        selected_e = 0;\r\n"
     b"    selected_e = 0;\r\n",
     b"    if (!has_mouse && selected_e == -1 && !need_ok_key && !box_info_flags)\r\n"
     b"        selected_e = 0;\r\n"
     b"    /* reciv-ru: keep item 0 unselected when it is dimmed, so the\r\n"
     b"     * un-highlight repaint (11->7, 3->22) never touches its text */\r\n"
     b"    if (!(dialog_dim & 1))\r\n"
     b"        selected_e = 0;\r\n"),

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


def lang_field(data):
    """CP866 bytes -> the readable russian column of ru.lang (inverse of what
    langgen.c's field decoder accepts)."""
    out = []
    for b in data:
        if b == 0x0A: out.append("\\n")
        elif b == 0x09: out.append("\\t")
        elif b == 0x0D: out.append("\\r")
        elif b == 0x22: out.append('\\"')
        elif b == 0x5C: out.append("\\\\")
        elif 0x20 <= b <= 0x7E: out.append(chr(b))
        elif b >= 0x80: out.append(bytes([b]).decode("cp866"))
        else: out.append("\\x%02X" % b)
    return "".join(out)


def load_lang(path):
    manual = {}
    if path.exists():
        for ln in path.read_text(encoding="utf-8").splitlines():
            if not ln or ln.startswith("#"):
                continue
            fld = ln.split("\t")
            if len(fld) != 4:
                sys.exit(f"{path}: not a 4-field tab line: {ln!r}")
            manual[(fld[0], fld[1], fld[2])] = fld[3]
    return manual


def strip_slot(row):
    """Slot-padding rules the old c_escape pass applied to table records."""
    ru = bytes.fromhex(row["ru_hex"])
    if row["method"] == "table":
        ru = ru.rstrip(b"\x00")
        if not row["english"].endswith(" "):
            ru = ru.rstrip(b" ")
    return ru


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", default="tools/localization/data/mapping.csv")
    ap.add_argument("--lang", default="tools/localization/data/ru.lang")
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
    applied_rows = defaultdict(list)
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
            new = ('TRANSLATE("' + r["english"] + '", "RU")').encode("latin1")
            if len(strip_slot(r)) > len(old) - 2:
                warnings.append(f"{fname}:{lineno} longer than original: "
                                f"{r['english']!r} -> {r['russian']!r}")
            n = 0
            if 1 <= lineno <= len(lines):
                n = patch_line(lineno - 1, old, new)
            if n:
                applied += n
                applied_rows[fname].append(r)
                dirty.add(path)
                continue
            n = 0
            for i in range(len(lines)):
                n = patch_line(i, old, new)
                if n:
                    break
            if n:
                moved += 1
                applied_rows[fname].append(r)
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

    # Pair every TRANSLATE in the final tree with the mapping row that created
    # it (both walks are in document order; the english body must match), and
    # write ru.lang with the FINAL physical line numbers - the ones langgen.c
    # will look up.
    # Manual edits are recognized against ru.lang.prev - the previous run's
    # machine (auto) values.  A ru.lang line that differs from ru.lang.prev
    # is a user edit and gets kept; everything else is regenerated from the
    # mapping, so mapping fixes always win.
    old_lang = load_lang(Path(args.lang))
    prev_auto = load_lang(Path(str(args.lang) + ".prev"))
    auto_fields = {}
    lang_entries = []
    errors = []
    used_keys = set()
    for fname in sorted(by_file):
        path = Path(args.src) / fname
        if not path.exists():
            continue
        data = path.read_bytes()
        remaining = sorted(applied_rows[fname], key=lambda r: int(r["line"]))
        consumed = []
        line = 1
        prev = 0
        for m in TRANSLATE_RE.finditer(data):
            line += data.count(b"\n", prev, m.start())
            prev = m.start()
            en = m.group(1).decode("latin1")
            row = None
            for idx, r in enumerate(remaining):
                if r["english"] == en:
                    row = remaining.pop(idx)
                    break
            if row is None:
                for r in consumed:  # a wrapped literal repeated on one line
                    if r["english"] == en:
                        row = r
                        break
            if row is None:
                errors.append(f"{fname}:{line}: TRANSLATE({en!r}) matches no mapping row")
                continue
            consumed.append(row)
            key = (fname, str(line), en.replace("\t", "\\t"))
            ru = strip_slot(row)
            auto = lang_field(ru)
            auto_fields[key] = auto
            field = old_lang[key] if (key in prev_auto and
                                      key in old_lang and
                                      old_lang[key] != prev_auto[key]) else auto
            used_keys.add(key)
            lang_entries.append(key + (field,))
        for r in remaining:
            errors.append(f"{fname}: mapping row {r['line']} ({r['english']!r}) "
                          "left no TRANSLATE in the tree")
    stale = [k for k in old_lang if k not in used_keys]

    if not args.dry_run:
        header = ("# reciv RU language file - the russian column is editable text.\n"
                  "# key: FILE.C <TAB> line <TAB> english literal body <TAB> russian text\n"
                  "# apply-translation.py rewrites this file; a russian column that differs\n"
                  "# from ru.lang.prev (the machine values) is a manual edit and is kept.\n"
                  "# line numbers refer to src/game AFTER wrapping and code fixes.\n")
        with open(args.lang, "w", encoding="utf-8", newline="\n") as f:
            f.write(header)
            for k in lang_entries:
                f.write("\t".join(k) + "\n")
        with open(Path(str(args.lang) + ".prev"), "w", encoding="utf-8", newline="\n") as f:
            f.write("# machine values from the last run - diff ru.lang against this to\n"
                    "# see the manual edits; apply-translation.py regenerates this file.\n")
            for k in lang_entries:
                f.write("\t".join((k[0], k[1], k[2], auto_fields[k[:3]])) + "\n")

    out = [f"applied={applied} moved={moved} skipped={len(skipped)} "
           f"warnings={len(warnings)} codefixes={fixed} lang={len(lang_entries)}"]
    out += ["SKIP " + repr(s) for s in skipped]
    out += ["WARN " + w for w in warnings]
    out += ["ERROR " + e for e in errors]
    if stale:
        out.append(f"NOTE dropped {len(stale)} stale manual ru.lang keys: {stale[:8]}")
    sys.stdout.buffer.write(("\n".join(out) + "\n").encode("utf-8", "replace"))
    if errors:
        sys.exit(1)


if __name__ == "__main__":
    main()
