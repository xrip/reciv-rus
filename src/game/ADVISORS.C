/* Overlay 14: advisor and city-status reports. Complete MSC 5.1 TU;
   CODE 1B72, DATA [473C,4A16), CONST [643A,643E). */
#include <string.h>
#include <stdlib.h>
#include "CIV.H"
#include "CONTROL.H"
#include "DIALOG.H"
#include "GAME.H"
#include "GLOBALS.H"
#include "RESIDENT.H"
#include "TURN.H"
#include "ADVISORS.H"
#include "CAPTURE.H"
#include "CITYMGT.H"
#include "CORE.H"
#include "DRAW.H"
#include "UNITS.H"
#include "ECONOMY.H"
#include "MESSAGE.H"

extern unsigned char far advance_owners[][2];

extern void label_with_style(int, int, int);

void advisor_open(char *title, int mode)
{
    mouse_off();
    menu_begin();
    advisor_header(title, mode);
}

void advisor_header(char *title, int mode)
{
    rect_fill(scrbuf, 0, 0, 320, 200, mode);
    draw_text(title, 160, 2, 15);
    switch (civ_gov[civnow] / 2) {
    case 0:
        strcpy(msgtext, "\x83\xAE\xE1-\xA2\xAE"); /* reciv-ru */
        break;
    case 1:
        strcpy(msgtext, "\x96\xA0\xE0\xE1\xE2\xA2\xAE"); /* reciv-ru */
        break;
    case 2:
        strcpy(msgtext, "\x90\xA5\xE1\xAF\xE3\xA1\xAB\xA8\xAA\xA0"); /* reciv-ru */
        break;
    }
    strcat(msgtext, ": "); /* reciv-ru */
    strcat(msgtext, civ_names[civnow]);
    draw_text(msgtext, 160, 10, 15);

    strcpy(msgtext, leader_titles[civ_gov[civnow]]);
    strcat(msgtext, " ");
    strcat(msgtext, leader_names[civnow]);
    strcat(msgtext, ": ");
    format_year();
    draw_text(msgtext, 160, 18, 15);
}

void advisor_close(void)
{
    ad_wait();
    menu_end();
    mouse_on();
}

void sci_rpt(int civ)
{                               /* science report */
    int x, x0, count, diff, cost, y, bw, i, step;

    advisor_open("\x8E\xE2\xE7\xA5\xE2 \xAE \xAD\xA0\xE3\xAA\xA5", 1); /* reciv-ru */
    x0 = 0;
    diff = cur_tier * 2 + cost_penalty + 6;
    if (diff < 11 - tech_count[civ]) {
        diff = 11 - tech_count[civ];
    }
    cost = tech_count[civ] * ((year < 0) ? 1 : 2) * diff;
    step = clamp(0x980 / (cost + 1), 1, 0x40);
    bw = clamp((step * cost) / 8 + 8, 8, 0x130);
    x = 160 - bw / 2;
    if (x > 40) {
        blit(tile_page, 0x118, 0x8c, 40, 60, scrbuf, 0x116, 2);
    }
    rect_fill(scrbuf, x, 25, bw, 16, 9);

    for (i = 0; i < clamp(civ_research_tally[civ], 0, cost); i++) {
        label_with_style(12, x + x0 / 8, 0x21);
        x0 += step;
    }

    y = 26;
    if (selected_index != -1) {
        strcpy(msgtext, "\x88\xE1\xE1\xAB\xA5\xA4\xE3\xA5\xE2\xE1\xEF "); /* reciv-ru */
        strcat(msgtext, advances[selected_index].name);
        draw_text(msgtext, 161, 26, 0);
        draw_text(msgtext, 160, 26, 15);
    }

    y = 42;
    count = 0;
    for (i = 0; i < 72; i++) {
        if (!has_tech(civ, i))
            continue;
        strcpy(msgtext, advances[i].name);
        draw_string(msgtext, (count % 3) * 100 + 8, y, ((advance_owners[i][0] & 7) == civ) ? 15 : 11);
        count++;
        if (count % 3 == 0) {
            y += 7;
        }
        if (y > 192)
            break;
    }

    advisor_close();
}

