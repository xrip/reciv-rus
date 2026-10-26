/* Main module: resident code segment 01A8 (main, mouse and screen helpers,
   disk prompt, break handlers, title menu) and the game rule tables in
   DGROUP 00AC..1C20. Compiled with /AM /Gs /Oas. Names are provisional. */
#include <dos.h>
#include <stdlib.h>
#include <string.h>
#include "GLOBALS.H"
#include "AUDIENCE.H"
#include "DIALOG.H"
#include "F0DATA.H"
#include "RESIDENT.H"
#include "CORE.H"
#include "FAME.H"
#include "FILEIO.H"
#include "GAME.H"
#include "INTRO.H"
#include "MESSAGE.H"
#include "NEWGAME.H"
#include "SAVE.H"
#include "SETUP.H"
#include "TURN.H"
#include "WORLDGEN.H"

extern union REGS bios_call_reg_block_io;
extern void (interrupt far * orig_break_isr) ();
extern void (interrupt far * int_23) ();

extern int load_program(char *, int);
extern void run_program(int);
extern int free_paragraphs(void);

extern void set_page(int);
extern int page_ready(int);
extern void flip(int);
extern void set_palette(int, int, int, int);
extern void set_text_pos(int, int);

extern void sprite_arena_init(void), sprite_arena_release(void);
extern void sound_shutdown(void), restore_timer(void);

extern int init_mouse(void), mouse_poll(void);
extern void set_mousecur(int, int, int), mouse_close(void);
extern void mouse_show(void), mouse_hide(void);
extern int file_exists(char *, int);
extern int sound_quiet_ticks(void);
extern void sound_command(int, int);

extern void (interrupt far * _dos_getvect(int)) ();
extern void _dos_setvect(int, void (interrupt far *) ());

extern void show_help(char *);

struct terrain_rec {
    char name[12];
    unsigned char f0, f1, f2, f3, f4, f5, f6;
};

struct word_rec {
    int a, b, c, d;
};

/* 0-11 the terrain types, 12-23 the same with their special resource. */
struct terrain_rec terrains[24] = {
    { "\x8F\xE3\xE1\xE2\xEB\xAD\xEF", 1, 2, 0, 1, 0, 1, 14 }, /* reciv-ru */
    { "\x90\xA0\xA2\xAD\xA8\xAD\xA0", 1, 2, 1, 1, 0, 1, 6 }, /* reciv-ru */
    { "\x8B\xE3\xA3\xA0", 1, 2, 2, 1, 0, 1, 10 }, /* reciv-ru */
    { "\x8B\xA5\xE1", 2, 3, 1, 2, 0, 2, 2 }, /* reciv-ru */
    { "\x95\xAE\xAB\xAC\xEB", 2, 4, 1, 0, 0, 2, 12 }, /* reciv-ru */
    { "\x83\xAE\xE0\xEB", 3, 6, 0, 1, 0, 3, 13 }, /* reciv-ru */
    { "\x92\xE3\xAD\xA4\xE0\xA0", 1, 2, 1, 0, 0, 0, 7 }, /* reciv-ru */
    { "\x8B\xEC\xA4\xEB", 2, 2, 0, 0, 0, 0, 15 }, /* reciv-ru */
    { "\x81\xAE\xAB\xAE\xE2\xA0", 2, 3, 1, 0, 0, 0, 3 }, /* reciv-ru */
    { "\x84\xA6\xE3\xAD\xA3\xAB\xA8", 2, 3, 1, 0, 0, 0, 11 }, /* reciv-ru */
    { "\x8E\xAA\xA5\xA0\xAD", 1, 2, 1, 0, 2, 0, 1 }, /* reciv-ru */
    { "\x90\xA5\xAA\xA0", 1, 3, 2, 1, 1, 2, 9 }, /* reciv-ru */
    { "\x8E\xA0\xA7\xA8\xE1", 1, 2, 3, 1, 0, 1, 14 }, /* reciv-ru */
    { "\x8B\xAE\xE8\xA0\xA4\xA8", 1, 2, 1, 3, 0, 1, 6 }, /* reciv-ru */
    { "\x8B\xE3\xA3\xA0", 1, 2, 2, 1, 0, 1, 10 }, /* reciv-ru */
    { "\x84\xA8\xE7\xEC", 2, 3, 3, 2, 0, 2, 2 }, /* reciv-ru */
    { "\x93\xA3\xAE\xAB\xEC", 2, 4, 1, 2, 0, 2, 12 }, /* reciv-ru */
    { "\x87\xAE\xAB\xAE\xE2\xAE", 3, 6, 0, 1, 6, 3, 13 }, /* reciv-ru */
    { "\x84\xA8\xE7\xEC", 1, 2, 3, 0, 0, 0, 7 }, /* reciv-ru */
    { "\x92\xEE\xAB\xA5\xAD\xA8", 2, 2, 2, 0, 0, 0, 15 }, /* reciv-ru */
    { "\x8D\xA5\xE4\xE2\xEC", 2, 3, 1, 4, 0, 0, 3 }, /* reciv-ru */
    { "\x80\xAB\xAC\xA0\xA7\xEB", 2, 3, 1, 0, 4, 0, 11 }, /* reciv-ru */
    { "\x90\xEB\xA1\xA0", 1, 2, 3, 0, 2, 0, 1 }, /* reciv-ru */
    { "\x90\xA5\xAA\xA0", 1, 3, 2, 1, 1, 2, 9 }, /* reciv-ru */
};

struct word_rec work_rules[18] = {
    { -2, 5, -2, 5 },
    { 0, 0, -2, 5 },
    { 2, 15, 1, 1 },
    { -2, 5, 2, 10 },
    { 0, 1, 6, 5 },
    { -1, 5, 0, 0 },
    { -2, 10, -4, 10 },
    { 1, 1, -1, 0 },
    { -2, 10, 0, 0 },
    { -1, 0, -1, 0 },
    { 0, 0, -1, 0 },
    { -1, 0, 0, 0 },
    { 10, 15, 2, 15 },
    { 0, 0, 10, 15 },
    { 2, 15, 0, 0 },
    { -1, 0, -1, 0 },
    { 0, 0, -2, 5 },
    { -1, 0, 0, 1 },
};

