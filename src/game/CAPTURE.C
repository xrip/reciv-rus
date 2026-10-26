/* Resident code segment 1459 (0000..0BE3): unit capture, spaceship loss,
   unrest, advance stealing, place names, ad screen wait and caravan trade
   routes. One complete MSC translation unit with its _DATA strings.
   Compiled with /AM /Gs /Oas. Names are provisional. */
#include <string.h>
#include "AUDIENCE.H"
#include "CITYVIEW.H"
#include "CIVWAR.H"
#include "CONTROL.H"
#include "CORE.H"
#include "ECONOMY.H"
#include "GAME.H"
#include "GLOBALS.H"
#include "MAP.H"
#include "NEWGAME.H"
#include "NEWS.H"
#include "RESIDENT.H"
#include "SPACESHP.H"
#include "TURN.H"
#include "UNITAI.H"
#include "UNITS.H"
#include "CAPTURE.H"

struct unit {
    unsigned long flags;
    unsigned char x, y;
    char f6, f7, f8;
    signed char f9, f10, owner;
    int pad12, f14;
    char pad16[6];
    unsigned char f22, slot[3];
    char pad26[2];
};
struct group {
    unsigned char x, y;
    char type;
    char pad3[7];
    signed char owner;
    char pad11;
};
struct unit_type {
    char pad0[20];
    int attack;
    char pad22[12];
};
struct relation {
    char a, b;
};
struct civ_type {
    char strings[48];
    int f0, f1, f2, f3, f4;
};

extern int debug;
extern char far place_name[][13];
extern struct unit townbuf[];
extern char armydata[];
#define unit_tab ((struct group (*)[128]) (armydata + 1))       /* units from x */
extern struct unit_type utypes[];
extern struct relation treaties[8][8];
extern struct civ_type civt[];

extern int rand_scaled(int);
extern int map_stack(int, int), wrap_x(int);

extern void sound(), disk_insert();
extern void map_mark(int, int), seen_map(int, int);
extern void unit_notify(), unit_defect();
extern void city_process();
extern void unit_home();
extern int kbhit(void), getch(void);
extern char *itoa(int, char *, int);

int incited = 0;                /* city taken by a diplomat: capture, never destroy */