extern struct unit_type utypes[];
extern int far unit_losses[8][28];

void mil_rpt(int civ)
{                               /* military status report */
    int type, y, n;

    advisor_open("\x91\xAE\xE1\xE2\xAE\xEF\xAD\xA8\xA5 \xA2\xAE\xA9\xE1\xAA", 1); /* reciv-ru */
    n = 0;
    y = 32;
    for (type = 0; type < 28; type++) {
        if (forces_present[civ][type] <= 0 && civ_units_ordered[civ][type] <= 0)
            continue;
        draw_line(36, y - 2, 319, y - 2, 9);
        label_with_style((civ << 5) + type + 64, ((n++ & 1) << 4) + 2, y - 4);
        draw_string(utypes[type].name, 36, y, 15);
        strcpy(msgtext, "(");
        strcat(msgtext, itoa(utypes[type].attack, nstr, 10));
        strcat(msgtext, "/");
        strcat(msgtext, itoa(utypes[type].defense, nstr, 10));
        strcat(msgtext, "/");
        strcat(msgtext, itoa(utypes[type].moves, nstr, 10));
        strcat(msgtext, ")");
        draw_string(msgtext, 112, y, 11);
        if (forces_present[civ][type] > 0) {
            msgtext[0] = 0;
            strcat(msgtext, itoa(forces_present[civ][type], nstr, 10));
            strcat(msgtext, " \xA0\xAA\xE2\xA8\xA2."); /* reciv-ru */
            draw_string(msgtext, 168, y, 15);
        }
        if (civ_units_ordered[civ][type] > 0) {
            msgtext[0] = 0;
            strcat(msgtext, itoa(civ_units_ordered[civ][type], nstr, 10));
            strcat(msgtext, " \xE4\xAE\xE0\xAC\xA8\xE0\xE3\xA5\xE2\xE1\xEF"); /* reciv-ru */
            draw_string(msgtext, 232, y, 11);
        }
        y += 9;
    }
    blit(tile_page, 0xA0, 0x8C, 40, 60, scrbuf, 0x116, 2);
    advisor_close();
    military_losses(civ);
}

void military_losses(int civ)
{
    int x, any, y, i, j, n;

    advisor_open("\x8F\xAE\xE2\xA5\xE0\xA8 \xA2\xAE\xA9\xE1\xAA", 4); /* reciv-ru */
    n = 0;
    y = 28;
    for (i = 0; i < 27; i++) {
        any = 0;
        for (j = 0; j < 8; j++) {
            if (unit_losses[j][i] != 0) {
                any = 1;
            }
        }
        if (!any)
            continue;
        draw_line(36, y - 1, 319, y - 1, 8);
        label_with_style((civ << 5) + i + 64, ((n++ & 1) << 4) + 2, y - 4);
        draw_string(utypes[i].name, 36, y, 15);
        x = 104;
        if (unit_losses[civ][i] != 0) {
            msgtext[0] = 0;
            strcat(msgtext, itoa(unit_losses[civ][i], nstr, 10));
            draw_string(msgtext, 104, y, 12);
        }
        for (j = 1; j < 8; j++) {
            if (j == civ)
                continue;
            x += 28;
            if (unit_losses[j][i] != 0) {
                msgtext[0] = 0;
                strcat(msgtext, itoa(unit_losses[j][i], nstr, 10));
                draw_string(msgtext, x, y, civ_colours[j]);
            }
        }
        y += 7;
    }
    draw_box(100, 25, 18, y - 25, civ_colours[civnow]);
    advisor_close();
}

extern struct city townbuf[];

extern int city_process(int, int);