struct advance advances[72] = {
    { "\x80\xAB\xE4\xA0\xA2\xA8\xE2", -1, -1 }, /* reciv-ru */
    { "\x91\xA2\xAE\xA4\xB0\x87\xA0\xAA\xAE\xAD\xAE\xA2", 0, -1 }, /* reciv-ru */
    { "\x84\xA5\xAD\xEC\xA3\xA8", 17, -1 }, /* reciv-ru */
    { "\x80\xE2\xAE\xAC\xAD\xA0\xEF\xB0\x92\xA5\xAE\xE0\xA8\xEF", 51, 11 }, /* reciv-ru */
    { "\x84\xA5\xAC\xAE\xAA\xE0\xA0\xE2\xA8\xEF", 28, 30 }, /* reciv-ru */
    { "\x8C\xAE\xAD\xA0\xE0\xE5\xA8\xEF", 25, 1 }, /* reciv-ru */
    { "\x80\xE1\xE2\xE0\xAE\xAD\xAE\xAC\xA8\xEF", 26, 9 }, /* reciv-ru */
    { "\x8A\xA0\xE0\xE2\xAE\xA3\xE0\xA0\xE4\xA8\xEF", 0, -1 }, /* reciv-ru */
    { "\x8D\xA0\xA2\xA8\xA3\xA0\xE6\xA8\xEF", 7, 6 }, /* reciv-ru */
    { "\x8C\xA0\xE2\xA5\xAC\xA0\xE2\xA8\xAA\xA0", 0, 16 }, /* reciv-ru */
    { "\x8C\xA5\xA4\xA8\xE6\xA8\xAD\xA0", 28, 24 }, /* reciv-ru */
    { "\x94\xA8\xA7\xA8\xAA\xA0", 9, 8 }, /* reciv-ru */
    { "\x92\xA5\xE5\xAD\xA8\xAA\xA0", 33, 45 }, /* reciv-ru */
    { "\x93\xAD\xA8\xA2\xA5\xE0\xE1\xA8\xE2\xA5\xE2", 9, 28 }, /* reciv-ru */
    { "\x8C\xA0\xA3\xAD\xA5\xE2\xA8\xA7\xAC", 8, 11 }, /* reciv-ru */
    { "\x9D\xAB\xA5\xAA\xE2\xE0\xAE\xAD\xA8\xAA\xA0", 12, 54 }, /* reciv-ru */
    { "\x91\xE2\xE0\xAE\xA8\xE2\xA5\xAB\xEC\xE1\xE2\xA2\xAE", -1, -1 }, /* reciv-ru */
    { "\x8E\xA1\xE0\xA0\xA1\xAE\xE2\xAA\xA0\xB0\x81\xE0\xAE\xAD\xA7\xEB", -1, -1 }, /* reciv-ru */
    { "\x8E\xA1\xE0\xA0\xA1\xAE\xE2\xAA\xA0\xB0\x86\xA5\xAB\xA5\xA7\xA0", 17, -1 }, /* reciv-ru */
    { "\x8C\xAE\xE1\xE2\xAE\xE1\xE2\xE0\xAE\xA5\xAD\xA8\xA5", 18, 45 }, /* reciv-ru */
    { "\x88\xA7\xAE\xA1\xE0\xA5\xE2\xA5\xAD\xA8\xEF", 12, 30 }, /* reciv-ru */
    { "\x8A\xAE\xAC\xAF\xEC\xEE\xE2\xA5\xE0\xEB", 9, 15 }, /* reciv-ru */
    { "\x8F\xA8\xE1\xEC\xAC\xA5\xAD\xAD\xAE\xE1\xE2\xEC", 0, -1 }, /* reciv-ru */
    { "\x8F\xA0\xE0\xAE\xA2\xAE\xA9\xB0\x84\xA2\xA8\xA3\xA0\xE2\xA5\xAB\xEC", 11, 20 }, /* reciv-ru */
    { "\x92\xAE\xE0\xA3\xAE\xA2\xAB\xEF", 2, 1 }, /* reciv-ru */
    { "\x96\xA5\xE0\xA5\xAC\xAE\xAD\xC0\xB0\x91\xAE\xA6\xA6\xA5\xAD\xA8\xA5", -1, -1 }, /* reciv-ru */
    { "\x8C\xA8\xE1\xE2\xA8\xE6\xA8\xA7\xAC", 25, -1 }, /* reciv-ru */
    { "\x90\xA0\xE1\xE9\xA5\xAF\xAB\xA5\xAD\xA8\xA5\xB0\x80\xE2\xAE\xAC\xA0", 41, 3 }, /* reciv-ru */
    { "\x94\xA8\xAB\xAE\xE1\xAE\xE4\xA8\xEF", 26, 30 }, /* reciv-ru */
    { "\x90\xA5\xAB\xA8\xA3\xA8\xEF", 28, 22 }, /* reciv-ru */
    { "\x8B\xA8\xE2\xA5\xE0\xA0\xE2\xE3\xE0\xA0", 22, 1 }, /* reciv-ru */
    { "\x85\xA7\xA4\xA0\xB0\x82\xA5\xE0\xE5\xAE\xAC", -1, -1 }, /* reciv-ru */
    { "\x94\xA5\xAE\xA4\xA0\xAB\xA8\xA7\xAC", 16, 5 }, /* reciv-ru */
    { "\x8A\xAE\xAB\xA5\xE1\xAE", -1, -1 }, /* reciv-ru */
    { "\x8F\xAE\xE0\xAE\xE5", 20, 18 }, /* reciv-ru */
    { "\x88\xAD\xA4\xE3\xE1\xE2\xE0\xA8\xA0\xAB\xA8\xA7\xA0\xE6\xA8\xEF", 49, 53 }, /* reciv-ru */
    { "\x95\xA8\xAC\xA8\xEF", 13, 10 }, /* reciv-ru */
    { "\x82\xAD\xE3\xE2\xE0\xA5\xAD\xAD\xA5\xA5\xB0\x91\xA3\xAE\xE0\xA0\xAD\xA8\xA5", 55, 56 }, /* reciv-ru */
    { "\x8F\xAE\xAB\xA5\xE2\xEB", 37, 11 }, /* reciv-ru */
    { "\x90\xA5\xA0\xAA\xE2\xA8\xA2\xAD\xEB\xA5\xB0\x8F\xAE\xAB\xA5\xE2\xEB", 38, 54 }, /* reciv-ru */
    { "\x8A\xAE\xE1\xAC\xA8\xE7\xA5\xE1\xAA\xA8\xA5\xB0\x8F\xAE\xAB\xA5\xE2\xEB", 21, 46 }, /* reciv-ru */
    { "\x8F\xE0\xAE\xA8\xA7\xA2\xAE\xA4\xE1\xE2\xA2\xAE", 58, 47 }, /* reciv-ru */
    { "\x83\xAB\xA8\xAD\xAE\xA4\xA5\xAB\xA8\xA5", -1, -1 }, /* reciv-ru */
    { "\x8A\xAE\xAC\xAC\xE3\xAD\xA8\xA7\xAC", 28, 35 }, /* reciv-ru */
    { "\x90\xA5\xE1\xAF\xE3\xA1\xAB\xA8\xAA\xA0", 1, 30 }, /* reciv-ru */
    { "\x8A\xAE\xAD\xE1\xE2\xE0\xE3\xA8\xE0\xAE\xA2\xA0\xAD\xA8\xA5", 16, 2 }, /* reciv-ru */
    { "\x90\xA0\xAA\xA5\xE2\xAE\xE1\xE2\xE0\xAE\xA5\xAD\xA8\xA5", 39, 15 }, /* reciv-ru */
    { "\x8A\xAE\xE0\xAF\xAE\xE0\xA0\xE6\xA8\xA8", 53, 35 }, /* reciv-ru */
    { "\x8C\xA5\xE2\xA0\xAB\xAB\xE3\xE0\xA3\xA8\xEF", 34, 13 }, /* reciv-ru */
    { "\x86\xA5\xAB\xA5\xA7\xAD\xA0\xEF\xB0\x84\xAE\xE0\xAE\xA3\xA0", 23, 19 }, /* reciv-ru */
    { "\x9F\xA4\xA5\xE0\xAD\xA0\xEF\xB0\x9D\xAD\xA5\xE0\xA3\xA8\xEF", 27, 15 }, /* reciv-ru */
    { "\x92\xA5\xAE\xE0\xA8\xEF\xB0\x83\xE0\xA0\xA2\xA8\xE2\xA0\xE6\xA8\xA8", 6, 13 }, /* reciv-ru */
    { "\x91\xE2\xA0\xAB\xEC", 48, 35 }, /* reciv-ru */
    { "\x81\xA0\xAD\xAA\xAE\xA2\xE1\xAA\xAE\xA5\xB0\x84\xA5\xAB\xAE", 24, 44 }, /* reciv-ru */
    { "\x9D\xAB\xA5\xAA\xE2\xE0\xA8\xE7\xA5\xE1\xE2\xA2\xAE", 48, 14 }, /* reciv-ru */
    { "\x8E\xE7\xA8\xE1\xE2\xAA\xA0", 36, 47 }, /* reciv-ru */
    { "\x82\xA7\xE0\xEB\xA2\xE7\xA0\xE2\xEB\xA5\xB0\x82\xA5\xE9\xA5\xE1\xE2\xA2\xA0", 34, 36 }, /* reciv-ru */
    { "\x91\xA2\xA5\xE0\xE5\xAF\xE0\xAE\xA2\xAE\xA4\xA8\xAC\xAE\xE1\xE2\xEC", 60, 41 }, /* reciv-ru */
    { "\x80\xA2\xE2\xAE\xAC\xAE\xA1\xA8\xAB\xEC", 37, 52 }, /* reciv-ru */
    { "\x83\xA5\xAD\xA5\xE2\xA8\xAA\xA0", 10, 47 }, /* reciv-ru */
    { "\x8F\xAB\xA0\xE1\xE2\xAC\xA0\xE1\xE1\xEB", 55, 40 }, /* reciv-ru */
    { "\x8F\xA5\xE0\xA5\xE0\xA0\xA1\xAE\xE2\xAA\xA0", 41, 4 }, /* reciv-ru */
    { "\x90\xEB\xE6\xA0\xE0\xE1\xE2\xA2\xAE", 32, 31 }, /* reciv-ru */
    { "\x90\xAE\xA1\xAE\xE2\xEB", 60, 21 }, /* reciv-ru */
    { "\x82\xAE\xA8\xAD\xE1\xAA\xA0\xEF\xB0\x8F\xAE\xA2\xA8\xAD\xAD\xAE\xE1\xE2\xEC", 44, 56 }, /* reciv-ru */
    { "\x8F\xE0\xAE\xE4\xE1\xAE\xEE\xA7\xEB", 41, 43 }, /* reciv-ru */
    { "\x9D\xAD\xA5\xE0\xA3\xA8\xEF\xB0\x90\xA0\xE1\xAF\xA0\xA4\xA0", 50, 57 }, /* reciv-ru */
    { "1", 127, 127 },
    { "2", 127, 127 },
    { "3", 127, 127 },
    { "4", 127, 127 },
    { "\x80\xAC\xA1\xA0\xE0", 127, 127 } /* reciv-ru */
};

