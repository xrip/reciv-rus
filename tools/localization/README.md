# EN -> RU string mapping for reciv

`make-mapping.py` builds a mapping table from the English 474.05 `CIV.EXE`
(reciv's source text) to the Russian retail `CIV.EXE` in `civ/`. The RU EXE is
the 1991 retail base patched in place to CP866: string order is kept, so
strings can be paired by position.

    python tools/localization/make-mapping.py \
        --en C:/Temp/civ47405-en/files/CIV.EXE --ru civ/CIV.EXE

(EN 474.05 EXE: download from https://www.civilization.fandom.com/wiki/Civilization_Updates
and unpack; the data segment is plaintext in the packed file.)

## How it works

1. Extract NUL-terminated literals from both EXEs (`0x0A` and 0x80..0xFF are
   part of strings: multi-line messages and CP437 glyphs are kept whole).
2. Sync points: identical `*KEY` anchors, identical long literals, and fixed
   tables detected from the reciv source lists (place_name 256x13, unit types,
   improvements 46, hall_text 24, civ_type 58-byte records, advances 72,
   terrain 24, governments, trade goods, adj8). Table names in the RU EXE are
   paired by index: the patch keeps source order. `pair_seq` finds each table
   as a window of the RU name stream, anchored and cross-checked by several
   known translations.
3. Longest increasing subsequence over sync points gives the offset backbone;
   between backbone points a DP aligns the literal streams (cost: length and
   punctuation-skeleton similarity). Strong gap pairs feed back as syncs.
4. Every display literal in `src/game/*.C` (1198) is looked up: table pair ->
   exact EXE literal pair -> exact bytes anywhere -> line-aligned containment;
   last resort is an interpolated RU offset (`interp`, review these rows).

## Output (tools/localization/data/)

- `mapping.csv` - file, line, english, russian, method, match. Methods in
  confidence order: `table` (index pairing), `gap` (DP alignment),
  `interp` (offset estimate, needs a human check).
- `pairs.csv` - all EXE offset pairs with kinds.
- `unmatched-src.txt`, `unmatched-ru.txt`, `syncs.txt`, `stats.txt`.

Current result: 1163/1198 mapped (642 table, 495 gap, 26 interp); the `drop`
set and `unmatched-src.txt` are the literals that stay English on purpose.

## Applying: the language layer

    git checkout c6ea308 -- src/game      # the English civd base
    python tools/localization/make-mapping.py --en <EN EXE> --ru civ/CIV.EXE
    python tools/localization/apply-translation.py

`apply-translation.py` wraps every mapped `table`/`gap` literal in the source
(1137 lines): `"G\207AME"` -> `TRANSLATE("G\207AME", "RU")`, marks the line
with `/* reciv-ru */`, keeps CRLF endings, re-applies the `CODE_FIXES` list
(SAVE.C drive-sentinel comparisons, font picks, layout re-spacing), and then
writes `data/ru.lang` from the final wrapped tree - tab-separated

    FILE.C <TAB> line <TAB> english literal body <TAB> russian text

where the russian column is readable UTF-8 text made of the CP866 characters
themselves; `\n \t \r \" \\ \xNN` escape texts cover the bytes text cannot
show, and an empty russian column is the empty string (the pack really blanks
such slots).  Line numbers refer to the wrapped tree - after the code fixes -
the numbers `langgen.c` will look up.  Manual russian edits in `ru.lang` are
kept across regenerations while their key (file, line, english) exists.
`interp` rows are never applied; review them first.

At build time CMake compiles `reciv-langgen` (`tools/localization/langgen.c`)
and expands every `TRANSLATE` call into the escaped CP866 literal from
`ru.lang`; the period library is built from those copies in
`<build>/period-gen`, so the period code still sees only plain-ASCII
literals and no compiler charset options are needed (MSVC and clang cannot
convert execution charsets to CP866 - measured).  A direct compile of
`src/game` without the langgen step keeps English (prelude fallback macro).
Migration check 2026-10: the langgen output was byte-identical to the old
committed escaped tree, 43/43 files.

## Known version differences (retail vs 474.05)

- Their civ idx10 is Lumumba/Congo ("Лумумба"/"Негры"), 474.05 has Shaka/Zulu.
- Their menu bar has 4 items (`G)Игра O)Команды W)Мир C)Цивилопедия`); no
  ADVISORS string exists in their EXE.
- Their `ratings[]` differs (five strings: Стойкие Красивые Посредственные
  Бедные Несчастные, plus Хилые elsewhere); only the tail three are mapped.
- Their `trade_goods[]` differs in content (Алмазы Агат Чай Перец); index
  pairing stays faithful to their game.
- Their historian list includes Ленин; hall entries localize two names
  (Игорь Великий for Simon Bolivar, Индиана Джонс for Dan Quayle).
- Long prose (story, news, help) lives in .TXT files - the RU pack already has
  translated copies, reciv picks them up through its file lookup order.