void trade_rpt(int civ)
{                               /* trade report */
    int tot_inc, tc, dummy, rate, i, turn, city, y, building_costs[24];

    rate = cur_tier / 2;
    if (has_tech(civ, 34))
        rate++;
    if (has_tech(civ, 37))
        rate++;

    advisor_open("\x92\xAE\xE0\xA3\xAE\xA2\xAB\xEF", 2); /* reciv-ru */
    blit(tile_page, 200, 140, 40, 60, scrbuf, 0x116, 2);
    draw_string("\x92\xAE\xE0\xA3\xAE\xA2\xAB\xEF", 8, 32, 15); /* reciv-ru */

    i = tot_inc = tc = 0;
    for (i = 0; i < 24; i++) {
        building_costs[i] = 0;
    }
    y = 40;

    for (city = 0; city < 128; city++) {
        if (townbuf[city].status != -1 && townbuf[city].owner == civ) {
            if (y >= 180) {
                ad_wait();
                advisor_header("\x92\xAE\xE0\xA3\xAE\xA2\xAB\xEF", 2); /* reciv-ru */
                y = 40;
            }

            RECIV_SI(city * 28), city_process(city, -1);
            msgtext[0] = 0;
            append_place_name(city);
            truncate_text(msgtext, 62);
            if (y < 180) {
                draw_shadowed(msgtext, 16, y, 15);
            }

            msgtext[0] = 0;
            if (townbuf[city].status & 1) {
                strcat(msgtext, "(\x82\xAE\xAB\xAD\xA5\xAD\xA8\xEF)"); /* reciv-ru */
            } else {
                strcat(msgtext, " ");
                strcat(msgtext, itoa(city_revel_sums, nstr, 10));
                strcat(msgtext, "\\/");
                strcat(msgtext, itoa(taxes, nstr, 10));
                strcat(msgtext, "$/");
                strcat(msgtext, itoa(learning_yield, nstr, 10));
                strcat(msgtext, "~");
                tot_inc += taxes;
                tc += learning_yield;
            }
            if (y < 180) {
                draw_string(msgtext, 80, y, 10);
                y += 8;
            }

            for (i = 1; i < 24; i++) {
                if (townbuf[city].buildings & (1L << i)) {
                    building_costs[i] += building_names[i].maintenance;
                    if (i == 1) {
                        building_costs[i] += rate;
                    }
                }
            }
        }
    }

    strcpy(msgtext, "\x8E\xA1\xE9. \xAF\xE0\xA8\xA1\xEB\xAB\xEC: "); /* reciv-ru */
    strcat(msgtext, itoa(tot_inc, nstr, 10));
    strcat(msgtext, "$");
    draw_string(msgtext, 8, y + 4, 10);

    if (civ_wonder(civnow, 20)) {
        tc += tc / 2;
    }
    if (tc != 0 && y < 180) {
        turn = (((year < 0) ? 1 : (year < 1750) ? 2 : 3) * tech_count[civnow] * (cur_tier * 2 + 6)) / tc;
        strcpy(msgtext, "\x8E\xE2\xAA\xE0\xEB\xE2\xA8\xEF: "); /* reciv-ru */
        strcat(msgtext, itoa(turn, nstr, 10));
        strcat(msgtext, " \xE5\xAE\xA4\xAE\xA2"); /* reciv-ru */
        draw_string(msgtext, 8, y + 12, 10);
    }

    draw_string("\xE1\xE2\xAE\xA8\xAC. \xE1\xAE\xA4\xA5\xE0\xA6\xA0\xAD\xA8\xEF", 160, 32, 15); /* reciv-ru */
    y = 40;
    tot_inc = 0;
    for (i = 0; i < 24; i++) {
        if (building_costs[i] != 0) {
            msgtext[0] = 0;
            if (i == 1) {
                strcat(msgtext, itoa(building_costs[i] / rate, nstr, 10));
            } else {
                strcat(msgtext, itoa(building_costs[i] / building_names[i].maintenance, nstr, 10));
            }
            strcat(msgtext, " ");
            strcat(msgtext, building_names[i].name);
            strcat(msgtext, ", ");
            strcat(msgtext, itoa(building_costs[i], nstr, 10));
            strcat(msgtext, "$");
            if (y < 180) {
                draw_string(msgtext, 160, y, 14);
                y += 8;
            }
            tot_inc += building_costs[i];
        }
    }
    strcpy(msgtext, "\x8E\xA1\xE9.\xE0\xA0\xE1\xE5\xAE\xA4: "); /* reciv-ru */
    strcat(msgtext, itoa(tot_inc, nstr, 10));
    strcat(msgtext, "$");
    draw_string(msgtext, 160, y + 4, 14);
    y += 12;

    advisor_close();
}

