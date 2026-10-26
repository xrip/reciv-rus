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
        strcat(msgtext, TRANSLATE(" capture\n", "RU")); /* reciv-ru */
        append_place_name(b);
        strcat(msgtext, TRANSLATE(". ", "RU")); /* reciv-ru */
        strcat(msgtext, itoa(val, nstr, 10));
        strcat(msgtext, TRANSLATE(" gold\npieces plundered.\n", "RU")); /* reciv-ru */
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
        strcpy(msgtext, TRANSLATE("Spies report:\n", "RU")); /* reciv-ru */
        strcat(msgtext, civ_names[a]);
        strcat(msgtext, townbuf[b].f7 ? TRANSLATE(" capture\nthe ", "RU") : TRANSLATE(" destroy\nthe ", "RU")); /* reciv-ru */
        strcat(msgtext, civ_adjectives[old]);
        strcat(msgtext, TRANSLATE(" city\nof ", "RU")); /* reciv-ru */
        append_place_name(b);
        strcat(msgtext, TRANSLATE(".\n", "RU")); /* reciv-ru */
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
    strcat(msgtext, TRANSLATE(" spaceship\n", "RU")); /* reciv-ru */
    if (game_flags & (1 << (p + 8))) {
        strcat(msgtext, (game_flags & (1 << p)) ? TRANSLATE("returns to Earth.\n", "RU") : TRANSLATE("construction cancelled.\n", "RU")); /* reciv-ru */
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
    strcpy(msgtext, TRANSLATE("Select one...\n ", "RU")); /* reciv-ru */
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
        strcat(msgtext, TRANSLATE(" take\n", "RU")); /* reciv-ru */
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
        strcat(msgtext, TRANSLATE("NONE", "RU")); /* reciv-ru */
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
        strcat(msgtext, TRANSLATE(" caravan from ", "RU")); /* reciv-ru */
        append_place_name(t);
        strcat(msgtext, TRANSLATE("\narrives in ", "RU")); /* reciv-ru */
        append_place_name(c);
        strcat(msgtext, TRANSLATE("\nTrade route established\nRevenue: $", "RU")); /* reciv-ru */
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
    TRANSLATE("Rome        ", "RU"), TRANSLATE("Caesarea    ", "RU"), TRANSLATE("Carthage    ", "RU"), TRANSLATE("Nicopolis   ", "RU"), /* reciv-ru */
    TRANSLATE("Byzantium   ", "RU"), TRANSLATE("Brundisium  ", "RU"), TRANSLATE("Syracuse    ", "RU"), TRANSLATE("Antioch     ", "RU"), /* reciv-ru */
    TRANSLATE("Palmyra     ", "RU"), TRANSLATE("Cyrene      ", "RU"), TRANSLATE("Gordion     ", "RU"), TRANSLATE("Tyrus       ", "RU"), /* reciv-ru */
    TRANSLATE("Jerusalem   ", "RU"), TRANSLATE("Seleucia    ", "RU"), TRANSLATE("Ravenna     ", "RU"), TRANSLATE("Artaxata    ", "RU"), /* reciv-ru */
    TRANSLATE("Babylon     ", "RU"), TRANSLATE("Sumer       ", "RU"), TRANSLATE("Uruk        ", "RU"), TRANSLATE("Nineveh     ", "RU"), /* reciv-ru */
    TRANSLATE("Ashur       ", "RU"), TRANSLATE("Ellipi      ", "RU"), TRANSLATE("Akkad       ", "RU"), TRANSLATE("Eridu       ", "RU"), /* reciv-ru */
    TRANSLATE("Kish        ", "RU"), TRANSLATE("Nippur      ", "RU"), TRANSLATE("Shuruppak   ", "RU"), TRANSLATE("Zariqum     ", "RU"), /* reciv-ru */
    TRANSLATE("Izibia      ", "RU"), TRANSLATE("Nimrud      ", "RU"), TRANSLATE("Arbela      ", "RU"), TRANSLATE("Zamua       ", "RU"), /* reciv-ru */
    TRANSLATE("Berlin      ", "RU"), TRANSLATE("Leipzig     ", "RU"), TRANSLATE("Hamburg     ", "RU"), TRANSLATE("Bremen      ", "RU"), /* reciv-ru */
    TRANSLATE("Frankfurt   ", "RU"), TRANSLATE("Bonn        ", "RU"), TRANSLATE("Nuremberg   ", "RU"), TRANSLATE("Cologne     ", "RU"), /* reciv-ru */
    TRANSLATE("Hannover    ", "RU"), TRANSLATE("Munich      ", "RU"), TRANSLATE("Stuttgart   ", "RU"), TRANSLATE("Heidelburg  ", "RU"), /* reciv-ru */
    TRANSLATE("Salzburg    ", "RU"), TRANSLATE("Konigsberg  ", "RU"), TRANSLATE("Dortmund    ", "RU"), TRANSLATE("Brandenburg ", "RU"), /* reciv-ru */
    TRANSLATE("Thebes      ", "RU"), TRANSLATE("Memphis     ", "RU"), TRANSLATE("Oryx        ", "RU"), TRANSLATE("Heliopolis  ", "RU"), /* reciv-ru */
    TRANSLATE("Gaza        ", "RU"), TRANSLATE("Alexandria  ", "RU"), TRANSLATE("Byblos      ", "RU"), TRANSLATE("Cairo       ", "RU"), /* reciv-ru */
    TRANSLATE("Coptos      ", "RU"), TRANSLATE("Edfu        ", "RU"), TRANSLATE("Pithom      ", "RU"), TRANSLATE("Busiris     ", "RU"), /* reciv-ru */
    TRANSLATE("Athribis    ", "RU"), TRANSLATE("Mendes      ", "RU"), TRANSLATE("Tanis       ", "RU"), TRANSLATE("Abydos      ", "RU"), /* reciv-ru */
    TRANSLATE("Washington  ", "RU"), TRANSLATE("New York    ", "RU"), TRANSLATE("Boston      ", "RU"), TRANSLATE("Philadelphia", "RU"), /* reciv-ru */
    TRANSLATE("Atlanta     ", "RU"), TRANSLATE("Chicago     ", "RU"), TRANSLATE("Buffalo     ", "RU"), TRANSLATE("St.Louis    ", "RU"), /* reciv-ru */
    TRANSLATE("Detroit     ", "RU"), TRANSLATE("New Orleans ", "RU"), TRANSLATE("Baltimore   ", "RU"), TRANSLATE("Denver      ", "RU"), /* reciv-ru */
    TRANSLATE("Cincinnati  ", "RU"), TRANSLATE("Dallas      ", "RU"), TRANSLATE("Los Angeles ", "RU"), TRANSLATE("Las Vegas   ", "RU"), /* reciv-ru */
    TRANSLATE("Athens      ", "RU"), TRANSLATE("Sparta      ", "RU"), TRANSLATE("Corinth     ", "RU"), TRANSLATE("Delphi      ", "RU"), /* reciv-ru */
    TRANSLATE("Eretria     ", "RU"), TRANSLATE("Pharsalos   ", "RU"), TRANSLATE("Argos       ", "RU"), TRANSLATE("Mycenae     ", "RU"), /* reciv-ru */
    TRANSLATE("Herakleia   ", "RU"), TRANSLATE("Antioch     ", "RU"), TRANSLATE("Ephesos     ", "RU"), TRANSLATE("Rhodes      ", "RU"), /* reciv-ru */
    TRANSLATE("Knossos     ", "RU"), TRANSLATE("Troy        ", "RU"), TRANSLATE("Pergamon    ", "RU"), TRANSLATE("Miletos     ", "RU"), /* reciv-ru */
    TRANSLATE("Delhi       ", "RU"), TRANSLATE("Bombay      ", "RU"), TRANSLATE("Madras      ", "RU"), TRANSLATE("Bangalore   ", "RU"), /* reciv-ru */
    TRANSLATE("Calcutta    ", "RU"), TRANSLATE("Lahore      ", "RU"), TRANSLATE("Karachi     ", "RU"), TRANSLATE("Kolhapur    ", "RU"), /* reciv-ru */
    TRANSLATE("Jaipur      ", "RU"), TRANSLATE("Hyderabad   ", "RU"), TRANSLATE("Bengal      ", "RU"), TRANSLATE("Chittagong  ", "RU"), /* reciv-ru */
    TRANSLATE("Punjab      ", "RU"), TRANSLATE("Dacca       ", "RU"), TRANSLATE("Indus       ", "RU"), TRANSLATE("Ganges      ", "RU"), /* reciv-ru */
    TRANSLATE("Moscow      ", "RU"), TRANSLATE("Leningrad   ", "RU"), TRANSLATE("Kiev        ", "RU"), TRANSLATE("Minsk       ", "RU"), /* reciv-ru */
    TRANSLATE("Smolensk    ", "RU"), TRANSLATE("Odessa      ", "RU"), TRANSLATE("Sevastopol  ", "RU"), TRANSLATE("Tblisi      ", "RU"), /* reciv-ru */
    TRANSLATE("Sverdlovsk  ", "RU"), TRANSLATE("Yakutsk     ", "RU"), TRANSLATE("Vladivostok ", "RU"), TRANSLATE("Novograd    ", "RU"), /* reciv-ru */
    TRANSLATE("Krasnoyarsk ", "RU"), TRANSLATE("Riga        ", "RU"), TRANSLATE("Rostov      ", "RU"), TRANSLATE("Astrakhan   ", "RU"), /* reciv-ru */
    TRANSLATE("Zimbabwe    ", "RU"), TRANSLATE("Ulundi      ", "RU"), TRANSLATE("Bapedi      ", "RU"), TRANSLATE("Hlobane     ", "RU"), /* reciv-ru */
    TRANSLATE("Isandhlwana ", "RU"), TRANSLATE("Intombe     ", "RU"), TRANSLATE("Mpondo      ", "RU"), TRANSLATE("Ngome       ", "RU"), /* reciv-ru */
    TRANSLATE("Swazi       ", "RU"), TRANSLATE("Tugela      ", "RU"), TRANSLATE("Umtata      ", "RU"), TRANSLATE("Umfolozi    ", "RU"), /* reciv-ru */
    TRANSLATE("Ibabanago   ", "RU"), TRANSLATE("Isipezi     ", "RU"), TRANSLATE("Amatikulu   ", "RU"), TRANSLATE("Zunguin     ", "RU"), /* reciv-ru */
    TRANSLATE("Paris       ", "RU"), TRANSLATE("Orleans     ", "RU"), TRANSLATE("Lyons       ", "RU"), TRANSLATE("Tours       ", "RU"), /* reciv-ru */
    TRANSLATE("Chartres    ", "RU"), TRANSLATE("Bordeaux    ", "RU"), TRANSLATE("Rouen       ", "RU"), TRANSLATE("Avignon     ", "RU"), /* reciv-ru */
    TRANSLATE("Marseilles  ", "RU"), TRANSLATE("Grenoble    ", "RU"), TRANSLATE("Dijon       ", "RU"), TRANSLATE("Amiens      ", "RU"), /* reciv-ru */
    TRANSLATE("Cherbourg   ", "RU"), TRANSLATE("Poitiers    ", "RU"), TRANSLATE("Toulouse    ", "RU"), TRANSLATE("Bayonne     ", "RU"), /* reciv-ru */
    TRANSLATE("Tenochtitlan", "RU"), TRANSLATE("Chiauhtia   ", "RU"), TRANSLATE("Chapultepec ", "RU"), TRANSLATE("Coatepec    ", "RU"), /* reciv-ru */
    TRANSLATE("Ayotzinco   ", "RU"), TRANSLATE("Itzapalapa  ", "RU"), TRANSLATE("Iztapam     ", "RU"), TRANSLATE("Mitxcoac    ", "RU"), /* reciv-ru */
    TRANSLATE("Tacubaya    ", "RU"), TRANSLATE("Tecamac     ", "RU"), TRANSLATE("Tepezinco   ", "RU"), TRANSLATE("Ticoman     ", "RU"), /* reciv-ru */
    TRANSLATE("Tlaxcala    ", "RU"), TRANSLATE("Xaltocan    ", "RU"), TRANSLATE("Xicalango   ", "RU"), TRANSLATE("Zumpanco    ", "RU"), /* reciv-ru */
    TRANSLATE("Peking      ", "RU"), TRANSLATE("Shanghai    ", "RU"), TRANSLATE("Canton      ", "RU"), TRANSLATE("Nanking     ", "RU"), /* reciv-ru */
    TRANSLATE("Tsingtao    ", "RU"), TRANSLATE("Hangchow    ", "RU"), TRANSLATE("Tientsin    ", "RU"), TRANSLATE("Tatung      ", "RU"), /* reciv-ru */
    TRANSLATE("Macao       ", "RU"), TRANSLATE("Anyang      ", "RU"), TRANSLATE("Shantung    ", "RU"), TRANSLATE("Chinan      ", "RU"), /* reciv-ru */
    TRANSLATE("Kaifeng     ", "RU"), TRANSLATE("Ningpo      ", "RU"), TRANSLATE("Paoting     ", "RU"), TRANSLATE("Yangchow    ", "RU"), /* reciv-ru */
    TRANSLATE("London      ", "RU"), TRANSLATE("Coventry    ", "RU"), TRANSLATE("Birmingham  ", "RU"), TRANSLATE("Dover       ", "RU"), /* reciv-ru */
    TRANSLATE("Nottingham  ", "RU"), TRANSLATE("York        ", "RU"), TRANSLATE("Liverpool   ", "RU"), TRANSLATE("Brighton    ", "RU"), /* reciv-ru */
    TRANSLATE("Oxford      ", "RU"), TRANSLATE("Reading     ", "RU"), TRANSLATE("Exeter      ", "RU"), TRANSLATE("Cambridge   ", "RU"), /* reciv-ru */
    TRANSLATE("Hastings    ", "RU"), TRANSLATE("Canterbury  ", "RU"), TRANSLATE("Banbury     ", "RU"), TRANSLATE("Newcastle   ", "RU"), /* reciv-ru */
    TRANSLATE("Samarkand   ", "RU"), TRANSLATE("Bokhara     ", "RU"), TRANSLATE("Nishapur    ", "RU"), TRANSLATE("Karakorum   ", "RU"), /* reciv-ru */
    TRANSLATE("Kashgar     ", "RU"), TRANSLATE("Tabriz      ", "RU"), TRANSLATE("Aleppo      ", "RU"), TRANSLATE("Kabul       ", "RU"), /* reciv-ru */
    TRANSLATE("Ormuz       ", "RU"), TRANSLATE("Basra       ", "RU"), TRANSLATE("Khanbalyk   ", "RU"), TRANSLATE("Khorasan    ", "RU"), /* reciv-ru */
    TRANSLATE("Shangtu     ", "RU"), TRANSLATE("Kazan       ", "RU"), TRANSLATE("Quinsay     ", "RU"), TRANSLATE("Kerman      ", "RU"), /* reciv-ru */
    TRANSLATE("Mecca       ", "RU"), TRANSLATE("Naples      ", "RU"), TRANSLATE("Sidon       ", "RU"), TRANSLATE("Tyre        ", "RU"), /* reciv-ru */
    TRANSLATE("Tarsus      ", "RU"), TRANSLATE("Issus       ", "RU"), TRANSLATE("Cunaxa      ", "RU"), TRANSLATE("Cremona     ", "RU"), /* reciv-ru */
    TRANSLATE("Cannae      ", "RU"), TRANSLATE("Capua       ", "RU"), TRANSLATE("Turin       ", "RU"), TRANSLATE("Genoa       ", "RU"), /* reciv-ru */
    TRANSLATE("Utica       ", "RU"), TRANSLATE("Crete       ", "RU"), TRANSLATE("Damascus    ", "RU"), TRANSLATE("Verona      ", "RU"), /* reciv-ru */
    TRANSLATE("Salamis     ", "RU"), TRANSLATE("Lisbon      ", "RU"), TRANSLATE("Hamburg     ", "RU"), TRANSLATE("Prague      ", "RU"), /* reciv-ru */
    TRANSLATE("Salzburg    ", "RU"), TRANSLATE("Bergen      ", "RU"), TRANSLATE("Venice      ", "RU"), TRANSLATE("Milan       ", "RU"), /* reciv-ru */
    TRANSLATE("Ghent       ", "RU"), TRANSLATE("Pisa        ", "RU"), TRANSLATE("Cordoba     ", "RU"), TRANSLATE("Seville     ", "RU"), /* reciv-ru */
    TRANSLATE("Dublin      ", "RU"), TRANSLATE("Toronto     ", "RU"), TRANSLATE("Melbourne   ", "RU"), TRANSLATE("Sydney      ", "RU") /* reciv-ru */
};
