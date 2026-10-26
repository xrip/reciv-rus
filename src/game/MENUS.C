/* Resident code segment 1C84 (0000..080F): the command menus (game, unit
   orders, advisors, world and civilopedia menus).  Names are provisional. */
#include <string.h>
#include "CONTROL.H"
#include "CORE.H"
#include "DIALOG.H"
#include "ECONOMY.H"
#include "MAP.H"
#include "GLOBALS.H"
#include "RESIDENT.H"
#include "SPACESHP.H"
#include "ADVISORS.H"
#include "FAME.H"
#include "GAME.H"
#include "MENUS.H"
#include "PEDIA.H"
#include "REPLAY.H"
#include "REPORTS.H"
#include "TURN.H"
#include "UNITS.H"

struct unit12 {
    unsigned char x, y;
    char type;
    char pad[6];
    char cargo;
    char pad2[2];
};
struct terr_fx {
    int irrigate, a, mine, b, c, d;
};
struct unit_type {
    char name[12];
    int obsolete_by, domain, w2, w3, w4, w5, w6, w7, w8, w9, w10;
};
struct terrain_rec {
    char name[12];
    unsigned char f0, f1, f2, f3, f4, f5, f6;
};

extern int debug;
extern char armydata[];
#define unit_tab ((struct unit12 (*)[128]) (armydata + 1))      /* units from x */
extern struct terr_fx work_rules[];
extern struct unit_type utypes[];
extern struct terrain_rec terrains[];

extern int map_flags(int, int), terrain_at(int, int);

extern void unit_table(int);
extern void spy_rpt();

extern void world_map(long);
extern void civ_score(int, long);

void cmd_menu(int x, int y, int sel)
{
    cmd_code = -1;
    menu_result = 0;
    if (sel == -1)
        sel = clamp(ptr_x / 60, 0, 4);
    mouse_off();
    menu_begin();
    mouse_on();
    switch (sel) {
    case 0:
        game_menu_a();
        break;
    case 1:
        unit_orders_menu(x, y);
        break;
    case 2:
        advisor_menu();
        break;
    case 3:
        world_menu();
        break;
    case 4:
        civilopedia_menu();
        break;
    }
    mouse_off();
    if (menu_result == 1L)
        sel_player();
    if (menu_result == 0L)
        menu_end();
    mouse_on();
}

void game_menu_a(void)
{
    int choice_h, unused_a_o, unused_b_m;

    if (!turn_tick)
        dialog_dim = 0x10;
    strcpy(msgtext, " \x8D\xA0\xAB\xAE\xA3\xA8\n \x90\xAE\xE1\xAA\xAE\xE8\xEC\n \x8D\xA0\xA9\xE2\xA8 \xA3\xAE\xE0\xAE\xA4\n \x8F\xA0\xE0\xA0\xAC\xA5\xE2\xE0\xEB\n \x91\xAE\xE5\xE0\xA0\xAD\xA8\xE2\xEC\xE1\xEF\n \x90\x85\x82\x8E\x8B\x9E\x96\x88\x9F\n \n \x8D\xA0 \xAF\xA5\xAD\xE1\xA8\xEE\n \x82\xEB\xE5\xAE\xA4 \xA2 DOS\n"); /* reciv-ru */
    if (game_flags & 0x100)
        strcat(msgtext, " \x8F\xE0\xAE\xE1\xAC\xAE\xE2\xE0\xA5\xE2\xEC\n"); /* reciv-ru */
    choice_h = menu_choose(msgtext, 0x10, 8);
    repaint();
    switch (choice_h) {
    case 5:
        cmd_code = -2;
        break;
    case 0:
        cmd_code = 0x3d;
        break;
    case 1:
        cmd_code = 0x2d;
        break;
    case 2:
        cmd_code = 0x3f;
        break;
    case 3:
        strcpy(msgtext, "\x8F\xA0\xE0\xA0\xAC\xA5\xE2\xE0\xEB:\n \x84\xA0\xE2\xEC \xE1\xAE\xA2\xA5\xE2\n \x80\xA2\xE2\xAE\x91\xAE\xE5\xE0. \n \x8A\xAE\xAD\xA5\xE6 \x95\xAE\xA4\xA0\n \x80\xAD\xA8\xAC\xA0\xE6\xA8\xEF  \n \x87\xA2\xE3\xAA\xA8\n \x95\xAE\xA4\xEB \xA2\xE0\xA0\xA3\xAE\xA2\n \x92\xA5\xAA\xE1\xE2 \xE6\xA8\xA2\xA8\xAB\xAE\xAF\xA5\xA4\xA8\xA8\n \x84\xA2\xAE\xE0\xA5\xE6\n"); /* reciv-ru */
        do {
            marked_mask = debug;
            choice_h = menu_choose(msgtext, 0x18, 0x10);
            if (choice_h != -1 && !edit_mode)
                debug ^= 1 << choice_h;
            if (choice_h != -1)
                editing = choice_h;
        } while (choice_h != -1 || edit_mode);
        break;
    case 4:
        cmd_code = 0x53;
        break;
    case 7:
        leave = 2;
q:     cmd_code = 0x1000;
        break;
    case 8:
        leave = 1;
        goto q;
    case 9:
        replay();
        break;
    }
}

