/* Overlay 8 (code length 17F6): the Civilopedia.
   One complete MSC translation unit with its resident _DATA [3BA4,3FAF)
   and CONST [641A,6422). Compiled with /AM /Gs /Oas /Zl. Names other than
   word_3c62 and the overlay8_* publics are provisional. */
#include <string.h>
#include <stdlib.h>
#include "CIV.H"
#include "AUDIENCE.H"
#include "CAPTURE.H"
#include "CITYMGT.H"
#include "CONTROL.H"
#include "CORE.H"
#include "DIALOG.H"
#include "DRAW.H"
#include "DRIVER.H"
#include "FILEIO.H"
#include "GAME.H"
#include "GLOBALS.H"
#include "RESIDENT.H"
#include "TURN.H"
#include "UNITS.H"
#include "MESSAGE.H"
#include "PEDIA.H"

struct terrain {
    char name[12];
    char move_cost, defense, food, shields, trade, f17, f18;
};

struct terrain_rule {
    int irrigate, irrigate_time, mine, mine_time, f8, f10;
};

extern int debug;
extern struct unit_type utypes[];
extern struct terrain terrains[];
extern struct terrain_rule work_rules[];
extern int far pedia_topics[];
extern int far topic_class[];
extern signed char far adv_source[][72];
extern signed char far pedia_icons[];

extern int disk_insert();
extern void recolour_rect();

extern void label_with_style(int, int, int);
extern int kbhit(void);
extern char *strupr(char *);

int no_pictures = 0;            /* pedia picture disk absent: text only */
char *concepts[] = {
    "\x82\xA5\xE2\xA5\xE0\xA0\xAD\xEB", "\x93\xAA\xE0\xA5\xAF\xAB\xC0", "\x8C\xA5\xAB\xA8\xAE\xE0\xA0\xE6\xA8\xEF", "\x98\xA0\xE5\xE2\xEB", "\x8F\xE3\xE2\xA8", /* reciv-ru */
    "\x86\xA5\xAB\xC0\xB0\x84\xAE\xE0\xC0", "\x80\xAD\xA0\xE0\xE5\xA8\xEF", "\x84\xA5\xE1\xAF\xAE\xE2\xA8\xA7\xAC", "\x8C\xAE\xAD\xA0\xE0\xE5\xA8\xEF", "\x8A\xAE\xAC\xAC\xE3\xAD\xA8\xA7\xAC", /* reciv-ru */
    "\x90\xA5\xE1\xAF\xE3\xA1\xAB.", "\x84\xA5\xAC\xAE\xAA\xE0\xA0\xE2\xA8\xEF", "\x94\xAE\xE0\xE2", "\x90\xA0\xA7\xA1\xAE\xA9", "\x90\xAE\xE1\xAF\xE3\xE1\xAA", /* reciv-ru */
    "\x82\xEB\xA1\xE0\xAE\xE1\xEB", "\x91\xE2\xE0\xA0\xA6\xA8", "\x90\xAE\xE1\xAA\xAE\xE8\xEC", "\x8D\xA0\xAB\xAE\xA3", "\x8D\xA0\xE3\xAA\xA0", "\x92\xAE\xE0\xA3\xAE\xA2\xEB\xA5\xB0\x8F\xE3\xE2\xA8" /* reciv-ru */
};

/* Non-prototype ABI calls below; NOLINTBEGIN(clang-diagnostic-deprecated-non-prototype) */
void civilopedia(int unused, int category)
{
    int start, choice;

    cycle_off();
    mouse_off();
    start = 0;
    do {
        choice = pedia_list(start, category);
        if (choice == -1) {
            if (pedia_more)
                start += 78;
            else
                start = 0;
        }
    } while (choice != -2);
    disk_insert(0, 1L);
    sel_player();
    cycle_on();
    mouse_on();
}

/* NOLINTEND(clang-diagnostic-deprecated-non-prototype) */

