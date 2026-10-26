#!/usr/bin/env python3
"""Build an English -> Russian string mapping from two Civilization DOS CIV.EXE files.

Pipeline:
  1. Extract NUL-delimited literals from data regions of both EXEs; detect
     fixed-record name tables (place names 13, unit types 34, civ names 16).
  2. Sync pairs: *KEY anchors (identical text in both EXEs), common literals,
     table records paired by index (translation patches in place, order kept).
  3. Monotone backbone (LIS), per-gap DP alignment of literal streams.
  4. Map every display literal in src/game (reciv, 474.05 English) to Russian.

Usage:
  python tools/localization/make-mapping.py --en <english CIV.EXE> --ru <russian CIV.EXE>
Output in tools/localization/data/: mapping.csv, pairs.csv, unmatched-ru.txt,
unmatched-src.txt, syncs.txt, stats.txt
"""

import argparse
import csv
import re
from pathlib import Path

def is_cyr(b):
    return (0x80 <= b <= 0x9F) or (0xA0 <= b <= 0xAF) or (0xE0 <= b <= 0xEF) or b in (0xF0, 0xF1)

def looks_like_ru(seg):
    return sum(1 for x in seg if is_cyr(x)) >= 2

def decode_ru(ba, space_box=False):
    s = ba.split(b"\x00")[0].decode("cp866", "replace").rstrip(" ")
    # the RU patch uses 0xB0 ('░') as an inner space in some long names
    if space_box:
        s = s.replace("░", " ")
    return s

def decode_en(ba):
    return ba.split(b"\x00")[0].decode("latin1").rstrip(" ")

def is_text_en(b):
    # the EXE keeps 0x0A inside NUL-terminated multi-line messages, and game
    # strings use CP437 glyph bytes 0x80..0xFF too
    return (0x20 <= b <= 0x7E) or (0x80 <= b <= 0xFF) or b == 0x0A

def is_text_ru(b):
    return is_text_en(b)

def extract_literals(buf, lo, hi, ru=False, minlen=3, loose=True):
    """Strict: start right after NUL. Loose: also runs that merely end at NUL."""
    out = []
    seen = set()
    text = is_text_ru if ru else is_text_en
    i = lo
    while i < hi:
        if text(buf[i]):
            j = i
            k = j
            while k < hi and text(buf[k]):
                k += 1
            if k < hi and buf[k] == 0x00 and k - j >= minlen:
                strict = j > lo and buf[j - 1] == 0x00
                if strict or loose:
                    seg = buf[j:k]
                    ok = looks_like_ru(seg) if ru else re.search(rb"[A-Za-z][A-Za-z0-9 ,.\-^'!?()$/:%+*]", seg)
                    if ok and j not in seen:
                        seen.add(j)
                        out.append((j, seg))
            i = max(k + 1, i + 1)
        else:
            i += 1
    return out

def find_anchors(buf, lo, hi):
    out = []
    for m in re.finditer(rb"\*[A-Z][A-Z0-9_]{2,12}", buf[lo:hi]):
        s = lo + m.start()
        e = lo + m.end()
        if s > 0 and buf[s - 1] in (0x00, 0x0A) and e < len(buf) and buf[e] in (0x00, 0x0A, 0x0D):
            out.append((s, buf[s:e]))
    return out

def rec_name_ok(seg, name):
    return seg[:len(name)] == name and all(c in (0x20, 0x00) for c in seg[len(name):])

def detect_table_en(buf, names, stride, width=None):
    """Find a fixed-stride padded-name table using source order of known names."""
    width = width or stride
    nbs = [n.encode("latin1") for n in names]
    probe = [nb for nb in nbs[:6] if len(nb) <= width]
    if len(probe) < 3:
        return None
    for o in range(0x1000, len(buf) - stride * len(probe)):
        if all(rec_name_ok(buf[o + stride * k: o + stride * k + width], probe[k])
               for k in range(len(probe))):
            # extend backward: shift to index 0 of the names list
            start = o - stride * len(probe)
            while start >= 0 and not rec_name_ok(buf[start:start + width], nbs[0]):
                start += stride
                if start > o:
                    start = o
                    break
            return start
    return None

def detect_table_ru(buf, lo, hi, stride, want, name_width=None):
    """Find a run of `want` consecutive fixed-stride records that look like Cyrillic names."""
    name_width = name_width or stride
    best = None
    o = lo
    while o < hi - stride * 20:
        seg = buf[o:o + name_width]
        if not looks_like_ru(seg) or 0x00 in seg[:1]:
            o += 1
            continue
        run = 0
        while run < want + 40:
            rec = buf[o + stride * run: o + stride * (run + 1)]
            rec_name = buf[o + stride * run: o + stride * run + name_width]
            if len(rec) < stride or not looks_like_ru(rec_name) or 0x00 in rec_name[:1]:
                break
            run += 1
        if run >= want:
            if best is None or run > best[1]:
                best = (o, run)
            o += stride * run
        else:
            o += 1
    return best

def ru_name_stream(buf, lo, hi):
    """Name-like strings split at NUL bytes: whole segments between NULs, leading and
    trailing bytes outside the text range trimmed (record int bytes leak in at edges)."""
    out = []
    i = lo
    while i < hi:
        j = buf.find(b"\x00", i, hi)
        if j < 0:
            break
        seg = buf[i:j]
        # a name stream segment starts at the first Cyrillic byte; leading bytes are
        # leaked int/punctuation junk from the record layout (e.g. "15", "3\x0b", '"\n')
        k = 0
        while k < len(seg) and not is_cyr(seg[k]):
            k += 1
        seg = seg[k:]
        while seg and not (is_cyr(seg[-1]) or 0x20 <= seg[-1] <= 0x7E):
            seg = seg[:-1]
        if seg and looks_like_ru(seg) and len(seg) <= 60 and is_cyr(seg[0]):
            out.append((i + k, seg))
        i = j + 1
    return out

def norm_ru(d):
    # RU patch strings can start with junk (punctuation/digits left from fixed fields)
    i = 0
    while i < len(d) and not d[i].isalpha():
        i += 1
    return d[i:]

def pair_seq(src, stream, anchors, tag, max_gap=130):
    """Pair a source name list with a window of the RU name stream.
    anchors: list of (source_name, [CP866 word prefixes]). Each candidate window must
    satisfy every anchor at its index, so a wrong anchor occurrence is rejected."""
    dec = [norm_ru(decode_ru(s)) for _, s in stream]
    n = len(src)
    checks = [(src.index(nm), w) for nm, w in anchors if nm in src]
    for ai, words in checks:
        for k, d in enumerate(dec):
            if not any(d.startswith(w) for w in words):
                continue
            st = k - ai
            if st < 0 or st + n > len(stream):
                continue
            win = stream[st:st + n]
            gaps = [win[i + 1][0] - win[i][0] for i in range(n - 1)]
            if max(gaps) > max_gap:
                continue
            if not all(any(dec[st + j].startswith(w) for w in w2) for j, w2 in checks):
                continue
            print(f"{tag}: {n} names @{win[0][0]:x}..{win[-1][0]:x} "
                  f"gap<={max(gaps)}: {', '.join(decode_ru(s, True) for _, s in win[:6])}")
            return win
    print(f"{tag}: not paired")
    return None

