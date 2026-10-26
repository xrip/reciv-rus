#include <string.h>
#include "GAME.H"
#include "CIV.H"
#include "GLOBALS.H"
#include "RESIDENT.H"
#include "DIALOG.H"
#include "DRAW.H"
#include "MESSAGE.H"
#include "NEWS.H"
#include "TURN.H"
#include "CAPTURE.H"

extern struct city townbuf[];

extern int rand_scaled(int n);

extern void sprite_draw_clipped(struct page *, int, int, int);

void news_box(int city)
{                               /* newsflash panel */
    int idx;
    int first;
    char buf[8];
    int col;
    int n;

    first = city;
    if (city < 0) {
        for (idx = 0; idx < 128; idx++) {
            if (townbuf[idx].status != -1 && townbuf[idx].owner == civnow && ((townbuf[idx].buildings & 1) || city == -1))
                city = idx;
        }
    }
    mouse_off();
    if (first != -2)
        blit(scrbuf, 0, 0, 320, 200, back_page, 0, 0);
    rect_fill(scrbuf, 0, 0, 320, 100, 15);
    scrbuf->font = 4;
    draw_line(1, 1, 318, 1, 0);
    rect_fill(scrbuf, 8, 3, 304, 20, 15);
    scrbuf->text_colour = 15;
    draw_line(0, 35, 319, 35, 0);
    draw_line(1, 1, 1, 35, 0);
    draw_line(318, 1, 318, 35, 0);
    draw_line(0, 97, 319, 97, 0);
    if (first != -2) {
        for (col = 0; col < 16; col++)
            sprite_draw_clipped(scrbuf, col * 20, 100 - rand_scaled(2), banner_tile);
    }
    news_effect(40, 16, 40, 0);
    scrbuf->font = 5;
    msgtext[0] = 0;
    switch (rand_scaled(4)) {
    case 0:
        append_place_name(city);
        strcat(msgtext, TRANSLATE(" Weekly", "RU")); /* reciv-ru */
        break;
    case 1:
        append_place_name(city);
        strcat(msgtext, TRANSLATE(" Today", "RU")); /* reciv-ru */
        break;
    case 2:
        strcpy(msgtext, TRANSLATE("The ", "RU")); /* reciv-ru */
        append_place_name(city);
        strcat(msgtext, TRANSLATE(" Times", "RU")); /* reciv-ru */
        break;
    case 3:
        strcpy(msgtext, TRANSLATE("The ", "RU")); /* reciv-ru */
        append_place_name(city);
        strcat(msgtext, TRANSLATE(" Tribune", "RU")); /* reciv-ru */
        break;
    }
    n = string_width(msgtext);
    if (n > 300) {
        msgtext[0] = 0;
        append_place_name(city);
        strcat(msgtext, TRANSLATE(" News", "RU")); /* reciv-ru */
        n = string_width(msgtext);
    }
    if (n < 200) {
        draw_string_dim(",-.", 8, 11, 0);
        draw_string_dim(",-.", 268, 11, 0);
    }
    draw_text(msgtext, 160, 11, 0);
    strcpy(msgtext, rand_scaled(2) ? TRANSLATE("EXTRA!", "RU") : "FLASH"); /* reciv-ru */
    scrbuf->font = 3;
    draw_string_dim(msgtext, 6, 3, 0);
    draw_string_dim(msgtext, 272, 3, 0);
    scrbuf->font = 1;
    msgtext[0] = 0;
    strcat(msgtext, TRANSLATE("10 cents", "RU")); /* reciv-ru */
    draw_string(msgtext, 272, 28, 0);
    strcpy(msgtext, TRANSLATE("January 1, ", "RU")); /* reciv-ru */
    format_year();
    draw_string(msgtext, 8, 28, 0);
    scrbuf->font = 2;
    strcpy(buf, "*NEWSA");
    buf[5] += rand_scaled(14);
    msgtext[0] = 0;
    news_name("king.txt", buf);
    draw_text(msgtext, 160, 3, 0);
    scrbuf->font = 1;
    if (first != -2) {
        menu_dialog(TRANSLATE("Press any key to continue.\n", "RU"), 80, 128); /* reciv-ru */
        blit(back_page, 0, 0, 320, 200, scrbuf, 0, 0);
    }
    mouse_on();
}