void unit_orders_menu(int p, int u)
{
    int pos, ox, oy, key, ret, tileflags, ter;
    char ty;
    char keybuf[32];

    if (u == -1 || u == 0x80)
        return;
    pos = 0;
    msgtext[0] = 0;
    ox = unit_tab[p][u].x;
    oy = unit_tab[p][u].y;
    tileflags = map_flags(ox, oy);
    ter = terrain_at(ox, oy);
    strcat(msgtext, " \x8F\xE0\xAE\xAF\xE3\xE1\xE2\xA8\xE2\xEC space\n"); /* reciv-ru */
    keybuf[pos++] = ' ';
    dialog_dim = 0;
    if (unit_tab[p][u].type == 0) {
        strcat(msgtext, (tileflags & 1) ? " \x8E\xA1\xAE\xE1\xAD\xAE\xA2\xA0\xE2\xEC\xE1\xEF b\n" : " \x8E\xE1\xAD\xAE\xA2\xA0\xE2\xEC \xA3\xAE\xE0\xAE\xA4  b\n"); /* reciv-ru */
        keybuf[pos++] = 'b';
        if (!(tileflags & 8)) {
            strcat(msgtext, " \x84\xAE\xE0\xAE\xA3\xA0  r\n"); /* reciv-ru */
            keybuf[pos++] = 'r';
        } else if (!(tileflags & 0x10)) {
            if (has_tech(p, 0x31)) {
                strcat(msgtext, " \x86\xA5\xAB\xA5\xA7\xAD. \xA4\xAE\xE0\xAE\xA3\xA0  r\n"); /* reciv-ru */
                keybuf[pos++] = 'r';
            }
        }
        if (!(tileflags & 2)) {
            if (work_rules[ter].irrigate == -2) {
                strcat(msgtext, " \x8C\xA5\xAB\xA8\xAE\xE0\xA0\xE6\xA8\xEF \xA7\xA5\xAC\xAB\xA8"); /* reciv-ru */
                if (!build_ok(ox, oy))
                    dialog_dim |= 1 << pos;
            } else {
                if (work_rules[ter].irrigate >= 0) {
                    strcat(msgtext, " \x88\xA7\xAC\xA5\xAD\xA8\xE2\xEC"); /* reciv-ru */
                    strcat(msgtext, terrains[terrain_map[work_rules[ter].irrigate]].name);
                }
            }
            if (work_rules[ter].irrigate != -1) {
                keybuf[pos++] = 'i';
                strcat(msgtext, " \217i\n");
            }
        }
        if (!(tileflags & 4)) {
            if (work_rules[ter].mine <= -2)
                strcat(msgtext, " \x98\xA0\xE5\xE2\xEB "); /* reciv-ru */
            else {
                if (work_rules[ter].mine >= 0) {
                    strcat(msgtext, " \x88\xA7\xAC\xA5\xAD\xA8\xE2\xEC"); /* reciv-ru */
                    strcat(msgtext, terrains[terrain_map[work_rules[ter].mine]].name);
                }
            }
            if (work_rules[ter].mine != -1) {
                keybuf[pos++] = 'm';
                strcat(msgtext, " \217m\n");
            }
        }
        if (tileflags & 0x40) {
            strcat(msgtext, " \x93\xA1\xE0\xA0\xE2\xEC \xA7\xA0\xA3\xE0\xEF\xA7\xAD\xA5\xAD\xA8\xA5  p\n"); /* reciv-ru */
            keybuf[pos++] = 'p';
        }
    }
    if (unit_tab[p][u].type == 0) {
        strcat(msgtext, " \x91\xE2\xE0\xAE\xA8\xE2\xEC \x94\xAE\xE0\xE2  f\n"); /* reciv-ru */
        if (!has_tech(p, 0x2d))
            dialog_dim |= 1 << pos;
    } else if (utypes[unit_tab[p][u].type].domain == 0) {
        strcat(msgtext, " \x8E\xAA\xAE\xAF\xA0\xE2\xEC  f\n"); /* reciv-ru */
    }
    if (utypes[unit_tab[p][u].type].domain == 0)
        keybuf[pos++] = 'f';
    strcat(msgtext, " \x86\xA4\xA0\xE2\xEC w\n \x91\xE2\xE0\xA0\xA6   s\n \x88\xA4\xE2\xA8\n"); /* reciv-ru */
    keybuf[pos++] = 'w';
    keybuf[pos++] = 's';
    keybuf[pos++] = 'g';
    if (tileflags & 0x1e) {
        ty = unit_tab[p][u].type;
        if (ty < 0x1a && ty != 0xe) {
            strcat(msgtext, " \x90\xA0\xA7\xA1\xAE\xA9 P\n"); /* reciv-ru */
            keybuf[pos++] = 'P';
        }
    }
    if (tileflags & 1) {
        strcat(msgtext, " \x8F\xE0\xAE\xAF\xA8\xE1\xA0\xE2\xEC h\n"); /* reciv-ru */
        keybuf[pos++] = 'h';
    }
    ty = unit_tab[p][u].type;
    if (utypes[ty].w9 == 5 || ty == 0x17) {
        if (unit_tab[p][u].cargo != -1) {
            strcat(msgtext, " \x90\xA0\xA7\xA3\xE0\xE3\xA7\xA8\xE2\xEC u\n"); /* reciv-ru */
            keybuf[pos++] = 'u';
        }
    }
    strcat(msgtext, " \n \x90\xA0\xE1\xAF\xE3\xE1\xE2\xA8\xE2\xEC  D\n"); /* reciv-ru */
    keybuf[pos++] = 0;
    keybuf[pos++] = 'D';
    ret = menu_choose(msgtext, 0x48, 8);
    if (ret >= 0 && ret < pos)
        cmd_code = keybuf[ret];
    else
        cmd_code = -1;
}