def lis_pairs(pairs):
    pairs = sorted(pairs, key=lambda p: p[0])
    ru = [p[1] for p in pairs]
    n = len(ru)
    dp = [1] * n
    prev = [-1] * n
    bi, bl = 0, 0
    for i in range(n):
        for j in range(i):
            if ru[j] < ru[i] and dp[j] + 1 > dp[i]:
                dp[i] = dp[j] + 1
                prev[i] = j
        if dp[i] > bl:
            bl, bi = dp[i], i
    chain = []
    i = bi
    while i >= 0:
        chain.append(pairs[i])
        i = prev[i]
    return list(reversed(chain))

def align_gap(en_lits, ru_lits, cap=400):
    if len(en_lits) > cap or len(ru_lits) > cap:
        return []
    n, m = len(en_lits), len(ru_lits)
    GAP = 6
    def cost(e, r):
        if e[1] == r[1]:
            return 0
        d = abs(len(e[1]) - len(r[1]))
        if d <= max(4, int(0.75 * max(len(e[1]), len(r[1])))):
            base = 1 + min(d, 16) // 4
        else:
            base = 10
        # translated strings keep the punctuation skeleton (':', '(', '\n', glyph marks);
        # mismatching skeletons are the usual sign of a shifted pairing
        extra = 0
        for ch in (0x3A, 0x28, 0x29, 0x0A, 0x8F, 0x8C, 0x87):
            if e[1].count(ch) != r[1].count(ch):
                extra += 1
        return base + extra
    dp = [[0] * (m + 1) for _ in range(n + 1)]
    for i in range(1, n + 1):
        dp[i][0] = dp[i - 1][0] + GAP
    for j in range(1, m + 1):
        dp[0][j] = dp[0][j - 1] + GAP
    for i in range(1, n + 1):
        ei = en_lits[i - 1]
        for j in range(1, m + 1):
            dp[i][j] = min(dp[i - 1][j - 1] + cost(ei, ru_lits[j - 1]),
                           dp[i - 1][j] + GAP, dp[i][j - 1] + GAP)
    pairs = []
    i, j = n, m
    while i > 0 and j > 0:
        c = dp[i][j]
        if c == dp[i - 1][j - 1] + cost(en_lits[i - 1], ru_lits[j - 1]):
            pairs.append((en_lits[i - 1], ru_lits[j - 1]))
            i -= 1
            j -= 1
        elif c == dp[i - 1][j] + GAP:
            i -= 1
        else:
            j -= 1
    return list(reversed(pairs))

C_ESC = {"\\n": b"\n", "\\t": b"\t", "\\r": b"\r", "\\\\": b"\\", '\\"': b'"', "\\'": b"'"}

def unescape(s):
    out = bytearray()
    i = 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            mo = re.match(r"[0-7]{1,3}", s[i + 1:])
            if mo:
                out.append(int(mo.group(0), 8) & 0xFF)
                i += 1 + len(mo.group(0))
                continue
            n2 = s[i:i + 2]
            if n2 in C_ESC:
                out += C_ESC[n2]
                i += 2
                continue
            if n2 == "\\x" and i + 4 <= len(s):
                out.append(int(s[i + 2:i + 4], 16))
                i += 4
                continue
            out += s[i + 1].encode("latin1")
            i += 2
            continue
        out += c.encode("latin1")
        i += 1
    return bytes(out)

def src_literals(src_dir):
    skip_name = re.compile(r"\.(H|C|EXE|CVL|CV|PIC|PAL|TXT|SVE|DTA|MAP|ME|COM|BAT)$", re.I)
    out = []
    for p in sorted(Path(src_dir).rglob("*.C")):
        text = p.read_text("latin1")
        for lineno, line in enumerate(text.splitlines(), 1):
            if re.match(r"\s*#include", line):
                continue
            for m in re.finditer(r'"((?:[^"\\]|\\.)*)"', line):
                s = m.group(1)
                if s.startswith("*") or skip_name.search(s) or len(s) < 3:
                    continue
                if not re.search(r"[A-Za-z]{2}", s):
                    continue
                out.append((p.name, lineno, s, unescape(s)))
    return out