extern int treaties[8][8];
struct civ_type {
    char leader[16], plural[16], adjective[16];
    int f0, f1, f2, f3, f4;
};
extern struct civ_type civt[];
extern int kbhit(void);
extern void set_text_pos(int, int);

/* Calls use differing argument counts; retain this interface; NOLINTBEGIN(clang-diagnostic-deprecated-non-prototype) */
void spy_rpt(civ_arg)           /* intelligence report */
int civ_arg;
{
    int num, civ, cap, civs[8], done, x, y, i, j, ch, flag2;
    char relation;

    advisor_open("\x96\xA8\xA2\xA8\xAB\xA8\xA7\xA0\xE6\xA8\xA8 \xAC\xA8\xE0\xA0", 1); /* reciv-ru */
    y = 32;
    num = flag2 = 0;

    for (civ = 1; civ < 8; civ++) {
        if (players & (1 << civ)) {
            y = num * 24 + 32;
            draw_line(4, y - 2, 316, y - 2, 9);
            strcpy(msgtext, civ_names[civ]);
            if (civ != civnow && !(treaties[civnow][civ] & 0x40)) {
                draw_string("\x8F\xAE\xE1\xAE\xAB\xEC\xE1\xE2\xA2\xAE \xAE\xE2\xE1\xE3\xE2\xE1\xE2\xA2\xE3\xA5\xE2.", 96, y, civ_colours[civ]); /* reciv-ru */
                num++;
            } else {
                cap = -1;
                for (i = 0; i < 128; i++) {
                    if (townbuf[i].owner == civ && townbuf[i].status != -1 && (townbuf[i].buildings & 1)) {
                        cap = i;
                    }
                }
                strcat(msgtext, ": ");
                strcat(msgtext, leader_names[civ]);
                draw_shadowed(msgtext, 8, y, 15);
                strcpy(msgtext, government_names[civ_gov[civ]]);
                strcat(msgtext, ", ");
                strcat(msgtext, itoa(money[civ], nstr, 10));
                strcat(msgtext, "$");
                if (cur_tier <= 1) {
                    strcat(msgtext, ", ");
                    strcat(msgtext, itoa(troops_total[civ], nstr, 10));
                    strcat(msgtext, " \x80\xE0\xAC\xA8\xA9."); /* reciv-ru */
                }
                draw_string(msgtext, 160, y, civ_colours[civ]);
                y += 8;
                x = 16;
                for (i = 1; i < 8; i++) {
                    msgtext[0] = 0;
                    strcpy(msgtext, civ_names[i]);
                    if (x > 240) {
                        x = 16;
                        y += 8;
                    }
                    relation = treaties[civ][i];
                    if (relation & 2) {
                        if (relation & 4) {
                            strcat(msgtext, " \x91\x8E\x9E\x87"); /* reciv-ru */
                        } else {
                            strcat(msgtext, " \x8C\x88\x90"); /* reciv-ru */
                        }
                        draw_string(msgtext, x, y, 7);
                        x += 96;
                    }
                    if ((treaties[civ][i] & 3) == 1) {
                        strcat(msgtext, ":\x82\xAE\xA9\xAD\xA0"); /* reciv-ru */
                        draw_string(msgtext, x, y, 7);
                        x += 96;
                    }
                }
                civs[num++] = civ;
                if (civ != civnow) {
                    strcpy(msgtext, "\x88\xAD\xE4\xAE"); /* reciv-ru */
                    strcat(msgtext, itoa(num, nstr, 10));
                    draw_button(281, num * 24 + 20, 318, num * 24 + 29, msgtext, civ_colours[civ]);
                }
            }
        }
    }

    mouse_on();
    do {
        do {
            poll_mouse();
        } while (mse_held == 0 && !kbhit());

        done = 1;
        if (kbhit()) {
            ch = key_cmd();
            if (ch >= '1' && ch <= '7') {
                civ = ch - '1';
                goto select_civ;
            }
        }
        if (ptr_x > 281) {
            civ = (ptr_y - 28) / 24;
select_civ:
            if (civ >= 0 && civ < num) {
                civ = civs[civ];
                if (civ != civnow && (treaties[civnow][civ] & 0x40)) {
                    civ_dossier(civ);
                    flag2 = 1;
                    done = 0;
                }
            }
        }
        civ = ptr_x;
        repaint();
        mouse_off();
        set_text_pos(270, ptr_y);
        mouse_on();
    } while (!done);

    mouse_off();
    if (flag2) {
        sel_player();
    } else {
        menu_end();
    }
    mouse_on();
}