struct advance_attr advance_attrs[72] = {
    { 5, 0 },
    { 4, 1 },
    { 4, 1 },
    { 4, 0 },
    { 3, 2 },
    { 6, 1 },
    { 4, 0 },
    { 6, -1 },
    { 6, -1 },
    { 6, -1 },
    { 4, 0 },
    { 3, 0 },
    { 4, 0 },
    { 3, 1 },
    { 3, -2 },
    { 3, 0 },
    { 3, 1 },
    { 6, -1 },
    { 5, -2 },
    { 3, 0 },
    { 4, 0 },
    { 3, 0 },
    { 3, 1 },
    { 4, -1 },
    { 4, 1 },
    { 5, 0 },
    { 4, 0 },
    { 8, -2 },
    { 3, 1 },
    { 3, 2 },
    { 3, 2 },
    { 4, -1 },
    { 5, -1 },
    { 4, -2 },
    { 8, -2 },
    { 5, -1 },
    { 5, -1 },
    { 6, -1 },
    { 4, -1 },
    { 5, -1 },
    { 4, 1 },
    { 6, 0 },
    { 6, 1 },
    { 2, 0 },
    { 5, 1 },
    { 4, 0 },
    { 6, -2 },
    { 4, 0 },
    { 6, -2 },
    { 4, 0 },
    { 3, 0 },
    { 3, 1 },
    { 4, -1 },
    { 4, 1 },
    { 3, 1 },
    { 4, 0 },
    { 5, -1 },
    { 2, 1 },
    { 8, -1 },
    { 2, 2 },
    { 4, 1 },
    { 2, 1 },
    { 5, -2 },
    { 5, -1 },
    { 6, -1 },
    { 4, -1 },
    { 3, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
};

struct improvement improvements[46] = {
    { "\x81\xA5\xA7 \xAF\xE0\xAE\xAF\xA8\xE1\xAA\xA8", 8, 0, -1 }, /* reciv-ru */
    { "\x84\xA2\xAE\xE0\xA5\xE6", 20, 5, 16 }, /* reciv-ru */
    { "\x8A\xA0\xA7\xA0\xE0\xAC\xEB", 4, 0, -1 }, /* reciv-ru */
    { "\x80\xAC\xA1\xA0\xE0", 6, 1, 42 }, /* reciv-ru */
    { "\x95\xE0\xA0\xAC", 4, 1, 25 }, /* reciv-ru */
    { "\x81\xA0\xA7\xA0\xE0", 8, 1, 2 }, /* reciv-ru */
    { "\x81\xA8\xA1\xAB\xA8\xAE\xE2\xA5\xAA\xA0", 8, 1, 22 }, /* reciv-ru */
    { "\x91\xE3\xA4", 8, 1, 1 }, /* reciv-ru */
    { "\x91\xE2\xA5\xAD\xEB", 12, 2, 16 }, /* reciv-ru */
    { "\x82\xAE\xA4\xAE\xAF\xE0\xAE\xA2\xAE\xA4", 12, 2, 45 }, /* reciv-ru */
    { "\x81\xA0\xAD\xAA", 12, 3, 53 }, /* reciv-ru */
    { "\x91\xAE\xA1\xAE\xE0", 16, 3, 29 }, /* reciv-ru */
    { "\x93\xAD\xA8\xA2\xA5\xE0\xE1\xA8\xE2\xA5\xE2", 16, 3, 13 }, /* reciv-ru */
    { "\x8E\xA1\xE9\xA5\xE1\xE2\xA2\xA5\xAD\xAD\xEB\xA9\xB0\x92\xE0\xA0\xAD\xE1\xAF\xAE\xE0\xE2", 16, 4, 41 }, /* reciv-ru */
    { "\x91\xE2\xA0\xA4\xA8\xAE\xAD", 10, 4, 45 }, /* reciv-ru */
    { "\x94\xA0\xA1\xE0\xA8\xAA\xA0", 20, 4, 35 }, /* reciv-ru */
    { "\x87\xA0\xA2\xAE\xA4", 32, 6, 63 }, /* reciv-ru */
    { "\x91\x8E\x88", 20, 4, 57 }, /* reciv-ru */
    { "\x96\xA5\xAD\xE2\xE0\xB0\x8F\xA5\xE0\xA5\xE0\xA0\xA1\xAE\xE2\xAA\xA8", 20, 2, 61 }, /* reciv-ru */
    { "\x9D\xAB\xA5\xAA\xE2\xE0\xAE\xE1\xE2\xA0\xAD\xE6\xA8\xEF", 16, 4, 55 }, /* reciv-ru */
    { "\x83\xA8\xA4\xE0\xAE\xE1\xE2\xA0\xAD\xE6\xA8\xEF", 24, 4, 15 }, /* reciv-ru */
    { "\x80\xE2\xAE\xAC\xAD\xA0\xEF\xB0\x91\xE2\xA0\xAD\xE6\xA8\xEF", 16, 2, 50 }, /* reciv-ru */
    { "\x8A\xAE\xAD\xE1\xE2\xE0\xE3\xAA\xE6\xA8\xA8\xB0\x90\xA0\xAA\xA5\xE2\xEB", 8, 0, 40 }, /* reciv-ru */
    { "\x8A\xAE\xAC\xAF\xAE\xAD\xA5\xAD\xE2\xEB\xB0\x90\xA0\xAA\xA5\xE2\xEB", 16, 0, 60 }, /* reciv-ru */
    { "\x8C\xAE\xA4\xE3\xAB\xEC\xB0\x90\xA0\xAA\xA5\xE2\xEB", 32, 0, 63 }, /* reciv-ru */
    { "\x8F\xA8\xE0\xA0\xAC\xA8\xA4\xEB", 30, 0, 16 }, /* reciv-ru */
    { "\x82\xA8\xE1\xEF\xE7\xA8\xA5\xB0\x91\xA0\xA4\xEB", 30, 0, 42 }, /* reciv-ru */
    { "\x8A\xAE\xAB\xAE\xE1\xE1", 20, 0, 17 }, /* reciv-ru */
    { "\x8C\xA0\xEF\xAA", 20, 0, 7 }, /* reciv-ru */
    { "\x82\xA5\xAB\xA8\xAA\xA0\xEF\xB0\x81\xA8\xA1\xAB\xA8\xAE\xE2\xA5\xAA\xA0", 30, 0, 30 }, /* reciv-ru */
    { "\x8E\xE0\xA0\xAA\xE3\xAB", 30, 0, 26 }, /* reciv-ru */
    { "\x82\xA5\xAB\xA8\xAA\xA0\xEF\xB0\x91\xE2\xA5\xAD\xA0", 30, 0, 16 }, /* reciv-ru */
    { "\x9D\xAA\xE1\xAF\xA5\xA4\xA8\xE6\xA8\xEF\xB0\x8C\xA0\xA3\xA5\xAB\xAB\xA0\xAD\xA0", 40, 0, 8 }, /* reciv-ru */
    { "\x8A\xA0\xAF\xA5\xAB\xAB\xA0\xB0\x8C\xA8\xAA\xA5\xAB\xA0\xAD\xA4\xA6\xA5\xAB\xAE", 30, 0, 29 }, /* reciv-ru */
    { "\x8E\xA1\xE1\xA5\xE0\xA2\xA0\xE2\xAE\xE0\xA8\xEF\xB0\x8A\xAE\xAF\xA5\xE0\xAD\xA8\xAA\xA0", 30, 0, 6 }, /* reciv-ru */
    { "\x92\xA5\xA0\xE2\xE0\xB0\x98\xA5\xAA\xE1\xAF\xA8\xE0\xA0", 40, 0, 10 }, /* reciv-ru */
    { "\x8A\xAE\xAB\xAB\xA5\xA4\xA6\xB0\x8D\xEC\xEE\xE2\xAE\xAD\xA0", 40, 0, 51 }, /* reciv-ru */
    { "\x91\xAE\xA1\xAE\xE0\xB0\x88\xC0\x91\xC0\xB0\x81\xA0\xE5\xA0", 40, 0, 29 }, /* reciv-ru */
    { "\x8F\xE3\xE2\xA5\xE8\xA5\xE1\xE2\xA2\xA8\xA5\xB0\x84\xA0\xE0\xA2\xA8\xAD\xA0", 30, 0, 49 }, /* reciv-ru */
    { "\x84\xA0\xAC\xA1\xA0\xB0\x83\xE3\xA2\xA5\xE0\xA0", 60, 0, 15 }, /* reciv-ru */
    { "\x94\xA5\xAC\xA8\xAD\xA8\xA7\xAC", 60, 0, 35 }, /* reciv-ru */
    { "\x8F\xE0\xAE\xA5\xAA\xE2\xB0\x8A\xE3\xE0\xE7\xA0\xE2\xAE\xA2\xA0", 60, 0, 27 }, /* reciv-ru */
    { "\x8E\x8E\x8D", 60, 0, 43 }, /* reciv-ru */
    { "\x8F\xE0\xAE\xA3\xE0\xA0\xAC\xAC\xA0\xB0\x82\xAE\xE1\xE2\xAE\xAA", 60, 0, 40 }, /* reciv-ru */
    { "\x82\xAD\xA5\xA7\xA5\xAC\xAD\xEB\xA5\xB0\x96\xA8\xA2\xA8\xAB\xA8\xA7\xA0\xE6\xA8\xA8", 60, 0, 21 }, /* reciv-ru */
    { "\x8B\xA5\xAA\xA0\xE0\xE1\xE2\xA2\xAE\xB0\x8E\xE2\xB0\x90\xA0\xAA\xA0", 60, 0, 59 } /* reciv-ru */
};

int improvement_words[22] = {
    -1, 43, 20, 54, 14, 13, 29, 34, 127, 43, 127, 15, 27, 127, 127, 127, 127, 127, 127, 127, 127, 127
};

/* Recovered DGROUP 1128..1882: unit types and civilization/leader tables. */

struct unit_type {
    char name[12];
    int obsolete_by;
    int domain;
    int w2, w3, w4, w5, w6, w7, w8, w9, w10;
};

struct civ_type {
    char leader[16];
    char plural[16];
    char adjective[16];
    int f0, f1, f2, f3, f4;
};

struct unit_type utypes[28] = {
    { "\x8A\xAE\xAB\xAE\xAD\xA8\xE1\xE2\xEB", 127, 0, 1, 0, 0, 1, 4, 0, 0, 0, -1 }, /* reciv-ru */
    { "\x8C\xA8\xAB\xA8\xE6\xA8\xEF", 34, 0, 1, 0, 1, 1, 1, 0, 0, 2, -1 }, /* reciv-ru */
    { "\x94\xA0\xAB\xA0\xAD\xA3\xA0", 34, 0, 1, 0, 1, 2, 2, 0, 0, 2, 17 }, /* reciv-ru */
    { "\x8B\xA5\xA3\xA8\xAE\xAD", 64, 0, 1, 0, 3, 1, 2, 0, 0, 1, 18 }, /* reciv-ru */
    { "\x8C\xE3\xE8\xAA\xA5\xE2\xA5\xE0\xEB", 64, 0, 1, 0, 2, 3, 3, 0, 0, 2, 34 }, /* reciv-ru */
    { "\x91\xE2\xE0\xA5\xAB\xAA\xA8", 127, 0, 1, 0, 3, 5, 3, 0, 0, 2, 64 }, /* reciv-ru */
    { "\x8A\xA0\xA2\xA0\xAB\xA5\xE0\xA8\xEF", 64, 0, 2, 0, 2, 1, 2, 0, 0, 1, 31 }, /* reciv-ru */
    { "\x90\xEB\xE6\xA0\xE0\xA8", 58, 0, 2, 0, 4, 2, 4, 0, 0, 1, 62 }, /* reciv-ru */
    { "\x8A\xA0\xE2\xA0\xAF\xE3\xAB\xEC\xE2\xA0", 48, 0, 1, 0, 6, 1, 4, 0, 0, 1, 9 }, /* reciv-ru */
    { "\x8F\xE3\xE8\xAA\xA0", 63, 0, 1, 0, 8, 1, 4, 0, 0, 1, 48 }, /* reciv-ru */
    { "\x8A\xAE\xAB\xA5\xE1\xAD\xA8\xE6\xA0", 62, 0, 2, 0, 4, 1, 4, 0, 0, 1, 33 }, /* reciv-ru */
    { "\x92\xA0\xAD\xAA", 127, 0, 3, 0, 10, 5, 8, 0, 0, 1, 58 }, /* reciv-ru */
    { "\x81\x8C\x8F", 127, 0, 3, 0, 6, 6, 5, 0, 0, 2, 65 }, /* reciv-ru */
    { "\x80\xE0\xE2\xA8\xAB\xAB\xA5\xE0\xA8\xEF", 127, 0, 2, 0, 12, 2, 6, 0, 0, 1, 63 }, /* reciv-ru */
    { "\x88\xE1\xE2\xE0\xA5\xA1\xA8\xE2\xA5\xAB\xEC", 127, 1, 10, 1, 4, 2, 6, 2, 0, 4, 38 }, /* reciv-ru */
    { "\x98\xE2\xE3\xE0\xAC\xAE\xA2\xA8\xAA", 127, 1, 8, 2, 12, 1, 12, 2, 0, 1, 39 }, /* reciv-ru */
    { "\x83\xA0\xAB\xA5\xE0\xA0", 8, 2, 3, 0, 1, 0, 4, 0, 2, 5, 7 }, /* reciv-ru */
    { "\x8F\xA0\xE0\xE3\xE1\xAD\xA8\xAA", 14, 2, 3, 0, 1, 1, 4, 0, 3, 5, 8 }, /* reciv-ru */
    { "\x94\xE0\xA5\xA3\xA0\xE2", 35, 2, 3, 0, 2, 2, 4, 0, 4, 5, 14 }, /* reciv-ru */
    { "\x8F\xA0\xE0\xAE\xE5\xAE\xA4", 37, 2, 4, 0, 4, 4, 6, 0, 0, 3, 23 }, /* reciv-ru */
    { "\x8A\xE0\xA5\xA9\xE1\xA5\xE0", 127, 2, 6, 0, 6, 6, 8, 3, 0, 3, 37 }, /* reciv-ru */
    { "\x8B\xA8\xAD\xAA\xAE\xE0", 127, 2, 4, 0, 18, 12, 16, 3, 0, 3, 52 }, /* reciv-ru */
    { "\x8F\xAE\xA4\xAB\xAE\xA4\xAA\xA0", 127, 2, 3, 0, 8, 2, 5, 3, 0, 3, 41 }, /* reciv-ru */
    { "\x80\xA2\xA8\xA0\xAD\xAE\xE1\xA5\xE6", 127, 2, 5, 0, 1, 12, 16, 3, 0, 3, 39 }, /* reciv-ru */
    { "\x92\xE0\xA0\xAD\xE1\xAF\xAE\xE0\xE2", 127, 2, 4, 0, 0, 3, 5, 0, 8, 5, 35 }, /* reciv-ru */
    { "\x80\xE2\xAE\xAC. \xA1\xAE\xAC\xA1\xA0", 127, 1, 16, 1, 99, 0, 16, 0, 0, 1, 46 }, /* reciv-ru */
    { "\x84\xA8\xAF\xAB\xAE\xAC\xA0\xE2", 127, 0, 2, 0, 0, 0, 3, 0, 0, 6, 22 }, /* reciv-ru */
    { "\x8A\xA0\xE0\xA0\xA2\xA0\xAD", 127, 0, 1, 0, 0, 1, 5, 0, 0, 6, 24 }, /* reciv-ru */
};

struct civ_type civt[16] = {
    { "", "", "", 0, 0, 0, 36, 36 },
    { "\x96\xA5\xA7\xA0\xE0\xEC", "\x90\xA8\xAC\xAB\xEF\xAD\xA5", "\x90\xA8\xAC\xAB\xEF\xAD\xA5", 0, 1, 1, 24, 10 }, /* reciv-ru */
    { "\x95\xA0\xAC\xAC\xE3\xE0\xA0\xA1\xA8", "\x82\xA0\xA2\xA8\xAB\xAE\xAD\xEF\xAD\xA5", "\x82\xA0\xA2\xA8\xAB\xAE\xAD\xEF\xAD\xA5", -1, -1, 1, 28, 14 }, /* reciv-ru */
    { "\x94\xE0\xA8\xA4\xE0\xA8\xE5 II", "\x8D\xA5\xAC\xE6\xEB", "\x8D\xA5\xAC\xE6\xEB", 1, -1, 1, 32, 18 }, /* reciv-ru */
    { "\x90\xA0\xAC\xA7\xA5\xE1", "\x85\xA3\xA8\xAF\xE2\xEF\xAD\xA5", "\x85\xA3\xA8\xAF\xE2\xEF\xAD\xA5", 0, 0, 1, 21, 7 }, /* reciv-ru */
    { "\x80\xA2\xE0\xA0\xA0\xAC \x8B\xA8\xAD\xAA\xAE\xAB\xEC\xAD", "\x80\xAC\xA5\xE0\xA8\xAA\xA0\xAD\xE6\xEB", "\x80\xAC\xA5\xE0\xA8\xAA\xA0\xAD\xE6\xEB", -1, 0, 1, 19, 5 }, /* reciv-ru */
    { "\x80\xAB\xA5\xAA\xE1\xA0\xAD\xA4\xE0", "\x83\xE0\xA5\xAA\xA8", "\x83\xE0\xA5\xAA\xA8", 0, 1, -1, 26, 12 }, /* reciv-ru */
    { "\x8C.\x83\xA0\xAD\xA4\xA8", "\x88\xAD\xA4\xA8\xA9\xE6\xEB", "\x88\xAD\xA4\xA8\xA9\xE6\xEB", -1, -1, 0, 31, 17 }, /* reciv-ru */
    { "", "", "", 0, 0, 0, 36, 36 },
    { "\x91\xE2\xA0\xAB\xA8\xAD", "\x90\xE3\xE1\xE1\xAA\xA8\xA5", "\x90\xE3\xE1\xE1\xAA\xA8\xA5", 1, 0, -1, 25, 11 }, /* reciv-ru */
    { "\x8B\xE3\xAC\xE3\xAC\xA1\xA0", "\x8D\xA5\xA3\xE0\xEB", "\x8D\xA5\xA3\xE0\xEB", 1, 0, 0, 22, 8 }, /* reciv-ru */
    { "\x8D\xA0\xAF\xAE\xAB\xA5\xAE\xAD", "\x94\xE0\xA0\xAD\xE6\xE3\xA7\xEB", "\x94\xE0\xA0\xAD\xE6\xE3\xA7\xEB", 1, 1, 1, 23, 9 }, /* reciv-ru */
    { "\x8C\xAE\xAD\xE2\xA5\xA7\xE3\xAC\xA0", "\x80\xE6\xE2\xA5\xAA\xA8", "\x80\xE6\xE2\xA5\xAA\xA8", 0, -1, 1, 20, 6 }, /* reciv-ru */
    { "\x8C\xA0\xAE \x84\xA7\xA5 \x84\xE3\xAD", "\x8A\xA8\xE2\xA0\xA9\xE6\xEB", "\x8A\xA8\xE2\xA0\xA9\xE6\xEB", 0, 0, 1, 29, 15 }, /* reciv-ru */
    { "\x85\xAB\xA8\xA7\xA0\xA2\xA5\xE2\xA0 I", "\x80\xAD\xA3\xAB\xA8\xE7\xA0\xAD\xA5", "\x80\xAD\xA3\xAB\xA8\xE7\xA0\xAD\xA5", 0, 1, 0, 27, 13 }, /* reciv-ru */
    { "\x97\xA8\xAD\xA3\xA8\xA7  \x95\xA0\xAD", "\x8C\xAE\xAD\xA3\xAE\xAB\xEB", "\x8C\xAE\xAD\xA3\xAE\xAB\xEB", 1, 1, -1, 30, 16 }, /* reciv-ru */
};

int spiral_x[49] = {
    0, 0, 1, 1, 1, 0, -1, -1, -1, 0, 1, 2, 2, 2, 1, 0,
    -1, -2, -2, -2, -1, 2, 2, -2, -2, 0, 1, 2, 3, 3, 3, 3,
    3, 2, 1, 0, -1, -2, -3, -3, -3, -3, -3, -2, -1, 3, 3, -3,
    -3
};

int spiral_y[49] = {
    0, -1, -1, 0, 1, 1, 1, 0, -1, -2, -2, -1, 0, 1, 2, 2,
    2, 1, 0, -1, -2, 2, -2, -2, 2, -3, -3, -3, -2, -1, 0, 1,
    2, 3, 3, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, 3, -3, 3,
    -3
};

int civ_colours[8] = {
    12, 15, 10, 9, 14, 11, 13, 7
};

int civ_shadows[8] = {
    4, 7, 2, 1, 10, 3, 4, 8
};

char *government_names[6] = {
    "\x80\xAD\xA0\xE0\xE5\xA8\xEF", /* reciv-ru */
    "\x84\xA5\xE1\xAF\xAE\xE2\xA8\xA7\xAC", /* reciv-ru */
    "\x8C\xAE\xAD\xA0\xE0\xE5\xA8\xEF", /* reciv-ru */
    "\x8A\xAE\xAC\xAC\xE3\xAD\xA8\xA7\xAC", /* reciv-ru */
    "\x90\xA5\xE1\xAF\xE3\xA1\xAB.", /* reciv-ru */
    "\x84\xA5\xAC\xAE\xAA\xE0\xA0\xE2\xA8\xEF" /* reciv-ru */
};

char *trade_goods[8] = {
    "\x98\xA5\xAB\xAA", /* reciv-ru */
    "\x80\xAB\xAC\xA0\xA7\xEB", /* reciv-ru */
    "\x82\xA8\xAD\xAE", /* reciv-ru */
    "\x8C\xA5\xA4\xEC", /* reciv-ru */
    "\x80\xAB\xAC\xA0\xA7\xEB", /* reciv-ru */
    "\x97\xA0\xA9", /* reciv-ru */
    "\x91\xAE\xAB\xEC", /* reciv-ru */
    "\x8F\xA5\xE0\xA5\xE6" /* reciv-ru */
};

/* reciv: one block, as the original's literals were: the save file
 * transfers all 8 * 11 bytes through civ_adjectives[0]. */
char civ_adjective_text[8][11] = {
    "tX\x8Bv\x06\xB1\x05\xD3\xE6\x8B^", /* reciv-ru */
    "\x90\xA8\xAC\xAB\xEF\xAD\xA5", /* reciv-ru */
    "\x8D\xA5\xA3\xE0\xEB", /* reciv-ru */
    "\x8C\xAE\xAD\xA3\xAE\xAB\xEB", /* reciv-ru */
    "\x80\xE6\xE2\xA5\xAA\xA8", /* reciv-ru */
    "\xF0\xB8\x0C\x00\xF7n\xAC\x03\xF0\x80\xBC", /* reciv-ru */
    "\xF7\x90\xFFt\xDC\xF6\x84\xF4\x90\x10u", /* reciv-ru */
    "\xD5\x80\xBC\xF7\x90\x19t\x03\xE9\xB8\xFE" /* reciv-ru */
};

char *civ_adjectives[8] = {
    civ_adjective_text[0],
    civ_adjective_text[1],
    civ_adjective_text[2],
    civ_adjective_text[3],
    civ_adjective_text[4],
    civ_adjective_text[5],
    civ_adjective_text[6],
    civ_adjective_text[7]
};

/* reciv: one block, as the original's literals were: the save file
 * transfers all 8 * 12 bytes through civ_names[0]. */
char civ_name_text[8][12] = {
    "\x90\xA8\xAC\xAB\xEF\xAD\xA5    ", /* reciv-ru */
    "\x82\xA0\xA2\xA8\xAB\xAE\xAD\xEF\xAD\xA5 ", /* reciv-ru */
    "\x8D\xA5\xAC\xE6\xEB      ", /* reciv-ru */
    "\x8D\xA5\xAC\xE6\xEB", /* reciv-ru */
    "\x80\xAC\xA5\xE0\xA8\xAA\xA0\xAD\xE6\xEB ", /* reciv-ru */
    "\x83\xE0\xA5\xAA\xA8      ", /* reciv-ru */
    "\x8C\xAE\xAD\xA3\xAE\xAB\xEB    ", /* reciv-ru */
    "\x80\xE2\xA8\xAB\xAB\xA0       " /* reciv-ru */
};

char *civ_names[8] = {
    civ_name_text[0],
    civ_name_text[1],
    civ_name_text[2],
    civ_name_text[3],
    civ_name_text[4],
    civ_name_text[5],
    civ_name_text[6],
    civ_name_text[7]
};

/* reciv: one block, as the original's literals were: the save file
 * transfers all 8 * 14 bytes through leader_names[0]. */
char leader_name_text[8][14] = {
    "\x96\xA5\xA7\xA0\xE0\xEC       ", /* reciv-ru */
    "\x96\xA5\xA7\xA0\xE0\xEC", /* reciv-ru */
    "\x8B\xE3\xAC\xE3\xAC\xA1\xA0", /* reciv-ru */
    "\x97\xA8\xAD\xA3\xA8\xA7  \x95\xA0\xAD", /* reciv-ru */
    "\x8C\xAE\xAD\xE2\xA5\xA7\xE3\xAC\xA0", /* reciv-ru */
    "\x8A\xE1\xA5\xE0\xAA\xE1       ", /* reciv-ru */
    "\x91\xA0\xAB\xA0\xA4\xA8\xAD      ", /* reciv-ru */
    "\x91\xE3\xAF\xAF\xA8\xAB\xE3\xAB\xA8\xE3\xAC\xA0\xE1" /* reciv-ru */
};

char *leader_names[8] = {
    leader_name_text[0],
    leader_name_text[1],
    leader_name_text[2],
    leader_name_text[3],
    leader_name_text[4],
    leader_name_text[5],
    leader_name_text[6],
    leader_name_text[7]
};

char *leader_titles[6] = {
    "\x83-\xAD", /* reciv-ru */
    "\x82\xAB\xA0\xA4\xEB\xAA\xA0", /* reciv-ru */
    "\x96\xA0\xE0\xEC ", /* reciv-ru */
    "\x92\xAE\xA2\xC0", /* reciv-ru */
    "\x8F\xE0\xA5\xA7\xA8\xA4\xA5\xAD\xE2", /* reciv-ru */
    "\x8F\xE0\xA5\xA7\xA8\xA4\xA5\xAD\xE2" /* reciv-ru */
};

int selected_index = -1;
int debug = 2;

struct page page1 = { 1, 0, 0, 319, 199, 0, 15, 4, 0 };

struct page *back_page = &page1;
struct page page2 = { 2, 0, 0, 319, 199, 0, 15, 4, 0 };

struct page *tile_page = &page2;
struct page page3 = { 3, 0, 0, 319, 199, 0, 15, 4, 0 };

struct page *spare_page = &page3;

char ega_palette[17] = { 0, 1, 2, 3, 4, 0, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0 };
char ega_default_palette[17] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0 };