/* Callers pass long arguments to int parameters; preserve their stack layout; NOLINTBEGIN(clang-diagnostic-deprecated-non-prototype) */
void capunit(a, b, c)
int a, b, c;
{
    int old, n, l, sy, sx, y, x, val, r;

    old = townbuf[b].owner;
    x = townbuf[b].x;
    y = townbuf[b].y;
    if (a == civnow) {
        last_met_turn[old] = -2;
        old = old;
    }
    if (old != civnow) {
        if (((townbuf[b].flags & 1) || townbuf[b].f9 == -1) && c == 0) {
            if ((townbuf[b].flags & 1) && city_count[old] > 4 && nation_ratings[civnow] < nation_ratings[old]) {
                if (civil_war(old) == 0) {
                    townbuf[b].owner = a;
                    r = central_city(old);
                    townbuf[r].f9 = -1;
                    townbuf[b].owner = old;
                }
            }
        }
    }
    if (townbuf[b].flags & 1)
        spaceship_lost(old);
    if (old == civnow)
        last_met_turn[a] = -1;
    val = clamp(unrest_chance(b), 0, money[old]);
    if (a == 0 && utypes[unit_tab[a][map_stack(x, y)].type].attack <= 2 && incited == 0) {
        vanish(b, x, y);
        townbuf[b].f7 = 0;
    } else {
        townbuf[b].f14 = 0;
        townbuf[b].f6 &= 0xae;
        townbuf[b].flags &= ~0x449;
        if (c == 0)
            townbuf[b].flags &= 0xaaaau >> rand_scaled(2);
        if (c == 0 || townbuf[b].f7 > 1)
            if (--townbuf[b].f7 == 0)
                vanish(b, x, y);
    }
    money[old] -= val;
    if (a)
        money[a] += val;
    if (townbuf[b].f7 != 0) {
        post_event(9, 4, a, townbuf[b].f22, x, y);
        clear_sites_near(a, 0, x, y, 4);
        townbuf[b].f6 &= 0x9b;
        n = 0;
        do {
            sx = wrap_x(spiral_x[n] + x);
            sy = spiral_y[n] + y;
            map_claim(sx + 80, sy, a, 2);
            ++n;
        } while (n < 20);
    }
    townbuf[b].owner = a;
    if (a == civnow || old == civnow) {
        map_mark(x, y);
        if (old == civnow)
            map_center(civnow, x, y);
        seen_map(x, y);
        strcpy(msgtext, civ_names[a]);
        strcat(msgtext, " \xAA.\xAA\xAE\xE0\xA0\xA1\xAB\xEC\n"); /* reciv-ru */
        append_place_name(b);
        strcat(msgtext, ". ");
        strcat(msgtext, itoa(val, nstr, 10));
        strcat(msgtext, "\xA2\xA5\xE0\xAD\xE3\xAB\xE1\xEF \xAD\xA0\xA7\xA0\xA4.\n"); /* reciv-ru */
        sound(civt[civ_tribe_of[a]].f3);
        if (debug & 8) {
            mouse_off();
            city_scene(b, -2);
            mouse_on();
            disk_insert(1, 1L);
            invasion_scene(a);
            disk_insert(0, 1L);
            sel_player();
        } else
            news_box(b);
        sound(1);
        townbuf[b].f8 = (a == civnow) ? 0 : townbuf[b].f7;
    } else if ((treaties[civnow][a].a & 0x40)
               || (treaties[civnow][old].a & 0x40)) {
        message_mode = 0;
        strcpy(msgtext, "\xE1\xE2\xE0\xAE\xA8\xE2\xA5\xAB\xEC\xE1\xE2\xA2\xAE \xAF\xE0\xA5\xE0\xA2\xA0\xAD\xAE.\n"); /* reciv-ru */
        strcat(msgtext, civ_names[a]);
        strcat(msgtext, townbuf[b].f7 ? "\xB0\xA2\xA7\xEF\xAB\xA8\n" : "\xB0\xAA\xA3\xC0\n\xA7\xAE\xAB\xAE\xE2\xA0\xB0\xE3\xAA\xE0\xA0\xA4\xA5\xAD\xAE\xB0\xC0\n"); /* reciv-ru */
        strcat(msgtext, civ_adjectives[old]);
        strcat(msgtext, "\x91\xAE\xAE\xA1\xE9\xC0\xB0\xE8\xAF\xA8\xAE\xAD\xC5\n"); /* reciv-ru */
        append_place_name(b);
        strcat(msgtext, ".\n");
        show_msg(msgtext, 100, 80);
    }
    steal_advance(a, old);
    map_refresh(a, x, y);
    n = 0;
    do {
        if (unit_tab[old][n].type != -1 && unit_tab[old][n].owner == b && (unit_tab[old][n].x != townbuf[b].x || unit_tab[old][n].y != townbuf[b].y || townbuf[b].f7 == 0))
            drop_u_stack(old, n);
        ++n;
    } while (n < 128);
    if (a == civnow && civ_gov[civnow] < 4)
        townbuf[b].f6 |= 1;
    if (townbuf[b].f7 != 0) {
        if (townbuf[b].f9 >= 0)
            --civ_units_ordered[old][townbuf[b].f9];
        if (a == civnow) {
            RECIV_SI(b * 28), city_process(b, 1L);
            sel_player();
        } else {
            townbuf[b].f9 = 0;
            townbuf[b].f9 = find_slot(a, b);
            if (townbuf[b].f9 >= 0)
                ++civ_units_ordered[a][townbuf[b].f9];
        }
    }
    kill_civ(old, a);
}

/* NOLINTEND(clang-diagnostic-deprecated-non-prototype) */

void spaceship_lost(int p)
{
    strcpy(msgtext, civ_adjectives[p]);
    strcat(msgtext, "\xB0\xA2\xA7\xEF\xAB\xA8\n\xA3\xAE\xE0\xC0\xB0"); /* reciv-ru */
    if (game_flags & (1 << (p + 8))) {
        strcat(msgtext, (game_flags & (1 << p)) ? "\xB0\xE3\xAD\xA8\xE7\xE2\xAE\xA6\xA8\xAB\xA8\n\xB0" : "\xB0\xA3\xAE\xE0\xAE\xA4\n\xB0"); /* reciv-ru */
        show_msg(msgtext, 100, 80);
    }
    game_flags &= ~(1 << p);
    game_flags &= ~(1 << (p + 8));
    reset_spaceship(p);
}