/* NOLINTEND(clang-diagnostic-deprecated-non-prototype) */

void civ_dossier(int civ)
{
    int cap, i, y;

    advisor_open("\x96\xA8\xA2\xA8\xAB\xA8\xA7\xA0\xE6\xA8\xA8 \xAC\xA8\xE0\xA0", 1); /* reciv-ru */
    y = 32;

    strcpy(msgtext, "\x92\xA5\xAC\xA0: "); /* reciv-ru */
    strcat(msgtext, civ_names[civ]);
    draw_shadowed(msgtext, 16, 32, 15);
    y += 12;

    strcpy(msgtext, "\x92\xA5\xAC\xA0: "); /* reciv-ru */
    strcat(msgtext, leader_titles[civ_gov[civ]]);
    strcat(msgtext, " ");
    strcat(msgtext, leader_names[civ]);
    draw_string(msgtext, 16, y, 9);
    y += 8;

    if (civt[civ_tribe_of[civ]].f0 == 1) {
        draw_string("\x82\xAE\xA8\xAD\xE1\xE2\xA2\xA5\xAD.", 24, y, 7); /* reciv-ru */
        y += 8;
    }
    if (civt[civ_tribe_of[civ]].f0 == -1) {
        draw_string("\x84\xE0\xE3\xA6\xA5\xE1\xE2.", 24, y, 7); /* reciv-ru */
        y += 8;
    }
    if (civt[civ_tribe_of[civ]].f1 == 1) {
        draw_string("\x87\xA0\xE5\xA2\xA0\xE2\xAD\xA8\xE7\xA5\xE1\xAA\xA0\xEF", 24, y, 7); /* reciv-ru */
        y += 8;
    }
    if (civt[civ_tribe_of[civ]].f1 == -1) {
        draw_string("\x91\xAE\xA2\xA5\xE0\xE8\xA5\xAD\xAD\xA0\xEF", 24, y, 7); /* reciv-ru */
        y += 8;
    }
    if (civt[civ_tribe_of[civ]].f2 == 1) {
        draw_string("\x96\xA8\xA2\xA8\xAB\xA8\xA7.", 24, y, 7); /* reciv-ru */
        y += 8;
    }
    if (civt[civ_tribe_of[civ]].f2 == -1) {
        draw_string("\x8C\xA8\xAB\xA8\xE2\xA0\xE0\xA8\xA7\xAE\xA2.", 24, y, 7); /* reciv-ru */
        y += 8;
    }
    y += 4;

    cap = -1;
    for (i = 0; i < 128; i++) {
        if (townbuf[i].owner == civ && townbuf[i].status != -1 && (townbuf[i].buildings & 1)) {
            cap = i;
        }
    }

    strcpy(msgtext, "\x91\xE2\xAE\xAB\xA8\xE6\xA0:\x0F "); /* reciv-ru */
    append_place_name(cap);
    draw_string(msgtext, 16, y, 9);
    y += 8;

    strcpy(msgtext, "\x83\xAE\xE1. \xE1\xE2\xE0\xAE\xA9:\x0F "); /* reciv-ru */
    strcat(msgtext, government_names[civ_gov[civ]]);
    draw_string(msgtext, 16, y, 9);
    y += 8;

    strcpy(msgtext, "\x8A\xA0\xAF\xA8\xE2\xA0\xAB: \x0F "); /* reciv-ru */
    strcat(msgtext, itoa(money[civ], nstr, 10));
    strcat(msgtext, "$");
    draw_string(msgtext, 16, y, 9);
    y += 8;

    if (cur_tier <= 1) {
        strcpy(msgtext, "\x82\xAE\xA9\xE1\xAA\xA0:  \x0F "); /* reciv-ru */
        strcat(msgtext, itoa(troops_total[civ], nstr, 10));
        strcat(msgtext, " \x80\xE0\xAC\xA8\xA9."); /* reciv-ru */
        draw_string(msgtext, 16, y, 9);
        y += 12;
    }

    draw_string("\x8E\xE2\xAD\xAE\xE8\xA5\xAD\xA8\xEF \xE1 \xA4\xE0.:", 16, y, 9); /* reciv-ru */
    y += 8;

    for (i = 1; i < 8; i++) {
        if (treaties[civ][i] & 2) {
            strcpy(msgtext, " \x8C\x88\x90  "); /* reciv-ru */
        }
        if ((treaties[civ][i] & 3) == 1) {
            strcpy(msgtext, " \x82\x8E\x89\x8D\x80"); /* reciv-ru */
        }
        if (treaties[civ][i] & 1) {
            strcat(msgtext, " \xE1 \xA3\xAE\xE1.:  "); /* reciv-ru */
            strcat(msgtext, civ_names[i]);
            strcat(msgtext, ".");
            draw_string(msgtext, 24, y, 7);
            y += 8;
        }
    }
    y += 4;

    draw_string("\x92\xA5\xE5\xAD\xAE\xAB\xAE\xA3\xA8\xA8:", 16, y, 9); /* reciv-ru */
    y += 8;

    for (i = 71; i >= 0; i--) {
        if (has_tech(civ, i)) {
            draw_string(advances[i].name, 24, y, 7);
            y += 8;
            if (y >= 192) {
                break;
            }
        }
    }

    advisor_close();
}