char graphics_driver[] = "mgraphic.exe";
char sound_driver[] = "nsound.cvl";
int has_mouse = 1;
int low_memory = 0;

/*
 * ## main
 *
 * Game entry point.
 * Initializes the runtime environment, drivers, screen, memory,
 * displays the opening menu, and runs the main turn loop.
 */
int main(void)
{
    int j, k, n, memory, i;

    run_program(load_program("misc.exe", 0));
    setup_main();
    *(char far *) MK_FP(0x0040, 0x0017) &= 0xdf;    /* reciv: was (char far *) 0x00400017L */
    install_break_handlers();
    game_startup();
    disk_insert(0, 1L);
    memory = free_paragraphs();
    if (small_space_check)
        memory = 0;
    if ((vga_modes ? 8000 : 4000) > memory) {
        rect_fill(scrbuf, 0, 0, 320, 200, 12);
        msgtext[0] = 0;
        strcat(msgtext, itoa(memory, nstr, 10));
        strcpy(message_slot, msgtext);
        msgtext[0] = 0;
        named_msg("*LOMEM");
        show_menu(msgtext, 64, 49);
    }
    if (vga_modes && memory < 9150)
        low_memory = 1;
    sprite_arena_init();
    mouse_cursor = load_cursor("torch.pic");
    sprite_arena_release();
    if (has_mouse)
        has_mouse = init_mouse();
    if (has_mouse) {
        set_mousecur(0, 0, mouse_cursor);
        mouse_on();
    }
    game_menu();
    start_game();
    if (main_option_result == 1) {
        map_x0 = x_cap[civnow] - 7;
        map_y0 = 19;
        sel_player();
    }
    leave = 0;
    do
        game_turn();
    while (!leave);
    if (has_mouse)
        mouse_close();
    sound(0);
    sound_shutdown();
    restore_timer();
    restore_break_handlers();
    bios_call_reg_block_io.h.ah = 0;
    bios_call_reg_block_io.h.al = 3;
    int86(0x10, &bios_call_reg_block_io, &bios_call_reg_block_io);
}