/* Non-prototype ABI calls below; NOLINTBEGIN(clang-diagnostic-deprecated-non-prototype) */
int pedia_list(int start, int category)
{
    int width;
    char label[24], low[24];
    int best_kind;
    char last[24];
    int x, count, selected, key, i, y, best_n;

    rect_fill(scrbuf, 0, 0, 320, 200, 14);
    strcpy(last, "a");
    rect_fill(scrbuf, 60, 2, 200, 9, 15);
    draw_text("\x9D\x8D\x96\x88\x8A\x8B\x8E\x8F\x85\x84\x88\x9F \x96\x88\x82\x88\x8B\x88\x87\x80\x96\x88\x88", 161, 4, 0); /* reciv-ru */
    draw_text("\x9D\x8D\x96\x88\x8A\x8B\x8E\x8F\x85\x84\x88\x9F \x96\x88\x82\x88\x8B\x88\x87\x80\x96\x88\x88", 160, 3, 10); /* reciv-ru */
    draw_string("EXIT", 286, 4, 12);
    if (category == -1)
        draw_string("\x85\xE9\xA5", 8, 4, 12); /* reciv-ru */
    rect_fill(scrbuf, 2, 14, 316, 184, 15);
    width = category <= 0 ? 100 : 150;
    x = 10;
    y = 16;
    count = 0;
    do {
        strcpy(low, "\x91\xE2\xE0. "); /* reciv-ru */
        best_n = -1;
        best_kind = -1;
        if (category == 0 || category == -1) {
            for (i = 0; i < 67; i++) {
                if (stricmp(advances[i].name, last) > 0 && stricmp(advances[i].name, low) < 0) {
                    best_n = i;
                    best_kind = 0;
                    strcpy(low, advances[i].name);
                }
            }
        }
        if (category == 1 || category == -1) {
            for (i = 1; i <= 45; i++) {
                if (stricmp(improvements[i].name, last) > 0 && stricmp(improvements[i].name, low) < 0) {
                    best_n = i;
                    best_kind = 1;
                    strcpy(low, improvements[i].name);
                }
            }
        }
        if (category == 2 || category == -1) {
            for (i = 0; i < 28; i++) {
                if (stricmp(utypes[i].name, last) > 0 && stricmp(utypes[i].name, low) < 0) {
                    best_n = i;
                    best_kind = 2;
                    strcpy(low, utypes[i].name);
                }
            }
        }
        if (category == 3 || category == -1) {
            for (i = 0; i < 12; i++) {
                if (stricmp(terrains[i].name, last) > 0 && stricmp(terrains[i].name, low) < 0) {
                    best_n = i;
                    best_kind = 3;
                    strcpy(low, terrains[i].name);
                }
            }
        }
        if (category == 4 || category == -1) {
            for (i = 0; i < 21; i++) {
                if (stricmp(concepts[i], last) > 0 && stricmp(concepts[i], low) < 0) {
                    best_n = i;
                    best_kind = 4;
                    strcpy(low, concepts[i]);
                }
            }
        }
        if (best_kind == -1)
            break;
        if (count >= start) {
            strcpy(label, low);
            truncate_text(label, width);
            draw_string(label, x, y, 0);
            pedia_topics[count - start] = best_n;
            topic_class[count - start] = best_kind;
            y += 7;
            if (y > 192) {
                x += width;
                y = 16;
            }
        }
        count++;
        strcpy(last, low);
    } while (x < 300);
    pedia_more = x >= 300 ? 1 : 0;
    if (has_mouse) {
        mouse_on();
        do
            poll_mouse();
        while (!mse_held && !kbhit());
        x = (ptr_x - 10) / width;
        if (ptr_y >= 16)
            y = (ptr_y - 16) / 7;
        else
            y = -1;
        selected = x * 26 + y;
        mouse_off();
        if (mse_held == 2) {
            y = -1;
            x = 2;
        }
        if (kbhit()) {
            key_cmd();
            y = -1;
            x = 2;
        }
    } else {
        x = y = 0;
        do {
            recolour_rect(scrbuf, x * width + 8, y * 7 + 15, width - 2, 8, 15, 10);
            key = key_cmd();
            recolour_rect(scrbuf, x * width + 8, y * 7 + 15, width - 2, 8, 10, 15);
            switch (key) {
            case 0x4800:
                if (y)
                    y--;
                else if (x) {
                    x--;
                    y = 25;
                }
                break;
            case 0x4D00:
                if (x * 26 + y + 26 < count - start)
                    x++;
                break;
            case 0x5000:
                if (x * 26 + y + 1 < count - start) {
                    if (y < 25 && x * 26 + y + 1 < count - start)
                        y++;
                    else {
                        y = 0;
                        x++;
                    }
                }
                break;
            case 0x4B00:
                if (x)
                    x--;
                break;
            }
            if (key == 'm')
                return -1;
            if (key == 27 || key == 'x')
                return -2;
            selected = x * 26 + y;
        } while (key != 13 && key != ' ');
    }
    if (y >= 0 && count - start <= selected)
        return 0;
    if (y >= 0) {
        if (which_data != 3)
            disk_insert(3, 1L);
        pedia_show(pedia_topics[selected], topic_class[selected]);
    } else
        selected = x ? -2 : -1;
    repaint();
    return selected;
}