void mood_rpt(int civ)
{                               /* attitude survey */
    int happy_total, unhappy, tot, x, i, y, city, j, dummy, step;

    advisor_open("\x8E\xA1\xE9\xA8\xA9 \xAE\xA1\xA7\xAE\xE0", 9); /* reciv-ru */
    y = 32;
    city = step = tot = happy_total = unhappy = 0;

    for (city = 0; city < 128; city++) {
        if (townbuf[city].status != -1 && townbuf[city].owner == civ) {
            if (y > 190) {
                ad_wait();
                advisor_header("\x8E\xA1\xE9\xA8\xA9 \xAE\xA1\xA7\xAE\xE0", 9); /* reciv-ru */
                y = 32;
            }

            RECIV_SI(city * 28), city_process(city, -1);
            msgtext[0] = 0;
            append_place_name(city);
            strcat(msgtext, ":");
            draw_string(msgtext, 16, y, 15);

            x = (step & 7) + 80;
            RECIV_STACK(1, "city status report"), draw_city_view(city, x, y - 3, city_experts_num, 124);

            x = 212;
            rect_fill(scrbuf, 212, y - 1, 90, 10, 11);

            for (i = 0; i < 24; i++) {
                if ((townbuf[city].buildings & (1 << i)) & 0x2618) {
                    draw_icon(i + 1, x, y - 1);
                    x += 18;
                }
            }

            tot += townbuf[city].size;
            happy_total += town_happy_ones_num;
            unhappy += town_unhappy_counts;
            step += 4;
            y += 10;
        }
    }

    if (tot == 0) {
        tot++;
    }

    strcpy(msgtext, "\x8D\xA0\xE1\xA5\xAB\xA5\xAD\xA8\xA5: "); /* reciv-ru */
    count_weighted_units(civ);
    strcat(msgtext, " \x84\xAE\xA2\xAE\xAB:"); /* reciv-ru */
    append_percent(happy_total, tot);
    strcat(msgtext, "% \x8A\xAE\xAB\xA5\xA1\xAB.:"); /* reciv-ru */
    append_percent(tot - happy_total - unhappy, tot);
    strcat(msgtext, "% \x8D\xA5\xA4\xAE\xA2\xAE\xAB:"); /* reciv-ru */
    append_percent(unhappy, tot);
    strcat(msgtext, "%");
    draw_string(msgtext, 16, y + 8, 15);

    advisor_close();
}