void empty_hook(void)
{
}

void set_screen_mode(int mode)
{
    if (page_ready(0)) {
        back_page->number = mode;
        blit(scrbuf, 0, 0, 320, 200, back_page, 0, 0);
        set_page(mode);
        back_page->number = scrbuf->number ^ 1;
        flip(1);
    }
}

void poll_mouse(void)
{
    if (has_mouse) {
        mse_held = mouse_poll() | mouse_info_a;
        ptr_x = mouse_info_b;
        ptr_y = mouse_info_c;
    } else {
        mse_held = ptr_x = ptr_y = 0;
    }
}

void mouse_on(void)
{
    pointer_hide_count++;
    if (has_mouse && pointer_hide_count == 1)
        mouse_show();
}

void mouse_off(void)
{
    if (has_mouse && pointer_hide_count == 1)
        mouse_hide();
    pointer_hide_count--;
}

void mouse_force_on(void)
{
    mouse_hide_limit = pointer_hide_count;
    while (pointer_hide_count < 1)
        mouse_on();
}

void mouse_restore(void)
{
    while (pointer_hide_count > mouse_hide_limit)
        mouse_off();
}

char *file_names[8] = {
    "civ.exe",
    "docker.pic",
    "govt0a.pic",
    "blurb0.txt",
    "king00.pic",
    "king08.pic",
    "fonts.cv",
    "birth2.pic"
};