int unrest_chance(int i)
{
    int owner;

    owner = townbuf[i].owner;
    if (32000 / townbuf[i].f7 > money[owner])
        return money[owner] * townbuf[i].f7 / (citizens[owner] + 1);
    return money[owner] / (citizens[owner] + 1) * townbuf[i].f7;
}

void steal_advance(int a, int b)
{
    int num, top, opts[72], bv, i;
    unsigned int choice;

    if (a == 0)
        return;
    if (b == 0)
        return;
    num = 0;
    strcpy(msgtext, "\x82\xEB\xA1\xA5\xE0\xA8\xE2\xA5\xC0\xC0\xC0\n "); /* reciv-ru */
    bv = -1;
    i = 0x47;
    do {
        if (!has_tech(a, i) && has_tech(b, i)) {
            strcat(msgtext, advances[i].name);
            strcat(msgtext, "\n ");
            opts[num++] = i;
            if (advance_value(a, i) > bv) {
                bv = advance_value(a, i);
                top = i;
            }
        }
    } while (--i >= 0);
    if (num == 0)
        return;
    if (a == civnow) {
        do {
            if (mouse_mode == -1)
                choice = show_msg(msgtext, 80, 32);
            else
                choice = leader_menu(msgtext, 20, 139);
        } while (choice == -1);
        give_advance(a, opts[choice], b);
        return;
    }
    if (b == civnow) {
        strcpy(msgtext, civ_names[a]);
        strcat(msgtext, "\xB0\xE3\xA7\xAD\xC0\n"); /* reciv-ru */
        strcat(msgtext, advances[top].name);
        strcat(msgtext, ".\n");
        if (mouse_mode == -1)
            show_msg(msgtext, 80, 80);
        else
            leader_menu(msgtext, 20, 139);
    }
    give_advance(a, top, b);
    retarget(a, -1);
}

/* Non-prototype ABI calls below; NOLINTBEGIN(clang-diagnostic-deprecated-non-prototype) */
void append_place_name(int i)
{
    if (i == -1)
        strcat(msgtext, "\x8D\xA5\xE2"); /* reciv-ru */
    else
        copy_from_far(place_name[townbuf[i].f22], msgtext + strlen(msgtext), 13);
}

/* NOLINTEND(clang-diagnostic-deprecated-non-prototype) */

int ad_wait(void)
{
    /* reciv: the original is void, but callers use what it leaves in AX:
       ptr_y (poll_mouse ends by storing it) or kbhit's result, else the
       key (or its scan code). */
    int ax;
    repaint();
    do {
        poll_mouse();
        ax = ptr_y;
        if (mse_held)
            break;
        ax = kbhit();
    } while (!ax);
    if (!mse_held) {
        ax = getch();
        if (!ax)
            ax = getch();
    }
    return ax;
}

int rpath(int a, int b, int c)
{
    int r, s, t, w, j, k, best, n1;

    t = unit_tab[a][b].owner;
    r = map_dist(townbuf[c].x, townbuf[t].x, townbuf[c].y, townbuf[t].y) + 10;
    r *= townbuf[c].f10 + townbuf[t].f10;
    r /= 24;
    n1 = townbuf[t].x;
    if (region_key(townbuf[c].x, townbuf[c].y) == region_key(n1, n1))
        r /= 2;
    if (townbuf[c].owner == a)
        r /= 2;
    if (has_tech(townbuf[c].owner, 0x31))
        r -= r / 3;
    if (has_tech(townbuf[c].owner, 0x26))
        r -= r / 3;
    money[a] += r;
    civ_research_tally[a] += r;
    if (a == civnow) {
        strcpy(msgtext, trade_goods[b & 7]);
        strcat(msgtext, "\xB0\xAA\xA0\xE0\xA0\xA2\xA0\xAD\xB0\xA8\xA7\xB0"); /* reciv-ru */
        append_place_name(t);
        strcat(msgtext, "\n\xAF\xE0\xA8\xA1\xEB\xAB\xB0\xA2\xB0\xA3\xB0"); /* reciv-ru */
        append_place_name(c);
        strcat(msgtext, "\n\x8E\xE1\xAD\xAE\xA2\xA0\xAD \xE2\xAE\xE0\xA3\xAE\xA2\xEB\xA9 \xAF\xE3\xE2\xEC\n\x8F\xE0\xA8\xA1\xEB\xAB\xEC: $"); /* reciv-ru */
        strcat(msgtext, itoa(r, nstr, 10));
        strcat(msgtext, ".\n");
        show_msg(msgtext, 80, 80);
        status_draw();
    }
    drop_u_stack(a, b);
    s = townbuf[c].f10;
    if (townbuf[c].owner != a)
        s <<= 1;
    best = -1;
    for (j = 0; j < 3; j++) {
        k = townbuf[t].slot[j];
        if (k == c)
            return 1;
        if (k == 0xff)
            w = -1;
        else {
            w = townbuf[k].f10;
            if (townbuf[k].owner != a)
                w <<= 1;
        }
        if (w < s) {
            s = w;
            best = j;
        }
    }
    if (best != -1)
        townbuf[t].slot[best] = c;
    return 1;
}