/* NOLINTEND(clang-diagnostic-deprecated-non-prototype) */

char blurb_file[] = "blurb0.txt";

/* Non-prototype ABI calls below; NOLINTBEGIN(clang-diagnostic-deprecated-non-prototype) */
void pedia_show(int index, int kind)
{                               /* Civilopedia article */
    int back, unused, disk_ok, t, sprite, title_x, y0, i;
    char key[32];
    int y, pic;

    blurb_file[5] = kind + '0';
    cycle_off();
    mouse_off();
    disk_ok = -1;
    if (which_data != 3) {
        disk_ok = disk_insert(3, 0L);
        if (!disk_ok)
            no_pictures = 1;
    }
    back = 15;
    if (vga_modes) {
        if (no_pictures)
            back = 255;
        else {
            back = 253;
            set_dac(253, 61, 61, 61);
        }
    }
    rect_fill(scrbuf, 0, 0, 320, 200, back);
    menu_clear();
    scrbuf->font = 7;
    msgtext[0] = 0;
    title_x = 204;
    if (kind == 2)
        title_x = 224;
    if (kind == 4 || kind == 1 && index > 24 || no_pictures)
        title_x = 160;
    switch (kind) {
    case 0:
        if (!no_pictures) {
            pic = pedia_advance_index(index);
            strcpy(msgtext, "iconpg1.pic");
            msgtext[6] += pic / 9;
            load_screen(1, msgtext);
            restore_palette("iconpg1.pal");
            blit_centred(pic % 3 * 111 + 1, pic % 9 / 3 * 69 + 1, 110, 68, 62, 42);
        }
        strcpy(msgtext, advances[index].name);
        draw_text("\x90\xA0\xA7\xA2\xA8\xE2\xA8\xA5 \xE6\xA8\xA2\xA8\xAB\xA8\xA7\xA0\xE6\xA8\xA8", title_x, 36, 7); /* reciv-ru */
        break;
    case 1:
        if (!disk_insert(1, 0L))
            no_pictures = 1;
        if (index > 1 && index <= 21 && !no_pictures) {
            load_screen(1, "citypix2.pic");
            restore_palette("hill.pal");
            sprite_arena_init();
            sprite = cut_tile(1, (index - 2) / 4 * 50 + 1, ((index - 2) & 3) * 50 + 1, 49, 49);
            sprite_arena_release();
            sprite_draw(scrbuf, 37, 17, sprite);
            free_block(sprite);
        }
        strcpy(msgtext, improvements[index].name);
        draw_text(index <= 24 ? "\x90\xA0\xA7\xA2\xA8\xE2\xA8\xA5 \xA3\xAE\xE0\xAE\xA4\xAE\xA2" : "\x97\xE3\xA4\xA5\xE1\xA0 \x91\xA2\xA5\xE2\xA0", title_x, 36, 7); /* reciv-ru */
        break;
    case 2:
        if (!no_pictures) {
            pic = pedia_unit_index(index);
            strcpy(msgtext, "iconpga.pic");
            msgtext[6] += pic / 6;
            load_screen(1, msgtext);
            restore_palette("iconpga.pal");
            blit_centred((pic & 1) * 160 + 1, pic % 6 / 2 * 61, 158, 61, 88, 42);
        }
        strcpy(msgtext, utypes[index].name);
        draw_text("\x82\xAE\xA9\xE1\xAA\xA0", title_x, 36, 7); /* reciv-ru */
        sprite_draw(scrbuf, 216, 48, colour_unit_glyph[32 + index]);    /* civ colour 1 */
        break;
    case 3:
        if (!no_pictures) {
            pic = pedia_terrain_index(index);
            strcpy(msgtext, "iconpgt1.pic");
            msgtext[7] += pic / 6;
            load_screen(1, msgtext);
            if (vga_modes) {
                load_palette("sp256.pal", icon_colours_block);
                apply_palette(icon_colours_block);
                set_dac(253, 61, 61, 61);
            }
            blit_centred(pic % 3 * 107 + 1, pic % 6 / 3 * 87 + 5, 106, 77, 76, 46);
        }
        strcat(msgtext, terrains[index].name);
        draw_text("\x92\xA5\xE0\xE0\xA8\xE2\xAE\xE0\xA8\xA8", title_x, 36, 7); /* reciv-ru */
        break;
    case 4:
        strcat(msgtext, concepts[index]);
        draw_text("\x8F\xE0\xA8\xAD\xE6\xA8\xAF \xA8\xA3\xE0\xEB", title_x, 36, 7); /* reciv-ru */
        break;
    }
    scrbuf->font = 6;
    draw_text(strupr(msgtext), title_x, 20, 0);
    scrbuf->font = 7;
    strcpy(key, "*");
    strcat(key, msgtext);
    msgtext[0] = 0;
    y = 76;
    if (disk_ok) {
        if ((debug & 0x40) && news_name(blurb_file, key) != -1) {
            news_effect(37, 12, 76, 1);
            ad_wait();
            rect_fill(scrbuf, 8, 76, 304, 116, back);
        }
        strcat(key, "2");
        msgtext[0] = 0;
        if (kind) {
            news_name(blurb_file, key);
            y = news_effect(36, 12, y, 1) + 8;
        }
    }
    switch (kind) {
    case 0:
        y += 8;
        msgtext[0] = 0;
        if (advances[index].prereq1 != -1) {
            strcpy(msgtext, "\x92\xE0\xA5\xA1\xE3\xA5\xE2: "); /* reciv-ru */
            strcat(msgtext, advances[advances[index].prereq1].name);
        }
        if (advances[index].prereq2 != -1) {
            strcat(msgtext, " \xA8 "); /* reciv-ru */
            strcat(msgtext, advances[advances[index].prereq2].name);
        }
        draw_string(msgtext, 32, y, 1);
        y += 16;
        draw_string("\x84\xA0\xA5\xE2: ", 32, y, 1); /* reciv-ru */
        y += 8;
        y0 = y;
        for (i = 0; i < 72; i++) {
            if (advances[i].prereq1 == index) {
                strcpy(msgtext, advances[i].name);
                if (advances[i].prereq2 != -1) {
                    strcat(msgtext, " (\xAD\xA0\xA4\xAE\xB0"); /* reciv-ru */
                    strcat(msgtext, advances[advances[i].prereq2].name);
                    strcat(msgtext, ")");
                }
                draw_string(msgtext, 40, y, 9);
                y += 8;
            }
            if (advances[i].prereq2 == index) {
                strcpy(msgtext, advances[i].name);
                if (advances[i].prereq1 != -1) {
                    strcat(msgtext, " (\xAD\xA0\xA4\xAE\xB0"); /* reciv-ru */
                    strcat(msgtext, advances[advances[i].prereq1].name);
                    strcat(msgtext, ")");
                }
                draw_string(msgtext, 40, y, 9);
                y += 8;
            }
        }
        y += 4;
        for (i = 0; i < 28; i++) {
            if (utypes[i].prereq == index) {
                label_with_style(i + (civnow << 5) + 0x40, 40, y - 4);
                strcpy(msgtext, utypes[i].name);
                strcat(msgtext, " unit");
                draw_string(msgtext, 60, y, 12);
                y += 12;
            }
        }
        for (i = 1; i <= 45; i++) {
            if (improvements[i].prereq == index) {
                draw_icon(i, 40, y - 2);
                strcpy(msgtext, improvements[i].name);
                strcat(msgtext, i <= 24 ? " (\xE1\xE2\xE0\xAE\xA5\xAD\xA8\xA5)" : " (\x97\xE3\xA4\xAE)"); /* reciv-ru */
                draw_string(msgtext, 60, y, 2);
                y += 12;
            }
        }
        if (y0 + 4 == y)
            draw_string("\x84\xA0\xA5\xE2: ", 32, y - 12, back); /* reciv-ru */
        switch (adv_source[civnow][index]) {
        case -2:
            strcpy(msgtext, "(\xA8\xA7 \x82\xA5\xAB. \x81\xA8\xA1\xAB\xA8\xAE\xE2\xA5\xAA\xA8)"); /* reciv-ru */
            break;
        case -1:
            msgtext[0] = 0;
            break;
        case 0:
            strcpy(msgtext, "(\x8E\xE2\xAA\xE0\xEB\xE2\xAE)"); /* reciv-ru */
            break;
        default:
            strcpy(msgtext, "(\x8F\xAE\xE1\xE2\xA0\xE0\xA0\xAB\xA8\xE1\xEC"); /* reciv-ru */
            strcat(msgtext, civ_names[adv_source[civnow][index]]);
            strcat(msgtext, ")");
            break;
        }
        draw_text(msgtext, title_x, 48, 7);
        break;
    case 1:
        strcpy(msgtext, "\x92\xE0\xA5\xA1\xE3\xA5\xE2: "); /* reciv-ru */
        strcat(msgtext, advances[improvements[index].prereq].name);
        draw_string(msgtext, 12, y, 9);
        y += 8;
        strcpy(msgtext, "\x96\xA5\xAD\xA0: "); /* reciv-ru */
        strcat(msgtext, itoa(improvements[index].cost * 10, nstr, 10));
        strcat(msgtext, " \xE9\xA8\xE2\xAE\xA2."); /* reciv-ru */
        draw_string(msgtext, 12, y, 9);
        y += 8;
        strcpy(msgtext, "\x91\xAE\xA4\xA5\xE0\xA6\xA0\xAD\xA8\xA5: $"); /* reciv-ru */
        strcat(msgtext, itoa(improvements[index].maintenance, nstr, 10));
        draw_string(msgtext, 12, y, 12);
        y += 8;
        break;
    case 2:
        strcpy(msgtext, "\x92\xE0\xA5\xA1\xE3\xA5\xE2: "); /* reciv-ru */
        strcat(msgtext, advances[utypes[index].prereq].name);
        if (index == 25)
            strcat(msgtext, " \xA8 \x90\xA0\xE1\xAF\xA0\xA4 \xA0\xE2\xAE\xAC\xA0"); /* reciv-ru */
        draw_string(msgtext, 100, y, 9);
        y += 8;
        strcpy(msgtext, "\x96\xA5\xAD\xA0: "); /* reciv-ru */
        strcat(msgtext, itoa(utypes[index].cost * 10, nstr, 10));
        strcat(msgtext, " \xE0\xA5\xE1\xE3\xE0\xE1\xAE\xA2."); /* reciv-ru */
        draw_string(msgtext, 100, y, 9);
        y += 8;
        strcpy(msgtext, "\x93\xA4\xA0\xE0\xAD\xA0\xEF \xE1\xA8\xAB\xA0: "); /* reciv-ru */
        strcat(msgtext, itoa(utypes[index].attack, nstr, 10));
        draw_string(msgtext, 100, y, 12);
        y += 8;
        strcpy(msgtext, "\x91\xA8\xAB\xA0 \xA7\xA0\xE9\xA8\xE2\xEB: "); /* reciv-ru */
        strcat(msgtext, itoa(utypes[index].defense, nstr, 10));
        draw_string(msgtext, 100, y, 12);
        y += 8;
        strcpy(msgtext, "\x8F\xA5\xE0\xA5\xAC\xA5\xE9\xA5\xAD\xA8\xA5: "); /* reciv-ru */
        strcat(msgtext, itoa(utypes[index].moves, nstr, 10));
        draw_string(msgtext, 100, y, 0);
        y += 8;
        break;
    case 3:
        for (t = index; t <= index + 12; t += 12) {
            if (t != index && terrains[index].food == 2)
                continue;
            draw_string(terrains[t].name, 12, y, 1);
            y += 8;
            if (terrains[t].food || work_rules[index].irrigate < -1) {
                strcpy(msgtext, "\x8F\xA8\xE9\xA0: "); /* reciv-ru */
                strcat(msgtext, itoa(terrains[t].food, nstr, 10));
                if (terrains[t].food >= 3)
                    strcat(msgtext, "*");
                if (work_rules[index].irrigate < -1) {
                    strcat(msgtext, " (");
                    strcat(msgtext, itoa(terrains[t].food - work_rules[index].irrigate - 1, nstr, 10));
                    if (terrains[t].food >= 2)
                        strcat(msgtext, "*");
                    strcat(msgtext, " \xE1 \xAC\xA5\xAB\xA8\xAE\xE0\xA0\xE6\xA8\xA5\xA9)"); /* reciv-ru */
                }
                strcat(msgtext, " \xA5\xA4."); /* reciv-ru */
                draw_string(msgtext, 16, y, 9);
                y += 8;
            }
            if (terrains[t].shields || work_rules[index].mine < -1) {
                strcpy(msgtext, "\x8F\xE0\xAE\xA4\xE3\xAA\xE6\xA8\xEF: "); /* reciv-ru */
                if (index == 2 || index == 11)
                    strcat(msgtext, "0/");
                strcat(msgtext, itoa(terrains[t].shields, nstr, 10));
                if (work_rules[index].mine < -1) {
                    strcat(msgtext, " (");
                    strcat(msgtext, itoa(terrains[t].shields - work_rules[index].mine - 1, nstr, 10));
                    if (terrains[t].shields - work_rules[index].mine - 1 >= 3)
                        strcat(msgtext, "*");
                    strcat(msgtext, " \xE1 \xE8\xA0\xE5\xE2\xA0\xAC\xA8)"); /* reciv-ru */
                }
                strcat(msgtext, " \xA5\xA4."); /* reciv-ru */
                draw_string(msgtext, 16, y, 9);
                y += 8;
            }
            if (terrains[t].trade || index <= 2) {
                strcpy(msgtext, "\x92\xAE\xE0\xA3\xA8: "); /* reciv-ru */
                strcat(msgtext, itoa(terrains[t].trade, nstr, 10));
                if (terrains[t].trade)
                    strcat(msgtext, "%");
                if (terrains[t].trade >= 3)
                    strcat(msgtext, "*");
                if (index <= 2) {
                    strcat(msgtext, " (");
                    strcat(msgtext, itoa(terrains[t].trade + 1, nstr, 10));
                    strcat(msgtext, "% \xE1 \xA4\xAE\xE0\xAE\xA3\xA0\xAC\xA8)"); /* reciv-ru */
                }
                draw_string(msgtext, 16, y, 9);
                y += 8;
            }
            if (t == 7) {
                draw_string("\xAF\xE3\xE1\xE2\xAE", 16, y, 9); /* reciv-ru */
                y += 8;
            }
            y += 4;
        }
        draw_string("*  -1 if government is Despotism/Anarchy.", 16, y, 9);
        y += 8;
        draw_string("%  +1 \xAF\xE0\xA8 \xE0\xA5\xE1\xAF\xE3\xA1\xAB\xA8\xAA\xA5/\xA4\xA5\xAC\xAE\xAA\xE0\xA0\xE2\xA8\xA8.", 16, y, 9); /* reciv-ru */
        y += 12;
        strcpy(msgtext, "\x8F\xA5\xE0\xA5\xA4\xA2\xA8\xA6\xA5\xAD\xA8\xA5: "); /* reciv-ru */
        strcat(msgtext, itoa(terrains[index].move_cost, nstr, 10));
        strcat(msgtext, " \xA5\xA4"); /* reciv-ru */
        draw_string(msgtext, 12, y, 12);
        y += 8;
        strcpy(msgtext, "\x91\xA8\xAB\xA0 \xA7\xA0\xE9\xA8\xE2\xEB: +"); /* reciv-ru */
        strcat(msgtext, itoa(terrains[index].defense * 50 - 100, nstr, 10));
        strcat(msgtext, "%");
        draw_string(msgtext, 12, y, 12);
        y += 8;
        break;
    }
    scrbuf->font = 1;
    mouse_on();
    ad_wait();
    mouse_off();
    if (disk_ok == 1L)
        disk_insert(0, 1L);
    no_pictures = 0;
    if (mouse_mode == -1) {
        rect_fill(scrbuf, 8, 8, 304, 184, back);
        cycle_on();
    }
    mouse_on();
}