char disk_digit = 'A';
int disk_swap = 0;

/* Calls use differing argument counts; retain this interface; NOLINTBEGIN(clang-diagnostic-deprecated-non-prototype) */
disk_insert(file, mode)
int file, mode;
{
    int old_style;
    char buffer[256];
    if (mode == 0) {
        if (file_exists(file_names[file], 0) != -1) {
            which_data = file;
            return 1;
        }
        file = 0;
    }
    while (file_exists(file_names[file], 0) == -1 || mode == -1) {
        set_page(scrbuf->number);
        if (mode == -1)
            mode = 1;
        mouse_off();
        rect_fill(scrbuf, 0, 0, 320, 200, 15);
        disk_digit = file / found_floppy + '1';
        strcpy(buffer, "\x82\xE1\xE2\xA0\xA2\xEC\xE2\xA5 \xA4\xA8\xE1\xAA "); /* reciv-ru */
        strcat(buffer, &disk_digit);
        strcat(buffer, ".\n \n  \x8D\xA0\xA6\xAC\xA8\xE2\xA5 Enter\n \xA4\xAB\xEF \xAF\xE0\xAE\xA4\xAE\xAB\xA6\xA5\xAD\xA8\xEF.\n"); /* reciv-ru */
        old_style = style;
        style = -1;
        box_info_flags = 1;
        show_menu(buffer, 100, 81);
        style = old_style;
        rect_fill(scrbuf, 0, 0, 320, 200, 15);
        disk_swap = 1;
        mouse_on();
    }
    which_data = file;
    return mode;
}