/* City names, twelve characters padded with spaces; original far-data block. */
char far place_name[256][13] = {
    "\x90\xA8\xAC         ", "\x96\xA5\xA7\xA0\xE0\xA8\xEF     ", "\x8A\xA0\xE0\xE2\xA0\xA3\xA5\xAD    ", "\x8D\xA8\xAA\xAE\xAF\xAE\xAB\xA8\xE1   ", /* reciv-ru */
    "\x82\xA8\xA7\xA0\xAD\xE2\xA8\xEF    ", "\x81\xE0\xA0\xAD\xA4\xA8\xA7\xA8\xE3\xAC  ", "\x91\xA8\xE0\xA0\xAA\xE3\xA7\xEB    ", "\x80\xAD\xE2\xA8\xAE\xE5      ", /* reciv-ru */
    "\x8F\xA0\xAB\xEC\xAC\xA8\xE0\xA0    ", "\x91\xA8\xE0\xA5\xAD\xEB      ", "\x83\xAE\xE0\xA4\xA8\xAE\xAD     ", "\x92\xA8\xE0         ", /* reciv-ru */
    "\x88\xA5\xE0\xE3\xE1\xA0\xAB\xA8\xAC   ", "\x91\xA5\xAB\xA5\xE6\xA8\xEF     ", "\x90\xA0\xA2\xA5\xAD\xAD\xA0     ", "\x88\xA3\xAE\xE0\xEC\xA3\xE0\xA0\xA4   ", /* reciv-ru */
    "\x82\xA0\xA2\xA8\xAB\xAE\xAD     ", "\x98\xE3\xAC\xA5\xE0       ", "\x93\xE0\xEE\xAA        ", "\x8D\xA8\xAD\xA5\xA2\xA5\xE5     ", /* reciv-ru */
    "\x80\xE8\xE3\xE0        ", "\x9D\xAB\xAB\xA8\xAF\xA8      ", "\x80\xAA\xAA\xA0\xA4       ", "\x9D\xE0\xA8\xA4\xE3       ", /* reciv-ru */
    "\x8A\xA8\xE8         ", "\x8D\xA8\xAF\xAF\xE3\xE0      ", "\x98\xE3\xE0\xE3\xAF\xAF\xA0\xAA    ", "\x87\xA0\xE0\xA8\xAA\xE3\xAC     ", /* reciv-ru */
    "\x88\xA7\xA8\xA1\xA8\xEF      ", "\x8D\xA8\xAC\xE0\xE3\xA4      ", "\x80\xE0\xA1\xA5\xAB\xA0      ", "\x87\xA0\xAC\xA8\xEF       ", /* reciv-ru */
    "\x81\xA5\xE0\xAB\xA8\xAD      ", "\x8B\xA5\xA9\xAF\xE6\xA8\xA3     ", "\x83\xA0\xAC\xA1\xE3\xE0\xA3     ", "\x81\xE0\xA5\xAC\xA5\xAD      ", /* reciv-ru */
    "\x94\xE0\xA0\xAD\xAA\xE4\xE3\xE0\xE2   ", "\x81\xAE\xAD\xAD        ", "\x8D\xEE\xE0\xA1\xA5\xE0\xA3     ", "\x8A\xA5\xAB\xEC\xAD       ", /* reciv-ru */
    "\x83\xA0\xAD\xAD\xAE\xA2\xA5\xE0    ", "\x8C\xEE\xAD\xE5\xA5\xAD      ", "\x98\xE2\xE3\xE2\xA3\xA0\xE0\xA4    ", "\x83\xA5\xA4\xA5\xAB\xEC\xA1\xE3\xE0\xA3  ", /* reciv-ru */
    "\x91\xA0\xAB\xEC\xA7\xA1\xE3\xE0\xA3   ", "\x8A\xA5\xAD\xA8\xA3\xE1\xA1\xA5\xE0\xA3  ", "\x84\xA0\xE0\xE2\xAC\xE3\xAD\xA4    ", "\x81\xE0\xA0\xAD\xA4\xA5\xAD\xA1\xE3\xE0\xA3 ", /* reciv-ru */
    "\x88\xA1\xA8\xE1        ", "\x8C\xA5\xAC\xE4\xA8\xE1      ", "\x8E\xE0\xA8\xAA\xE1       ", "\x83\xA5\xAB\xA8\xAE\xAF\xAE\xAB\xA8\xE1  ", /* reciv-ru */
    "\x83\xA0\xA7\xA0        ", "\x80\xAB\xA5\xAA\xE1\xA0\xAD\xA4\xE0\xA8\xEF ", "\x81\xA8\xA1\xAB\xAE\xE1      ", "\x8A\xA0\xA8\xE0        ", /* reciv-ru */
    "\x8A\xAE\xAF\xE2\xAE\xE1      ", "\x85\xA4\xE4\xE3        ", "\x8F\xA8\xE2\xAE\xAD       ", "\x82\xA0\xA7\xA8\xE0\xA8\xE1     ", /* reciv-ru */
    "\x80\xE4\xE0\xA8\xA1\xA8\xE1     ", "\x8C\xA5\xAD\xA4\xA5\xE1      ", "\x92\xA0\xAD\xA8\xE1       ", "\x80\xA1\xA8\xA4\xAE\xE1      ", /* reciv-ru */
    "\x82\xA0\xE8\xA8\xAD\xA3\xE2\xAE\xAD   ", "\x8D\xEC\xEE \x89\xAE\xE0\xAA    ", "\x81\xAE\xE1\xE2\xAE\xAD      ", "\x94\xA8\xAB\xA0\xA4\xA5\xAB\xEC\xE4\xA8\xEF", /* reciv-ru */
    "\x80\xE2\xAB\xA0\xAD\xE2\xA0     ", "\x97\xA8\xAA\xA0\xA3\xAE      ", "\x81\xE3\xE4\xE4\xA0\xAB\xAE     ", "\x91\xA5\xAD\x8B\xE3\xE1      ", /* reciv-ru */
    "\x84\xA5\xE2\xE0\xAE\xA9\xE2     ", "\x8D\xAE\xA2\xEB\xA9 \x8E\xE0\xAB\xA5\xA0\xAD", "\x81\xA0\xAB\xE2\xA8\xAC\xAE\xE0    ", "\x84\xA5\xAD\xA2\xA5\xE0      ", /* reciv-ru */
    "\x96\xA8\xAD\xE6\xA8\xAD\xA0\xE2\xE2\xA8  ", "\x84\xA0\xAB\xAB\xA0\xE1      ", "\x8B\xAE\xE1 \x80\xAD\xA6\xA5\xAB\xA5\xE1 ", "\x8B\xA0\xE1 \x82\xA5\xA3\xA0\xE1   ", /* reciv-ru */
    "\x80\xE4\xA8\xAD\xEB       ", "\x91\xAF\xA0\xE0\xE2\xA0      ", "\x8A\xAE\xE0\xA8\xAD\xE4      ", "\x84\xA5\xAB\xEC\xE4\xEB      ", /* reciv-ru */
    "\x9D\xE0\xA5\xE2\xE0\xA8\xEF     ", "\x94\xA0\xE0\xE1\xA0\xAB\xAE\xE1    ", "\x80\xE0\xA3\xAE\xE1       ", "\x8C\xA8\xAA\xA5\xAD\xA8\xEF     ", /* reciv-ru */
    "\x83\xA5\xE0\xA0\xAA\xAB\xA5\xEF    ", "\x80\xAD\xE2\xA8\xAE\xE5      ", "\x9D\xE4\xA5\xE1        ", "\x90\xAE\xA4\xAE\xE1       ", /* reciv-ru */
    "\x8A\xAD\xAE\xE1\xE1       ", "\x92\xE0\xAE\xEF        ", "\x8F\xA5\xE0\xA3\xA0\xAC\xAE\xAD    ", "\x8C\xA8\xAB\xA5\xE2\xAE\xE1     ", /* reciv-ru */
    "\x84\xA5\xAB\xA8        ", "\x81\xAE\xAC\xA1\xA5\xA9      ", "\x8C\xA0\xA4\xE0\xA0\xE1      ", "\x81\xA0\xAD\xA3\xA0\xAB\xAE\xE0    ", /* reciv-ru */
    "\x8A\xA0\xAB\xEC\xAA\xE3\xE2\xE2\xA0   ", "\x8B\xA0\xE5\xAE\xE0       ", "\x8A\xA0\xE0\xA0\xE7\xA8      ", "\x8A\xAE\xAB\xE5\xA0\xAF\xE3\xE0    ", /* reciv-ru */
    "\x9F\xAF\xE3\xE0        ", "\x95\xA0\xA4\xA0\xE0\xA1\xA0\xA4    ", "\x81\xA5\xAD\xA3\xA0\xAB\xA5\xA7    ", "\x97\xA8\xE2\xE2\xA0\xA3\xAE\xAD    ", /* reciv-ru */
    "\x8F\xA5\xAD\xA4\xA6\xA0\xA1     ", "\x92\xAE\xAC\xE1\xAA       ", "\x88\xAD\xA4\xE3\xE1       ", "\x83\xA0\xAD\xA3        ", /* reciv-ru */
    "\x8C\xAE\xE1\xAA\xA2\xA0      ", "\x8F\xA5\xE2\xA5\xE0\xA1\xE3\xE0\xA3   ", "\x9F\xE0\xAE\xE1\xAB\xA0\xA2\xAB\xEC   ", "\x82\xAB\xA0\xA4\xA8\xAC\xA8\xE0    ", /* reciv-ru */
    "\x91\xAC\xAE\xAB\xA5\xAD\xE1\xAA    ", "\x90\xEF\xA7\xA0\xAD\xEC      ", "\x91\xA5\xA2\xA0\xE1\xE2\xAE\xAF\xAE\xAB\xEC ", "\x91\xA8\xAC\xA1\xA8\xE0\xE1\xAA    ", /* reciv-ru */
    "\x91\xA2\xA5\xE0\xA4\xAB\xAE\xA2\xE1\xAA  ", "\x8C\xE3\xE0\xAC\xA0\xAD\xE1\xAA    ", "\x82\xAB\xA0\xA4\xA8\xA2\xAE\xE1\xE2\xAE\xAA ", "\x8D\xAE\xA2\xA3\xAE\xE0\xAE\xA4    ", /* reciv-ru */
    "\x8A\xE0\xA0\xE1\xAD\xAE\xEF\xE0\xE1\xAA  ", "\x92\xA2\xA5\xE0\xEC       ", "\x91\xE2\xA0\xA2\xE0\xAE\xAF\xAE\xAB\xEC  ", "\x80\xE0\xE5\xA0\xAD\xA3\xA5\xAB\xEC\xE1\xAA ", /* reciv-ru */
    "\x8A\xA5\xAC\xA5\xE0\xAE\xA2\xAE    ", "\x93\xAB\xE3\xAD\xA4\xA8      ", "\x81\xA0\xAF\xA5\xA4\xA8      ", "\x95\xAB\xAE\xA1\xA0\xAD      ", /* reciv-ru */
    "\x88\xE1\xA0\xAD\xA4\xA2\xA0\xAD\xA0   ", "\x88\xAD\xE2\xAE\xAC\xA1\xA0     ", "\x8C\xAF\xAE\xAD\xA4\xAE      ", "\x8D\xA3\xAE\xAC        ", /* reciv-ru */
    "\x91\xA2\xA0\xA7\xA8       ", "\x92\xE3\xA3\xA5\xAB\xA0      ", "\x93\xAC\xE2\xA0\xE2\xA0      ", "\x93\xAC\xE4\xAE\xAB\xAE\xA7\xA8    ", /* reciv-ru */
    "\x88\xA1\xA0\xA1\xA0\xAD\xA0\xA3\xAE   ", "\x88\xA7\xA8\xAF\xA5\xA7\xA8     ", "\x80\xAC\xA0\xE2\xA8\xAA\xE3\xAB\xE3   ", "\x87\xA0\xAD\xA3\xE3\xA8\xAD     ", /* reciv-ru */
    "\x8F\xA0\xE0\xA8\xA6       ", "\x8E\xE0\xAB\xA5\xA0\xAD      ", "\x8B\xA8\xAE\xAD        ", "\x92\xE3\xE0         ", /* reciv-ru */
    "\x98\xA0\xE0\xE2\xE0\xA5\xA7     ", "\x81\xAE\xE0\xA4\xAE       ", "\x90\xA0\xA2\xA5\xAD       ", "\x80\xA2\xA8\xAD\xEC\xAE\xAD     ", /* reciv-ru */
    "\x8C\xA0\xE0\xE1\xA5\xAB\xEC     ", "\x83\xE0\xA5\xAD\xAE\xA1\xAB\xEC    ", "\x84\xA8\xA6\xAE\xAD       ", "\x80\xAC\xEC\xA5\xAD       ", /* reciv-ru */
    "\x98\xA5\xE0\xA1\xE3\xE0\xA3     ", "\x8F\xAE\xE2\xA8        ", "\x92\xE3\xAB\xE3\xA7\xA0      ", "\x81\xA0\xA9\xAE\xAD       ", /* reciv-ru */
    "\x92\xA5\xAD\xAE\xE7\xA8\xE2\xAB\xA0\xAD", "\x97\xA0\xE3\xE5\xE2\xA8\xEF     ", "\x97\xA0\xAF\xE3\xAB\xE2\xA5\xAF\xA8\xAA  ", "\x8A\xAE\xA0\xE2\xA5\xAF\xA8\xAA    ", /* reciv-ru */
    "\x80\xA9\xAE\xE6\xA8\xAD\xAA\xAE    ", "\x88\xE6\xA0\xAF\xA0\xAB\xA0\xAF\xA0   ", "\x88\xE6\xE2\xA0\xAF\xA0\xAC     ", "\x8C\xA8\xE6\xAE\xA0\xE1      ", /* reciv-ru */
    "\x92\xA0\xAA\xE3\xA1\xA0\xEF     ", "\x92\xAE\xAA\xA0\xAC\xA0\xAA     ", "\x92\xA5\xAF\xA5\xA7\xA8\xAD\xAE    ", "\x92\xA8\xAA\xAE\xAC\xA0\xAD     ", /* reciv-ru */
    "\x92\xAB\xA0\xE1\xAA\xA0\xAB\xA0    ", "\x8A\xE1\xA0\xAB\xEC\xE2\xAE\xAA\xA0\xAD  ", "\x8A\xE1\xA8\xAA\xA0\xAB\xA0\xAD\xA3\xAE  ", "\x87\xE3\xAC\xAF\xA0\xAD\xAA\xAE    ", /* reciv-ru */
    "\x8F\xA5\xAA\xA8\xAD       ", "\x98\xA0\xAD\xE5\xA0\xA9      ", "\x8A\xA0\xAD\xE2\xAE\xAD      ", "\x8D\xA0\xAD\xAA\xA8\xAD      ", /* reciv-ru */
    "\x96\xA7\xA8\xAD\xEC\xE2\xA0\xAE    ", "\x95\xA0\xAD\xA3\xE5\xAE\xE3     ", "\x92\xA5\xAD\xE6\xA7\xA8\xAD\xEC    ", "\x92\xA0\xE2\xE3\xAD\xA3      ", /* reciv-ru */
    "\x8C\xA0\xAA\xA0\xAE       ", "\x80\xAD\xEC\xEF\xAD\xA3      ", "\x98\xA0\xAD\xE2\xE3\xAD\xA3     ", "\x97\xA8\xAD\xA0\xAD       ", /* reciv-ru */
    "\x8A\xA0\xA9\xE4\xA5\xAD      ", "\x8D\xA8\xAD\xA3\xAF\xAE      ", "\x8F\xA0\xAE\xE2\xA8\xAD\xA3     ", "\x9F\xAD\xE7\xAE\xE3       ", /* reciv-ru */
    "\x8B\xAE\xAD\xA4\xAE\xAD      ", "\x8A\xAE\xA2\xA5\xAD\xE2\xE0\xA8    ", "\x81\xA8\xE0\xAC\xA8\xAD\xA3\xA5\xAC   ", "\x84\xE3\xA2\xE0        ", /* reciv-ru */
    "\x8D\xAE\xE2\xA8\xAD\xA3\xA5\xAC    ", "\x89\xAE\xE0\xAA        ", "\x8B\xA8\xA2\xA5\xE0\xAF\xE3\xAB\xEC   ", "\x81\xE0\xA0\xA9\xE2\xAE\xAD     ", /* reciv-ru */
    "\x8E\xAA\xE1\xE4\xAE\xE0\xA4     ", "\x83\xAB\xA0\xA7\xA3\xAE      ", "\x9D\xAA\xE1\xA5\xE2\xA5\xE0     ", "\x8A\xA5\xAC\xA1\xE0\xA8\xA4\xA6    ", /* reciv-ru */
    "\x83\xA0\xE1\xE2\xA8\xAD\xA3\xE1    ", "\x8A\xA0\xAD\xE2\xA5\xE0\xA1\xA8    ", "\x81\xA0\xAD\xA1\xE3\xE0\xA8     ", "\x8D\xEC\xEE\xAA\xA0\xE1\xAB     ", /* reciv-ru */
    "\x91\xA0\xAC\xA0\xE0\xAA\xA0\xAD\xA4   ", "\x81\xE3\xE5\xA0\xE0\xA0      ", "\x8D\xA8\xE8\xA0\xAF\xE3\xE0     ", "\x8A\xA0\xE0\xA0\xAA\xE3\xAC     ", /* reciv-ru */
    "\x8A\xA0\xE8\xA3\xA0\xE0      ", "\x92\xA0\xA1\xE0\xA8\xA7      ", "\x80\xAB\xA5\xAF\xAF\xAE      ", "\x8A\xA0\xA1\xE3\xAB       ", /* reciv-ru */
    "\x8E\xE0\xAC\xE3\xA7       ", "\x81\xA0\xE1\xE0\xA0       ", "\x95\xA0\xAD\xA1\xA0\xAB\xEB\xAA    ", "\x95\xAE\xE0\xA0\xA7\xA0\xAD     ", /* reciv-ru */
    "\x98\xA0\xAD\xE2\xE3       ", "\x8A\xA0\xA7\xA0\xAD\xEC      ", "\x8A\xA8\xAD\xE1\xA5\xA9      ", "\x8A\xA5\xE0\xAC\xA0\xAD      ", /* reciv-ru */
    "\x8C\xA5\xAA\xAA\xA0       ", "\x8D\xA0\xAF\xAB\xEC       ", "\x91\xA8\xA4\xAE\xAD       ", "\x92\xA8\xE0         ", /* reciv-ru */
    "\x92\xA0\xE0\xE1\xE3\xE1      ", "\x88\xE1\xE1\xE3\xE1       ", "\x8A\xE3\xAD\xA0\xAA\xE1\xA0     ", "\x8A\xE0\xA5\xAC\xAE\xAD\xA0     ", /* reciv-ru */
    "\x8A\xA0\xAD\xAD        ", "\x8A\xA0\xAF\xE3\xEF       ", "\x92\xE3\xE0\xA8\xAD       ", "\x83\xA5\xAD\xE3\xEF       ", /* reciv-ru */
    "\x93\xE2\xA8\xAA\xA0       ", "\x8A\xE0\xA8\xE2        ", "\x84\xA0\xAC\xA0\xE1\xE1\xAA     ", "\x82\xA5\xE0\xAE\xAD\xA0      ", /* reciv-ru */
    "\x91\xA0\xAB\xEF\xAC\xA8\xE1     ", "\x8B\xA8\xE1\xE1\xA0\xA1\xAE\xAD    ", "\x83\xA0\xAC\xA1\xE3\xE0\xA3     ", "\x8F\xE0\xA0\xA3\xA0       ", /* reciv-ru */
    "\x91\xA0\xAB\xEC\xA7\xA1\xE3\xE0\xA3   ", "\x81\xA5\xE0\xA3\xA5\xAD      ", "\x82\xA5\xAD\xA5\xE6\xA8\xEF     ", "\x8C\xA8\xAB\xA0\xAD       ", /* reciv-ru */
    "\x8A\xA5\xAD\xE2        ", "\x8F\xA8\xA7\xA0        ", "\x8A\xAE\xE0\xA4\xAE\xA1\xA0     ", "\x91\xA5\xA2\xA8\xAB\xEC\xEF     ", /* reciv-ru */
    "\x84\xE3\xA1\xAB\xA8\xAD      ", "\x92\xAE\xE0\xAE\xAD\xE2\xAE     ", "\x8C\xA5\xAB\xEC\xA1\xE3\xE0\xAD    ", "\x91\xA8\xA4\xAD\xA5\xA9      " /* reciv-ru */
};