void advisor_menu(void)
{
    int unused_word_c, choice_h;

    strcpy(msgtext, " \x91\xAE\xE1\xE2\xAE\xEF\xAD\xA8\xA5 (F1)\n \x82\xAE\xA5\xAD\xAD\xEB\xA9 \xE1\xAE\xA2\xA5\xE2\xAD\xA8\xAA (F2)\n \x91\xAE\xA2\xA5\xE2\xAD\xA8\xAA \xAF\xAE \xA8\xAD. \xA4\xA5\xAB\xA0\xAC (F3)\n \x8E\xA1\xE9\xA5\xA5 \xAF\xAE\xAB\xAE\xA6\xA5\xAD\xA8\xA5 (F4)\n"); /* reciv-ru */
    strcat(msgtext, " \x92\xAE\xE0\xA3.\x91\xAE\xA2\xA5\xE2\xAD\xA8\xAA (F5)\n \x93\xE7\xA5\xAD\xEB\xA9 \x91\xAE\xA2\xA5\xE2 (F6)\n"); /* reciv-ru */
    choice_h = menu_choose(msgtext, 0x70, 8);
    mouse_off();
    menu_end();
    mouse_on();
    menu_result = -1;
    switch (choice_h) {
    case 4:
        trade_rpt(civnow);
        break;
    case 1:
        if (xray_on)
            unit_table(civnow);
        else
            mil_rpt(civnow);
        break;
    case 2:
        spy_rpt(civnow);
        break;
    case 3:
        mood_rpt(civnow);
        break;
    case 0:
        city_rpt(civnow);
        break;
    case 5:
        sci_rpt(civnow);
        break;
    }
}

void world_menu(void)
{
    int unused_word_c, choice_h;

    if (!(game_flags & 0xfe00))
        dialog_dim = 0x20;
    strcpy(msgtext, " \x97\xE3\xA4\xA5\xE1\xA0 \x91\xA2\xA5\xE2\xA0 (F7)\n \x8B\xE3\xE7\xE8\xA8\xA5 5 \xA3\xAE\xE0\xAE\xA4\xAE\xA2 (F8)\n \x93\xE0\xAE\xA2\xA5\xAD\xEC \xE6\xA8\xA2\xA8\xAB\xA8\xA7\xA0\xE6\xA8\xA8 (F9)\n"); /* reciv-ru */
    strcat(msgtext, " \x8A\xA0\xE0\xE2\xA0 \x8C\xA8\xE0\xA0 (F10)\n \x84\xA5\xAC\xAE\xA3\xE0\xA0\xE4\xA8\xEF\n \x8A\xAE\xE1.\x8A\xAE\xE0\xA0\xA1\xAB\xEC\n"); /* reciv-ru */
    choice_h = menu_choose(msgtext, 0x90, 8);
    mouse_off();
    menu_end();
    mouse_on();
    menu_result = -1;
    switch (choice_h) {
    case 0:
        wonders();
        break;
    case 4:
        demographics(civnow);
        break;
    case 2:
        civ_score(civnow, 1L);
        break;
    case 1:
        top_five();
        break;
    case 3:
        world_map(1L);
        break;
    case 5:
        spaceship_inspect();
        break;
    }
}

void civilopedia_menu(void)
{
    int choice_h;

    strcpy(msgtext, " \x8F\xAE\xAB\xAD\xA0\xEF\n \x84\xAE\xE1\xE2\xA8\xA6\xA5\xAD\xA8\xEF \xE6\xA8\xA2\xA8\xAB\xA8\xA7\xA0\xE6\xA8\xA8\n \x90\xA0\xA7\xA2\xA8\xE2\xA8\xA5 \xA3\xAE\xE0\xAE\xA4\xAE\xA2\n \x82\xAE\xA9\xE1\xAA\xA0\n \x92\xA8\xAF\xEB \xE2\xA5\xE0\xE0\xA8\xE2\xAE\xE0\xA8\xA9\n"); /* reciv-ru */
    strcat(msgtext, " \x8F\xE0\xAE\xE7\xA5\xA5\n"); /* reciv-ru */
    choice_h = menu_choose(msgtext, 0xb6, 8);
    if (choice_h != -1) {
        civilopedia(0, choice_h - 1);
        menu_result = -1;
    } else
        menu_result = 1;
}