/* NOLINTEND(clang-diagnostic-deprecated-non-prototype) */

int pedia_advance_index(int n)
{
    int i;

    for (i = 0; i < 72; i++)
        if (pedia_icons[i] == n)
            return i;
    return -1;
}

/* Non-prototype ABI calls below; NOLINTBEGIN(clang-diagnostic-deprecated-non-prototype) */
void blit_centred(int x, int y, int w, int h, int cx, int cy)
{
    int x2, y2;

    x2 = x + w;
    if (x2 > 319) {
        x2 = 319;
        w = 319 - x;
    }
    y2 = y + h;
    if (y2 > 199) {
        y2 = 199;
        h = 199 - y;
    }
    if (cx < w / 2) {
        x += w / 2 - cx;
        w = cx * 2;
    }
    blit(back_page, x, y, w, h, scrbuf, cx - w / 2, cy - h / 2);
}

/* NOLINTEND(clang-diagnostic-deprecated-non-prototype) */

int pedia_unit_index(int n)
{
    int i;

    for (i = 0; i < 28; i++)
        if (pedia_icons[i + 68] == n)
            return i;
    return -1;
}

int pedia_terrain_index(int n)
{
    int i;

    for (i = 0; i < 12; i++)
        if (pedia_icons[i + 97] == n)
            return i;
    return -1;
}

/* Civilopedia topic orders: advances, units and improvements, each ended
   by -1; original far-data block. */
signed char far pedia_icons[109] = {
    24, 32, 11, 65, 26, 36, 13, 59, 53, 44, 39, 5, 1, 51, 4, 35,
    16, 50, 52, 38, 29, 49, 22, 0, 21, 10, 33, 60, 19, 37, 45, 12,
    15, 43, 30, 40, 2, 41, 17, 27, 18, 3, 61, 56, 57, 6, 34, 55,
    14, 62, 20, 58, 23, 8, 64, 66, 63, 31, 9, 48, 42, 7, 47, 54,
    28, 25, 46, -1, 4, 21, 19, 14, 24, 15, 13, 18, 16, 17, 8, 9,
    12, 26, 20, 6, 1, 22, 25, 23, 11, 0, 10, 5, 2, 3, 27, 7,
    -1, 6, 7, 8, 9, 10, 11, 0, 2, 3, 1, 4, 5
};