def parse_named_array(src_text, decl_regex, inner_only=True):
    m = re.search(decl_regex, src_text)
    if not m:
        return []
    start = src_text.find("{", m.end() - 1)
    if start < 0:
        return []
    depth, end = 0, start
    while end < len(src_text):
        if src_text[end] == "{":
            depth += 1
        elif src_text[end] == "}":
            depth -= 1
            if depth == 0:
                break
        end += 1
    body = src_text[start + 1:end]
    return re.findall(r'"([^"]*)"', body)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--en", required=True)
    ap.add_argument("--ru", required=True)
    ap.add_argument("--src", default="src/game")
    ap.add_argument("--out", default="tools/localization/data")
    args = ap.parse_args()

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    en = open(args.en, "rb").read()
    ru = open(args.ru, "rb").read()

    en_lo, en_hi = 0x1000, len(en) - 0x1000
    ru_lo, ru_hi = 0x1000, len(ru) - 0x1000

    en_anch = find_anchors(en, en_lo, en_hi)
    ru_anch = find_anchors(ru, ru_lo, ru_hi)
    en_lits = extract_literals(en, en_lo, en_hi, ru=False)
    ru_lits = extract_literals(ru, ru_lo, ru_hi, ru=True)
    print(f"anchors: EN {len(en_anch)} RU {len(ru_anch)}; literals: EN {len(en_lits)} RU {len(ru_lits)}")

    syncs = {}  # en_off -> (ru_off, kind)
    table_pairs = {}  # source english name -> ru bytes

    # 1) anchors: identical *KEY text in both
    emap, rmap = {}, {}
    for o, t in en_anch:
        emap.setdefault(t, []).append(o)
    for o, t in ru_anch:
        rmap.setdefault(t, []).append(o)
    for t, eo in emap.items():
        ro = rmap.get(t, [])
        if len(eo) == len(ro):
            for a, b in zip(eo, ro):
                syncs[a] = (b, "anchor")

    # 2) common literals
    eby, rby = {}, {}
    for o, s in en_lits:
        eby.setdefault(s, []).append(o)
    for o, s in ru_lits:
        rby.setdefault(s, []).append(o)
    for s, eo in eby.items():
        ro = rby.get(s)
        if ro and len(eo) == len(ro) and len(s) >= 10:
            for a, b in zip(eo, ro):
                syncs.setdefault(a, (b, "common"))

    cap = open("src/game/CAPTURE.C", encoding="latin1").read()
    game = open("src/game/GAME.C", encoding="latin1").read()

    # 3) place_name 256x13 (record 0 padding is dirty in the EN EXE, probe from index 1)
    place_off = None
    names = parse_named_array(cap, r"place_name\[256\]\[13\]")
    print("place names parsed:", len(names))
    if len(names) >= 200:
        eo = detect_table_en(en, names[1:], 13)
        if eo is not None:
            eo -= 13
        ro = detect_table_ru(ru, ru_lo, ru_hi, 13, want=180)
        print(f"place EN={eo and hex(eo)} RU={ro and (hex(ro[0]), ro[1])}")
        if eo is not None and ro is not None:
            for i in range(256):
                syncs[eo + 13 * i] = (ro[0] + 13 * i, "table:place")
                table_pairs[names[i]] = ru[ro[0] + 13 * i: ro[0] + 13 * i + 13]

    # 4) RU name tables (units 28, improvements 46, hall_text 24): in the RU EXE the
    #    names are runs of consecutive NUL strings kept in source order (only the
    #    translation changed). Pair by index, anchored on names whose translation is
    #    known for sure.
    ru_stream = ru_name_stream(ru, ru_lo, ru_hi)

    units = [n for n, _ in re.findall(r'\{\s*"([^"]+)"((?:,\s*-?\d+){11})\s*\}', game)]
    win = pair_seq(units, ru_stream, [("Cavalry", ["Кавалери"]), ("Phalanx", ["Фаланг"]),
                                      ("Legion", ["Легион"])], "units")
    if win:
        for i in range(len(units)):
            table_pairs.setdefault(units[i], win[i][1])
        uo = detect_table_en(en, units, 34, width=12)
        if uo is not None:
            print("unit EN records @", hex(uo))
            for i in range(len(units)):
                syncs.setdefault(uo + 34 * i, (win[i][0], "table:units"))

    imp = parse_named_array(game, r"improvements\[46\]")
    win = pair_seq(imp, ru_stream, [("Barracks", ["Казарм"]), ("Granary", ["Амбар"]),
                                    ("Cathedral", ["Собор"])], "improvements")
    if win:
        for i in range(len(imp)):
            table_pairs.setdefault(imp[i], win[i][1])

    fame = open("src/game/FAME.C", encoding="latin1").read()
    hall = parse_named_array(fame, r"hall_text\[24\]\[25\]")
    win = pair_seq(hall, ru_stream, [("Cleopatra", ["Клеопатра"]), ("Charlemagne", ["Шарлемаг"])],
                   "hall", max_gap=40)
    if win:
        for i in range(len(hall)):
            table_pairs.setdefault(hall[i], win[i][1])

    adv = parse_named_array(game, r"advances\[72\]")
    win = pair_seq(adv, ru_stream, [("Alphabet", ["Алфавит"]), ("Mathematics", ["Математик"]),
                                    ("Philosophy", ["Философ"]), ("Chemistry", ["Хими"]),
                                    ("Computers", ["Компьютер"]), ("Religion", ["Религи"]),
                                    ("Medicine", ["Медицин"]), ("Navigation", ["Навигаци"]),
                                    ("MapMaking", ["Картограф"])], "advances", max_gap=200)
    if win:
        for i in range(len(adv)):
            table_pairs.setdefault(adv[i], win[i][1])

    terr = parse_named_array(game, r"terrains\[24\]")
    win = pair_seq(terr, ru_stream, [("Desert", ["Пустыня"]), ("Ocean", ["Океан"]),
                                     ("River", ["Река"]), ("Mountains", ["Гор"])], "terrain", max_gap=70)
    if win:
        for i in range(len(terr)):
            table_pairs.setdefault(terr[i], win[i][1])

    gov = parse_named_array(game, r"government_names\[6\]")
    win = pair_seq(gov, ru_stream, [("Anarchy", ["Анархи"]), ("Monarchy", ["Монарх"]),
                                    ("Republic", ["Республ"]), ("Democratic", ["Демократ"])],
                    "government", max_gap=70)
    if win:
        for i in range(len(gov)):
            table_pairs.setdefault(gov[i], win[i][1])

    goods = parse_named_array(game, r"trade_goods\[8\]")
    win = pair_seq(goods, ru_stream, [("Silk", ["Шелк"]), ("Wine", ["Вино"]),
                                      ("Salt", ["Соль"])], "goods", max_gap=40)
    if win:
        for i in range(len(goods)):
            table_pairs.setdefault(goods[i], win[i][1])

    # 5) civ_type[16]: leader[16] plural[16] adjective[16] + 5 ints = 58-byte fixed records.
    #    In RU the same layout; Caesar is index 1 (index 0 is an empty placeholder).
    #    Note: one civ slot can hold a different people per version (their idx10 is
    #    Lumumba/Congo, 474.05 has Shaka/Zulu) - index pairing stays faithful to the RU game.
    rows = re.findall(r'\{\s*"([^"]*)",\s*"([^"]*)",\s*"([^"]*)",\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+)\s*\}', game)
    leaders = [r[0] for r in rows]
    plurals = [r[1] for r in rows]
    adjs = [r[2] for r in rows]
    if len(rows) == 16:
        dec = [decode_ru(s) for _, s in ru_stream]
        civ_o = None
        for k, d in enumerate(dec):
            if not d.startswith("Цезарь"):
                continue
            o = ru_stream[k][0] - 58
            if o < 0:
                continue
            good = sum(1 for i in range(1, 16) for f in (0, 16, 32)
                       if ru[o + 58 * i + f] and looks_like_ru(ru[o + 58 * i + f: o + 58 * i + f + 16]))
            if good >= 30:
                civ_o = o
                break
        if civ_o is not None:
            print("civ RU records @", hex(civ_o), "leaders:", ", ".join(decode_ru(ru[civ_o + 58 * i: civ_o + 58 * i + 16], True) for i in range(16)))
            for i in range(16):
                for base, lst in ((0, leaders), (16, plurals), (32, adjs)):
                    rb = ru[civ_o + 58 * i + base: civ_o + 58 * i + base + 16]
                    if lst[i] and looks_like_ru(rb):
                        table_pairs.setdefault(lst[i], rb)
            eo = detect_table_en(en, [l for l in leaders if l], 58, width=16)
            if eo is not None:
                eo -= 58
                print("civ EN records @", hex(eo))
                for i in range(16):
                    syncs.setdefault(eo + 58 * i, (civ_o + 58 * i, "table:civ"))

    # 6) civ_adjective_text[8][11]
    adj8 = parse_named_array(game, r"civ_adjective_text\[8\]\[11\]")
    if len(adj8) >= 6:
        eo = detect_table_en(en, adj8[:4], 11)
        ro = detect_table_ru(ru, ru_lo, ru_hi, 11, want=5)
        print(f"adj8 EN={eo and hex(eo)} RU={ro and (hex(ro[0]), ro[1])}")
        if eo is not None and ro is not None:
            for i in range(len(adj8)):
                syncs.setdefault(eo + 11 * i, (ro[0] + 11 * i, "table:adj8"))
                rb = ru[ro[0] + 11 * i: ro[0] + 11 * i + 11]
                if looks_like_ru(rb):
                    table_pairs.setdefault(adj8[i].rstrip(), rb)

    # 6) backbone + gaps
    spairs = [(e, v[0]) for e, v in syncs.items()]
    backbone = lis_pairs(spairs)
    print(f"backbone: {len(backbone)} of {len(spairs)}")

    e_by_off = {o: s for o, s in en_lits}
    r_by_off = {o: s for o, s in ru_lits}
    pairs = {}
    for e, (r, k) in syncs.items():
        seg_e = e_by_off.get(e) or en[e:e + 64].split(b"\x00")[0]
        seg_r = r_by_off.get(r) or ru[r:r + 96].split(b"\x00")[0]
        pairs[e] = (seg_r, k)

    for _pass in range(2):
        spairs = [(e, v[0]) for e, v in syncs.items()]
        backbone = lis_pairs(spairs)
        # a synthetic tail pair keeps the region after the last anchor alignable
        bb = backbone + [(en_hi, ru_hi)]
        for i in range(len(bb) - 1):
            e0, r0 = bb[i]
            e1, r1 = bb[i + 1]
            if e1 <= e0 or r1 <= r0 or e1 - e0 > 0x6000 or r1 - r0 > 0x9000:
                continue
            gl = [x for x in en_lits if e0 < x[0] < e1]
            gr = [x for x in ru_lits if r0 < x[0] < r1]
            for (eo, es), (ro, rs) in align_gap(gl, gr, cap=1200):
                if eo not in pairs:
                    pairs[eo] = (rs, "gap")
                    # feed strong gap pairs back as sync points
                    if abs(len(es) - len(rs)) <= max(4, len(es) // 2):
                        syncs.setdefault(eo, (ro, "gap"))

    # 6c) manual pairs for regions where the two versions' arrays are not 1:1.
    #     Their menu bar has 4 items (G)Игра O)Команды W)Мир C)Цивилопедия);
    #     no ADVISORS string exists in their EXE. Block the EN bar offsets so no
    #     automatic method can invent a shifted pair.
    blocked = set()
    bar_en = b"G\x87AME\x00O\x87RDERS\x00A\x87DVISORS\x00W\x87ORLD\x00C\x87IVILOPEDIA"
    mo = en.find(bar_en)
    if mo >= 0:
        off = mo
        for nm in bar_en.split(b"\x00"):
            blocked.add(off)
            pairs.pop(off, None)
            syncs.pop(off, None)
            off += len(nm) + 1
    ro = ru.find("G)Игра".encode("cp866"))
    if ro >= 0:
        segs, o = [], ro
        while len(segs) < 4 and o < len(ru):
            e = ru.find(b"\x00", o)
            if e < 0:
                break
            if e > o:
                segs.append(ru[o:e])
            o = e + 1
        for key, rb in zip(("G\\207AME", "O\\207RDERS", "W\\207ORLD", "C\\207IVILOPEDIA"), segs):
            table_pairs[key] = rb
    # Their ADVISORS bar label is a lone string away from the rest of the bar.
    rb = ru.find("A)Советники".encode("cp866"))
    if rb >= 0:
        table_pairs["A\\207DVISORS"] = ru[rb:ru.find(b"\x00", rb)]
    # show_start() preview labels: the RU chain here has an extra "spacest.pic",
    # which shifted the automatic alignment by one (Village got Меню). Pair directly.
    preview = [b"Village", b"Menu Bar", b"Map Window", b"Active Unit"]
    q = en.find(b"\x00".join(preview))
    if q >= 0:
        off = q
        for nm in preview:
            blocked.add(off)
            pairs.pop(off, None)
            off += len(nm) + 1
        w = ru.find("Поселок".encode("cp866"))
        if w >= 0:
            o = w
            for key in ("Village", "Menu Bar", "Map Window", "Active Unit"):
                e = ru.find(b"\x00", o)
                table_pairs[key] = ru[o:e]
                o = e + 1
            print("preview labels RU @", hex(w))

    # Main menu and scenario customize block: the DP slipped across this whole block
    # (a few leader names landed onto the menu literals). The two EXEs line up 1:1
    # around the menu string, so pair that stretch explicitly and block bad pairs.
    q = en.find(b" Start a New Game\n Load a Saved Game")
    r = ru.find(" Новая игра".encode("cp866"))
    if q >= 0 and r >= 0:
        def strings_near(buf, o, back, fwd):
            res = []
            p = o
            for _ in range(fwd):
                e = buf.find(b"\x00", p)
                res.append((p, buf[p:e]))
                p = e + 1
            prev = []
            p = o
            for _ in range(back):
                s = buf.rfind(b"\x00", 0, p - 1)
                prev.append((s + 1, buf[s + 1:p - 1]))
                p = s + 1
            return list(reversed(prev)) + res
        ens = strings_near(en, q, 2, 10)
        rus = strings_near(ru, r, 2, 10)
        for (eo, es), (ro, rs) in zip(ens, rus):
            blocked.add(eo)
            pairs.pop(eo, None)
            syncs.setdefault(eo, (ro, "table:menu"))
            key = es.replace(b"\n", b"\\n").decode("latin1")
            if len(key) >= 5:
                table_pairs[key] = rs
        print("menu/customize block paired:", len(ens))

    reports = open("src/game/REPORTS.C", encoding="latin1").read()
    cats = parse_named_array(reports, r"categories\[\s*\]")
    win = pair_seq(cats, ru_stream, [("WEALTHIEST", ["БОГАТЕЙШИЕ"]), ("HAPPIEST", ["СЧАСТЛИВЫЕ"]),
                                     ("LARGEST", ["БОЛЬШИЕ"])], "categories", max_gap=40)
    if win:
        for i in range(len(cats)):
            table_pairs.setdefault(cats[i], win[i][1])
    # Their ratings array is not 1:1 with 474.05's seven ratings (their chain has
    # five strings: Стойкие Красивые Посредственные Бедные Несчастные + Хилые);
    # keep only the tail pairs confirmed by order, block the EN block.
    for key, word in (("Mediocre", "Посредственные"), ("Puny", "Бедные"), ("Pathetic", "Несчастные")):
        rb = ru.find(word.encode("cp866"))
        if rb >= 0:
            table_pairs.setdefault(key, ru[rb:rb + len(word.encode("cp866"))])
    for pat in (b"Great\x00Fine\x00Mediocre\x00Puny\x00Pathetic\x00Hopeless",
                b"Glorious\x00Great"):
        q = en.find(pat)
        if q >= 0:
            off = q
            for nm in pat.split(b"\x00"):
                blocked.add(off)
                pairs.pop(off, None)
                off += len(nm) + 1

    # 6d) rename captions and "Unknown city.": their EXE keeps this string group
    #     in a region far from the EN one (their 0x31cec..0x31d60: "set", four
    #     captions, "Unknown city."), so the gap DP matched the EN captions to
    #     unrelated table bytes there. Pair by content instead.
    for en_s, ru_s in (("City Name...", "Имя города.."),
                       ("Your Name...", "Ваше Имя..."),
                       ("Name of your Tribe...", "Имя вашего племени..."),
                       ("Where in the heck is ... (city name)", "Укажите начало имени ...(имя города)"),
                       ("Unknown city.", "Город не найден")):
        rb = ru.find(ru_s.encode("cp866"))
        if rb >= 0:
            table_pairs.setdefault(en_s, ru[rb:rb + len(ru_s.encode("cp866"))])
            for o, s in en_lits:
                if s == en_s.encode("latin1"):
                    blocked.add(o)
                    pairs.pop(o, None)

    # 6e) end-of-turn status block (EN 0x26798..0x267bc): their chain is
    # "Конец хода" / "" / "Нажмите Entr" - the last string is truncated to fit
    # the 12-byte slot, and the middle one was emptied.  The gap DP slid the
    # values by one row; pin the pairs.
    for en_s, ru_s in (("End of Turn", "Конец хода"),
                       ("Press Enter", ""),
                       ("to continue", "Нажмите Entr")):
        table_pairs.setdefault(en_s, ru_s.encode("cp866"))
    for o, s in en_lits:
        if s in (b"End of Turn", b"Press Enter", b"to continue"):
            blocked.add(o)
            pairs.pop(o, None)

    # 6f) foreign advisor status block (EN 0x28f4c..0x29035 chain) and the
    # spaceship part table (their 0x3154a..0x3161a): the gap DP slid these
    # tables by one or two entries (their records hold control bytes between
    # labels).  Pair by the labels' content, read from their EXE.
    def ru_label(txt):
        rb = ru.find(txt.encode("cp866"))
        if rb < 0:
            return None
        e = ru.find(b"\x00", rb)
        return ru[rb:e]
    for en_s, ru_s in (
        ("Subject: the ", "Тема: "),
        ("Capital:\\x8f ", "Столица:\x0f "),
        ("Government:\\x8f ", "Гос. строй:\x0f "),
        ("Treasury:\\x8f ", "Капитал: \x0f "),
        ("Military:\\x8f ", "Войска:  \x0f "),
        ("President", "Президент"),
        ("Structure1", "Структура1"),
        ("Structure2", "Структура2"),
        ("Structure3", "Структура3"),
        ("Propulsion", "Двигатель"),
        ("Fuel", "Горючее"),
        ("NONE", "Пусто"),
        ("Habitation", "Каюты"),
        ("Life Support", "Жизнеобеспеч└"),
        ("Solar Panel", "Солн. батареи"),
        ("Support", "Поддержка"),
    ):
        if en_s in table_pairs:
            continue
        rb = ru_label(ru_s)
        if rb is not None:
            table_pairs[en_s] = rb
    # Their advisor-status rows also hold 0f/0c/07 control bytes in place of
    # the EN 87 codes; block the EN block from automatic pairing.
    for pat in (b"Capital:\x8f \x00Government:\x8f \x00Treasury:\x8f \x00$\x00Military:\x8f \x00",
                b"Subject: the \x00"):
        q = en.find(pat)
        if q >= 0:
            off = q
            for nm in pat.rstrip(b"\x00").split(b"\x00"):
                blocked.add(off)
                pairs.pop(off, None)
                syncs.pop(off, None)
                off += len(nm) + 1

    # 70) hex-verified pins: the pack blanked or reused some slots, so the slot
    # chain above shifts the pairing by one or two strings. Values are copied
    # from civ/CIV.EXE dumps. Keys use raw source spelling (escapes stay two
    # characters); values are real text, encoded to cp866 here.
    pins = {
        "Empire": "Гос-во",
        "Kingdom": "Царство",
        " of the ": ": ",
        " founded: ": "░основан┼░",
        "Total Score: ": "Общий балл: ",
        "Bonus Score: ": "",
        # fake newspaper mastheads.  The pack's slots hold mistranslated
        # time/place words ('░Сверху' @0x31a6b, '░Вчера', '░Сбоку', '░Вечером',
        # '░Днем') - 'Weekly' rendered as "from above" etc.  Replaced with the
        # matching news names in the pack's ░ banner style (policy 2026-10:
        # fix wrong words, keep pack style).
        " Weekly": "░Неделя",
        " Today": "░Сегодня",
        " Times": "░Время",
        " Tribune": "░Трибуна",
        " News": "░Новости",
        "EXTRA!": "╜╛┐",
        "FLASH": "",
        r"The war between\nthe ": "Война народов\n",
        "Select new section.": "Выберите часть.",
        r"Which section shall we improve?\n": "Какую часть будем улучшать?\n",
        "Please insert Disk ": "Вставьте диск ",
        r".\n \n    Press Return\n    to continue.\n": ".\n \n  Нажмите Enter\n для продолжения.\n",
        " the CONQUEROR!": "░завоеватель├",
        " LAUNCH space ship!": " Корабль░стартовал├",
        "the advance pictured below.": "показанного ниже.",
        "The Wonders of the World": "Чудеса Света",
        "CIVILIZATION POWERGraph": "Уровень цивилизации",
        "The Top Five Cities in the World": "Пятерка лучших в мире городов",
    }
    for en_s, ru_s in pins.items():
        table_pairs[en_s] = ru_s.encode("cp866")

    ru_used = set()
    for v in pairs.values():
        for o, s in ru_lits:
            if s is v[0]:
                ru_used.add(o)
                break
    unmatched_ru = [(o, s) for o, s in ru_lits if o not in ru_used]

    # 7) reciv inventory
    bb = [(en_lo, ru_lo)] + backbone + [(en_hi, ru_hi)]

    def interp_ru(eo):
        for i in range(len(bb) - 1):
            e0, r0 = bb[i]
            e1, r1 = bb[i + 1]
            if e0 <= eo <= e1 and e1 > e0 and r1 > r0:
                return r0 + (r1 - r0) * (eo - e0) // (e1 - e0)
        return None

    def interp_pair(o_en, es):
        # last resort: EN offset known, DP left it unpaired - estimate the RU
        # offset from the backbone and take the nearest plausible literal.
        if len(es) < 4 or o_en in blocked or re.fullmatch(rb"[a-z0-9_.]{1,6}", es):
            return None  # too short, blocked region, or an internal identifier (pal1, SVE...)
        r_exp = interp_ru(o_en)
        if r_exp is None:
            return None
        best = None
        for o, s in ru_lits:
            if abs(o - r_exp) > 400 or not looks_like_ru(s):
                continue
            if not (0.45 * len(es) <= len(s) <= 2.2 * len(es) + 8):
                continue
            sc = abs(len(s) - len(es)) + abs(o - r_exp) // 8
            if best is None or sc < best[0]:
                best = (sc, s)
        return best[1] if best else None

    lits = src_literals(args.src)
    rows = []
    unmatched_src = []
    for fname, lineno, text, raw in lits:
        tkey = text if text in table_pairs else (text.strip() if text.strip() in table_pairs else None)
        if tkey is not None:
            seg = table_pairs[tkey]
            rows.append([fname, lineno, text, decode_ru(seg, True), "table", "exact", seg.hex()])
            continue
        hits = [o for o, s in en_lits if s == raw]
        if hits:
            for o in hits:
                p = pairs.get(o)
                rs = p[0] if p else interp_pair(o, raw)
                if rs is not None:
                    rows.append([fname, lineno, text, decode_ru(rs, True), p[1] if p else "interp", "exact", rs.hex()])
                    break
            else:
                unmatched_src.append((fname, lineno, text + f"  [en@0x{hits[0]:x} unpaired]"))
            continue
        # exact bytes anywhere in EN file
        o = en.find(raw)
        if o >= 0:
            p = pairs.get(o)
            rs = p[0] if p else interp_pair(o, raw)
            if rs is not None:
                rows.append([fname, lineno, text, decode_ru(rs, True), p[1] if p else "interp", "exact-anywhere", rs.hex()])
                continue
        # containment: literal sits at a line start of a longer paired EN EXE string
        for o, s in en_lits:
            pos = s.find(raw)
            if len(raw) >= 8 and pos >= 0 and (pos == 0 or s[pos - 1] == 0x0A):
                p = pairs.get(o)
                if p:
                    rs = p[0]
                    if b"\n" in s and rs.count(b"\n") == s.count(b"\n"):
                        rs = rs.split(b"\n")[s[:pos].count(b"\n")]
                    rows.append([fname, lineno, text, decode_ru(rs, True), p[1] + "+whole", "partial", rs.hex()])
                    break
        else:
            unmatched_src.append((fname, lineno, text))

    # 71) correct the generated rows: for slots the pack blanked or reused, the
    # automatic methods landed on a neighbour slot or on unrelated text. Override
    # those, drop rows whose EN slot has no usable RU slot, and add literals too
    # short for interp_pair (it skips len < 4). Only table/gap rows get applied.
    row_over = {
        # the ADVISORS government-name slots in the pack are Гос-во/Царство/
        # Республика/": "; the automatic pairing picked 'Республ.' from a
        # different region, and "Republic" cannot go through table_pairs
        # because other files use the word for other text.
        ("ADVISORS.C", 44): "Республика",
        # the WLKD banner is drawn from love1/love2.pic (pack data, already
        # Russian); love_anim uses no text literal.  The pack chain at
        # 0x2f110/0x2f136 holds "loved/not loved anymore" news strings, which
        # are mapped through their own EN rows.  Nothing to pin here.
        ("NOTES.C", 113): "- Подсказка Цивилизации -",
        ("CAPTURE.C", 276): "Нет",
        ("NEWGAME.C", 471): "НЕТ",
        ("SPACESHP.C", 52): "Пусто",
        # the city-management «Конц» button (box 284..316, 33px): «Конец» is
        # 29px in font 1 (measured with the FONTS.CV width table) - the full
        # word fits the button, so the abbreviation is expanded.
        ("CITYMGT.C", 1376): "Конец",
        # the end-of-turn panel drew the instruction twice: the English
        # "Press Enter" (line 1386, its pack slot is blanked - see drop) and
        # the RU «Нажмите Entr» here.  Merge into one «Нажми Enter»: 66px in
        # font 1 (FONTS.CV width table), panel is 80px wide.  The line-1386
        # removal is a CODE_FIX in apply-translation.py.
        ("CONTROL.C", 1387): "Нажми Enter",
        # the W menu item «Кос.Корабль» (pack 194596): the menu box is sized by
        # the longest row «Уровень цивилизации (F9)» = 137px; «Космический
        # корабль» is 105px (FONTS.CV font 1 table), so the full words fit.
        # The sidebar blurb «К.Корабль: (» (CITY.C:597) stays short - no room.
        ("MENUS.C", 289): " Карта Мира (F10)\n Демография\n Космический корабль\n",
        ("NEWS.C", 63): "    ",
        ("NEWS.C", 68): "    ",
        # format_year's eras are the pack slots "░днэ" at 0x2fe77 (verified in the
        # DOSBox sidebar pixels; the gap method mispaired it with a "░дэ" news
        # slot).  User decision 2026-10-26: write the normal "░до н.э." / "░н.э."
        # for every caller; the only width-tight one is the status line, whose
        # blurb tiles moved (CODE_FIX in apply-translation.py).  The three-key
        # form avoids clobbering the " AD" row on the same line.
        ("TURN.C", 670, " BC"): "░до н.э.",
        ("TURN.C", 670, " AD"): "░н.э.",
        # 75) save/load drive dialog (SAVE.C 229/230/251): the slot chain
        # shifted these by one, doubling the Russian.  Hex-verified pack slots:
        # 0x30c74 drive-letter line, 0x30cb1 escape line, 0x30cce no-disk line.
        ("SAVE.C", 229, ":\\n\\n    Press drive letter and\\nReturn when disk is inserted.\\n"):
            ":\n\n    Укажите имя дисковода\nи затем нажмите Enter.\n",
        ("SAVE.C", 230, "    Press Escape to cancel.\\n"): "    Esc - прерваться.\n",
        ("SAVE.C", 251, "No Disk in Drive A.\\n"): "Нет диска: Drive A.\n",
        # wonders() "The " (REPORTS.C 289): the DP chain paired it with an
        # unrelated battle string; the pack slot is four blanks - the pack
        # drops "The " (civ/CIV.EXE 0x30d69, slot order verified against the
        # EN table 0x28a8a..0x28a9f: The / of / in / . ( / ) / (Destroyed)
        # vs pack blank / blank / ' в: ' / '. (' / ')' / ' (Разрушено)').
        ("REPORTS.C", 289): "    ",
        # 76) the pack left some EN slots blanked, but the RU sentence it built
        # there is complete without the English head; rows keyed here so the
        # garbage interp estimate is never applied.  Wording from civ/CIV.EXE
        # byte dumps (offsets noted).
        # emissary intro: pack starts with the civ name; tail ' желают
        # поговоритьс вами└' @0x302bf (applied at 702); split that typo.
        ("AUDIENCE.C", 700): "",
        ("AUDIENCE.C", 702): " желают поговорить с вами└\n",
        # leader greeting case 2: pack 'давший смертным\nсчастье: ' @0x30392;
        # drop the trailing ': ' because line 766 already adds ': ' + title.
        ("AUDIENCE.C", 764): "давший смертным\nсчастье\n",
        # custom world creation menus (GAME.C 774/788/800): the pack did
        # translate these prompts; the DP chain paired them with blank slots.
        # Pack slots: 'Температура:...' @0x2e51f, 'Климат:...' @0x2e550,
        # 'Эра:...' @0x2e575.  The 'Эра' third option reads '5 миллиардов   '
        # in the pack - it cut its own 'лет' because '5 billion years' already
        # runs the menu box to the screen edge; keep the pack form.
        ("GAME.C", 774): "Температура:\n Холодно\n Тепло\n Жарко\n",
        ("GAME.C", 788): "Климат:\n Сухой\n Норм.\n Вода\n",
        ("GAME.C", 800): "Эра:\n 3 миллиарда лет\n 4 миллиарда лет\n 5 миллиардов   \n",
        # 78) clear language errors in the pack's own text (policy: fix them).
        # 'Новых технологие нет.' (DIPLOMAT.C 153) - the pack stores the wrong
        # case; the correct Russian genitive plural is 'технологий'.
        ("DIPLOMAT.C", 153): "Новых технологий нет.\n",
        # 79) the pack stores 'MORE'/'AUTO.'/'CHANGE' buttons with a Latin
        # first letter ('Mеще' @0x30eeb, 'Aвто.', 'Cтроим' - hex-verified
        # bytes 4d/41/43).  Same word length and width, only the initial
        # letter is Cyrillic now.  CP866 has no Ё, so 'Еще' stays without one.
        ("CITYMGT.C", 1253): "Еще",
        ("PEDIA.C", 99): "Еще",
        ("CITYMGT.C", 1265, "AUTO."): "Авто.",
        ("CITYMGT.C", 1265, "CHANGE"): "Строим",
        # 'of the <nation>' tail: pack chain ' ' @0x303b1 before '...\n' @0x303bb.
        ("AUDIENCE.C", 777): " ",
        # revolution news / gov-change announcement: pack tails ('░Правительство
        # \nбыло░свергнуто' @0x2f351) attach straight to the civ name.
        ("CONTROL.C", 1130): "",
        ("DIPLOMAC.C", 186): "",
        # peace/ceasefire messages: pack ' and\n' slots are '░и\n'
        # @0x2f373 (заключили @0x2f379) and @0x2f390 (прервали @0x2f396).
        ("DIPLOMAC.C", 239, " and\\n"): "░и\n",
        ("DIPLOMAC.C", 253, " and\\n"): "░и\n",
        # science query: pack has only the 'wise men' slot 'нами' @0x2ed43
        # ('В каком направлении надо\nработать' @0x2ed1c); the scientists slot
        # is blanked, so the pack shows nothing there.
        ("ECONOMY.C", 710, "scientists"): "",
        # landform intro: pack merged the story heads into 'долина реки и '
        # /'луга на ' @0x2ffc4/0x2ffd7 (mapped to 422); its 'river valley/
        # grasslands on a ' slots are blanked.
        ("NEWGAME.C", 424, "river valley on a "): "",
        ("NEWGAME.C", 427, "grasslands on a "): "",
        # civopedia page titles: the pack keeps one literal per page
        # ('Развитие цивилизации' @0x30652, 'Развитие городов' @0x3067d,
        # 'Чудеса Света' @0x3068e, 'Войска' @0x306ba, 'Территории' @0x306df);
        # gap pairing had dumped three EN titles on the one slot 0x3067d.
        ("PEDIA.C", 295): "Развитие цивилизации",
        ("PEDIA.C", 310, "City Improvement"): "Развитие городов",
        ("PEDIA.C", 310, "Wonder of the World"): "Чудеса Света",
        ("PEDIA.C", 322, "Military Unit"): "Войска",
        # advisor panel titles: pack titles already end in ':' (0x2f8b0
        # 'Министр Обороны:'); no ' report:' slot exists.
        ("DIALOG.C", 237): "",
        # historian quote: pack categories are uppercase ('БОГАТЕЙШИЕ' etc.
        # @0x30daa table); only the opening quote of 'The <cat> Civilizations'
        # survives.
        ("REPORTS.C", 351): "'",
        # spaceship parts dialog: pack blanked the head slot; tail
        # ' не надо.\n' is already applied at 473.
        ("SPACESHP.C", 471): "",
        # prerequisites connectors: ' и ' fits trivially between advance names
        # ('Требуется: ' @371 is pack wording).
        ("PEDIA.C", 375, " and "): " и ",
        ("QUIZ.C", 116, " and "): " и ",
    }
    # 77) rows the pack has no counterpart for: drop them so apply-translation
    # leaves the English literal (user decision 2026-10).  Entries may be
    # (file, line) or (file, line, english) when only one row on a line is
    # meant (NEWS.C 85: keep the EXTRA! pack row, revert FLASH to English).
    drop = {("AUDIENCE.C", 931), ("CITYMGT.C", 1377), ("REPORTS.C", 293),
            ("CITY.C", 645), ("CONTROL.C", 1386),
            # show_help() keys below stay EN: the pack stores the *help.txt
            # keys ('Land' @0x2e51a ... 'Age' @0x2e5ae) untranslated.
            ("GAME.C", 779), ("GAME.C", 793), ("NEWS.C", 85, "FLASH"),
            # show_help keys stay English: the pack help.txt keys are Latin
            # (*LAND/*TEMPERATURE/*CLIMATE/*AGE), a RU key finds no section.
            ("GAME.C", 804)}
    rows = [r for r in rows if (r[0], r[1]) not in drop
            and (r[0], r[1], r[2]) not in drop]
    seen = {(r[0], r[1], r[2]) for r in rows}
    for r in rows:
        # 'or' would skip an intentional "" override - look the keys up by None.
        v = row_over.get((r[0], r[1], r[2]))
        if v is None:
            v = row_over.get((r[0], r[1]))
        if v is not None:
            r[3], r[4], r[5] = v, "table", "exact"
            r[6] = v.encode("cp866").hex()
    # 72) civt plural columns (GAME.C 363..377): the pack's civt table
    # (civ/CIV.EXE 0x2de92, hex-verified, 16 x 58-byte records) stores the
    # plural in BOTH the plural and adjective columns, so the 474.02 code
    # takes the card name straight from the plural.  474.05 left those plural
    # slots empty and builds the plural as adjective + "s" (NEWGAME.C 161-165,
    # 311-318); with the pack adjectives that renders "Римлянеs" on the
    # leader card.  Fill each empty plural with the pack's plural column.
    civt_plural = {363: "Римляне", 364: "Вавилоняне", 366: "Египтяне",
                   367: "Американцы", 368: "Греки", 369: "Индийцы",
                   371: "Русские", 372: "Негры", 374: "Ацтеки",
                   377: "Монголы"}
    # 73) city status report (ADVISORS.C 661/670): civd's EN literal "\x87 ("
    # carries a 0x87 byte out of the EN font range, so EN draws just " (".  The
    # pack slot is plain " (" (DOSBox shows no glyph after the unit name); the
    # RU font would print 'З' ("Милиция3"), so map it to " (".  src_literals
    # skips this literal (no two-letter run), hence an explicit row.
    # 74) historian ratings (REPORTS.C 321-329): the pack table (civ/CIV.EXE
    # 0x30daa..0x30dd3, right after historians Геродот/Ленин/Тойнби/Гиббон)
    # holds 5 ratings Стойкие/Красивые/Посредственные/Бедные/Несчастные; 474.02
    # used 5 tiers and 474.05 put a new tier at each end.  The three middle
    # pairs are already mapped (325-327); Great and Fine take the pack's first
    # two.  Glorious (322) and Hopeless (328) are 474.05-only tiers - the pack
    # has no word for them, so they stay English.
    # 75) diplomat sabotage message (DIPLOMAT.C 181): EN slots 0x298c9..0x298e7
    # ' production sabotaged\n' / 'in ' / '.\n'; pack slots 0x31ba9..: ' массовый
    # саботаж в\n', two blanked slots (474.02 leftovers), 'г. ', '.\n'.  The pack
    # translated 'in ' to 'г. ' (city), like its 'Dissidents in ' -> 'Диссиденты:
    # г.'; the '.\n' slot is unchanged, so line 183 needs no row.  AUDIENCE.C
    # 'demand' (315) is left English: the pack has no 'demand'/'требуют' anywhere
    # (slot blanked at 0x300ec) because the RU *CANCEL blurb hard-codes 'Мы
    # требуем...' without $RPLC1, so the slot value is never shown.
    # no row for the "." period marker on DIPLOMAT.C 455: the city-founded
    # CODE_FIX in apply-translation.py deletes that whole strcat line.
    for row in [("DIPLOMAT.C", 181, "in ", "г. ")] + \
               [("ADVISORS.C", ln, r"\x87 (", " (") for ln in (661, 670)] + \
               [("REPORTS.C", 323, "Great", "Стойкие"),
                ("REPORTS.C", 324, "Fine", "Красивые"),
                # wonders() connectors (REPORTS.C 293), pack slots 0x30d6e/0x30d73
                ("REPORTS.C", 293, " of ", "    "),
                ("REPORTS.C", 293, " in ", " в: "),
                # save/load drive prompts (SAVE.C 227): src_literals skips them
                # (no letter pair after the leading blanks+escape).  Pack slots
                # 0x30bf5 (load; pack typos "диску"/"сохраненный" kept) and
                # 0x30c35 (save).  The 12-space tail is the drive-letter slot.
                ("SAVE.C", 227,
                 "  Which drive contains your\\n    saved game files?\\n\\n            ",
                 "  На каком диску находятся\nваши сохраненный игры?\n\n            "),
                ("SAVE.C", 227,
                 "  Which drive contains your\\n     Save Game disk?\\n\\n            ",
                 "  На каком диске находятся\n сохраненные игры?\n\n            ")] + \
               [("GAME.C", ln, "", ru) for ln, ru in sorted(civt_plural.items())]:
        if (row[0], row[1], row[2]) not in seen:
            rows.append([row[0], row[1], row[2], row[3], "table", "exact",
                         row[3].encode("cp866").hex()])

    with open(out_dir / "mapping.csv", "w", newline="", encoding="utf-8-sig") as f:
        w = csv.writer(f)
        w.writerow(["file", "line", "english", "russian", "method", "match", "ru_hex"])
        w.writerows(rows)
    with open(out_dir / "pairs.csv", "w", newline="", encoding="utf-8-sig") as f:
        w = csv.writer(f)
        w.writerow(["en_off", "english", "ru_off", "russian", "kind"])
        for e in sorted(pairs):
            ro = None
            for o, s in ru_lits:
                if s is pairs[e][0]:
                    ro = o
                    break
            w.writerow([f"0x{e:x}", decode_en(e_by_off.get(e, en[e:e + 64])),
                        f"0x{ro:x}" if ro is not None else "", decode_ru(pairs[e][0]), pairs[e][1]])
    with open(out_dir / "unmatched-ru.txt", "w", encoding="utf-8") as f:
        for o, s in unmatched_ru:
            f.write(f"0x{o:x}\t{decode_ru(s)}\n")
    with open(out_dir / "unmatched-src.txt", "w", encoding="utf-8") as f:
        for a, b, c in unmatched_src:
            f.write(f"{a}:{b}\t{c}\n")
    with open(out_dir / "syncs.txt", "w", encoding="utf-8") as f:
        for e in sorted(syncs):
            r, k = syncs[e]
            f.write(f"0x{e:x}\t0x{r:x}\t{k}\n")
    stats = [f"anchors EN={len(en_anch)} RU={len(ru_anch)}",
             f"literals EN={len(en_lits)} RU={len(ru_lits)}",
             f"syncs={len(syncs)} backbone={len(backbone)}",
             f"pairs={len(pairs)} gap={sum(1 for v in pairs.values() if v[1]=='gap')}",
             f"src literals={len(lits)} mapped={len(rows)} unmapped={len(unmatched_src)}",
             f"unmatched RU={len(unmatched_ru)}"]
    (out_dir / "stats.txt").write_text("\n".join(stats) + "\n", encoding="utf-8")
    print("\n".join(stats))

if __name__ == "__main__":
    main()