/* NOLINTEND(clang-diagnostic-deprecated-non-prototype) */

void interrupt far break_handler(void)
{
}

void install_break_handlers(void)
{
    orig_break_isr = _dos_getvect(0x1b);
    int_23 = _dos_getvect(0x23);
    _dos_setvect(0x1b, break_handler);
    _dos_setvect(0x23, break_handler);
}

void restore_break_handlers(void)
{
    _dos_setvect(0x1b, orig_break_isr);
    _dos_setvect(0x23, int_23);
}

/* Calls use differing argument counts; retain this interface; NOLINTBEGIN(clang-diagnostic-deprecated-non-prototype) */
sound(a, b)
int a, b;
{
    if (debug & 0x10)
        sound_command(a, b);
}

/* NOLINTEND(clang-diagnostic-deprecated-non-prototype) */

void game_menu(void)
{
    int seed, i, j;

    beginner_help = 0;
    goto test;
    do {
        strcpy(msgtext, " \x8D\xAE\xA2\xA0\xEF \xA8\xA3\xE0\xA0\n \x82\xAE\xE1\xE1\xE2\xA0\xAD\xAE\xA2\xA8\xE2\xEC \xAF\xE0\xA5\xA6\xAD\xEE\xEE\n \x87\x85\x8C\x8B\x9F\n \x91\xAE\xA7\xA4\xA0\xE2\xEC \xAD\xAE\xA2\xEB\xA9 \xAC\xA8\xE0\n \x8A\xAD\xA8\xA3\xA0 \xE0\xA5\xAA\xAE\xE0\xA4\xAE\xA2 CIV\n"); /* reciv-ru */
        main_option_result = show_menu(msgtext, 100, 140);
test:  ;
    } while (main_option_result == -1);
    do
        seed = sound_quiet_ticks();
    while (seed > 5269 && (seed - 5269) % 288 != 0);
    sound(1);
    switch (main_option_result) {
    case 0:
        mouse_off();
        for (i = 0; i < 4; i++)
            custom_params[i] = 1;
start:
        make_world();
        scrbuf->number = 0;
        set_page(0);
        load_screen(1, "sp299.pic");
        blit(back_page, 160, 50, 160, 150, tile_page, 160, 50);
        disk_insert(0, 1L);
        break;
    case 3:
        mouse_off();
        disk_insert(7, 1L);
        if (vga_modes)
            set_palette(5, 0, 0, 0);
        rect_fill(scrbuf, 0, 0, 320, 200, 0);
        load_picture(1, "custom.pic");
        disk_insert(0, 1L);
        blit(back_page, 0, 0, 320, 200, scrbuf, 0, 0);
        mouse_on();
        editing = 1;
        for (;;) {
            set_text_pos(210, 11);
            strcpy(msgtext, "\x91\xE3\xE8\xA0:\n \xAC\xA0\xAB\xAE\n \xE1\xE0\xA5\xA4\xAD\xA5\n \xAC\xAD\xAE\xA3\xAE\n"); /* reciv-ru */
            custom_params[0] = show_menu(msgtext, 200, 1);
            if (custom_params[0] == -1)
                continue;
            if (edit_mode) {
                show_help("Land");
                editing = custom_params[0];
                continue;
            }
            break;
        }
        editing = 1;
        for (;;) {
            set_text_pos(210, 61);
            strcpy(msgtext, "\x92\xA5\xAC\xAF\xA5\xE0\xA0\xE2\xE3\xE0\xA0:\n \x95\xAE\xAB\xAE\xA4\xAD\xAE\n \x92\xA5\xAF\xAB\xAE\n \x86\xA0\xE0\xAA\xAE\n"); /* reciv-ru */
            custom_params[1] = show_menu(msgtext, 200, 51);
            if (custom_params[1] == -1)
                continue;
            if (edit_mode) {
                show_help("Temperature");
                editing = custom_params[1];
                continue;
            }
            break;
        }
        editing = 1;
        for (;;) {
            set_text_pos(210, 111);
            strcpy(msgtext, "\x8A\xAB\xA8\xAC\xA0\xE2:\n \x91\xE3\xE5\xAE\xA9\n \x8D\xAE\xE0\xAC.\n \x82\xAE\xA4\xA0\n"); /* reciv-ru */
            custom_params[2] = show_menu(msgtext, 200, 101);
            if (custom_params[2] == -1)
                continue;
            if (edit_mode) {
                show_help("Climate");
                editing = custom_params[2];
                continue;
            }
            editing = 1;
            do {
                set_text_pos(210, 161);
                strcpy(msgtext, "\x9D\xE0\xA0:\n 3 \xAC\xA8\xAB\xAB\xA8\xA0\xE0\xA4\xA0 \xAB\xA5\xE2\n 4 \xAC\xA8\xAB\xAB\xA8\xA0\xE0\xA4\xA0 \xAB\xA5\xE2\n 5 \xAC\xA8\xAB\xAB\xA8\xA0\xE0\xA4\xAE\xA2   \n"); /* reciv-ru */
                custom_params[3] = show_menu(msgtext, 200, 151);
            } while (custom_params[3] == -1);
            if (edit_mode) {
                show_help("Age");
                editing = custom_params[3];
                continue;
            }
            break;
        }
        mouse_off();
        goto start;
    case 2:
        mouse_off();
        beginner_help = 1;
        make_world();
        scrbuf->number = 0;
        set_page(0);
        load_screen(1, "sp299.pic");
        blit(back_page, 160, 50, 160, 150, tile_page, 160, 50);
        disk_insert(0, 1L);
        break;
    case 1:
        if (load_game(-1) == -1)
            goto reset;
        mouse_off();
        rect_fill(scrbuf, 0, 0, 320, 200, 7);
        break;
    case 4:
        mouse_off();
        fame_load();
        show_fame(-1);
        mouse_on();
reset:
        main_option_result = -1;
        goto test;
    }
    setup_art();
    busy_flag = 1;
    if (vga_modes && (main_option_result == 0 || main_option_result == 2 || main_option_result == 3))
        set_palette(5, 0, 0, 0);
    rect_fill(scrbuf, 0, 0, 320, 200, 0);
    load_back("sp257.pic");
    setup_cycles();
    mouse_on();
}