void append_percent(int part, int total)
{
    if (part < 300) {
        strcat(msgtext, itoa((100 * part) / total, nstr, 10));
    } else {
        strcat(msgtext, itoa((10 * part) / (total / 10), nstr, 10));
    }
}

void city_rpt(int civ)
{                               /* city status report */
    int y, city, dummy, i;

    advisor_open("\x83\xAE\xE0\xAE\xA4\xA0", 8); /* reciv-ru */
    y = 32;

    for (city = 0; city < 128; city++) {
        if (townbuf[city].status != -1 && townbuf[city].owner == civ) {
            if (y >= 192) {
                ad_wait();
                advisor_header("\x83\xAE\xE0\xAE\xA4\xA0", 8); /* reciv-ru */
                y = 32;
            }

            RECIV_SI(city * 28), city_process(city, -1);
            msgtext[0] = 0;
            append_place_name(city);
            draw_string(msgtext, 8, y, 15);

            msgtext[0] = 0;
            strcat(msgtext, itoa(townbuf[city].size, nstr, 10));
            strcat(msgtext, "-");
            strcat(msgtext, itoa(town_res_sums[0], nstr, 10));
            strcat(msgtext, "{ ");
            strcat(msgtext, itoa(town_res_sums[1], nstr, 10));
            strcat(msgtext, "| ");
            strcat(msgtext, itoa(town_res_sums[2], nstr, 10));
            strcat(msgtext, "}");
            draw_string(msgtext, 80, y, 15);

            if (townbuf[city].production >= 0) {
                if (townbuf[city].production < 64) {
                    strcpy(msgtext, utypes[townbuf[city].production].name);
                    strcat(msgtext, " ("); /* reciv-ru */
                    strcat(msgtext, itoa(*(int *) &townbuf[city].rest[2], nstr, 10));
                    strcat(msgtext, "/");
                    strcat(msgtext, itoa(utypes[townbuf[city].production].cost * 10, nstr, 10));
                    strcat(msgtext, ")");
                }
            } else {
                strcpy(msgtext, improvements[-townbuf[city].production].name);
                truncate_text(msgtext, 96);
                strcat(msgtext, " ("); /* reciv-ru */
                strcat(msgtext, itoa(*(int *) &townbuf[city].rest[2], nstr, 10));
                strcat(msgtext, "/");
                strcat(msgtext, itoa(improvements[-townbuf[city].production].cost * 10, nstr, 10));
                strcat(msgtext, ")");
            }
            draw_string(msgtext, 172, y, 15);
            y += 8;
        }
    }

    advisor_close();
}