void start_game(void)
{
    int a, b, c;

    if (has_mouse)
        set_mousecur(0, 0, splashhand);
    if (!turn_tick) {
        load_picture(1, "diffs.pic");
        disk_insert(0, 1L);
        new_game();
    }
    cabinet();
    disk_insert(0, 1L);
}

void set_cursor(int row, int column)
{
    bios_call_reg_block_io.h.ah = 2;
    bios_call_reg_block_io.h.bh = 0;
    bios_call_reg_block_io.h.dh = row;
    bios_call_reg_block_io.h.dl = column;
    int86(0x10, &bios_call_reg_block_io, &bios_call_reg_block_io);
}

/* Far world state. These uninitialized far arrays are communals, which LINK
   lays out in FAR_BSS by its symbol hash bucket, (length + sum of
   (char | 0x20) << (position & 3)) & 0xFF, not in declaration order. The
   original names are unknown; these ascend in that order. Other modules
   declare them extern with the types they need. */
char far power_history[150][8];
int far advance_owners[72];
char far city_site_x[256];
char far city_site_y[256];
char far map_visible_to[80][50];
char far spaceships[8][15][12];
int far pedia_topics[100];
signed char far adv_source[8][72];
char far war_record[150][8];
char far fog_map[20][13];
int far unit_losses[8][28];
unsigned char far replay_log[4096];
int far topic_class[84];
char far fame_ranks[332];
