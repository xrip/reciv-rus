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
    { TRANSLATE("Desert", "RU"), 1, 2, 0, 1, 0, 1, 14 }, /* reciv-ru */
    { TRANSLATE("Plains", "RU"), 1, 2, 1, 1, 0, 1, 6 }, /* reciv-ru */
    { TRANSLATE("Grassland", "RU"), 1, 2, 2, 1, 0, 1, 10 }, /* reciv-ru */
    { TRANSLATE("Forest", "RU"), 2, 3, 1, 2, 0, 2, 2 }, /* reciv-ru */
    { TRANSLATE("Hills", "RU"), 2, 4, 1, 0, 0, 2, 12 }, /* reciv-ru */
    { TRANSLATE("Mountains", "RU"), 3, 6, 0, 1, 0, 3, 13 }, /* reciv-ru */
    { TRANSLATE("Tundra", "RU"), 1, 2, 1, 0, 0, 0, 7 }, /* reciv-ru */
    { TRANSLATE("Arctic", "RU"), 2, 2, 0, 0, 0, 0, 15 }, /* reciv-ru */
    { TRANSLATE("Swamp", "RU"), 2, 3, 1, 0, 0, 0, 3 }, /* reciv-ru */
    { TRANSLATE("Jungle", "RU"), 2, 3, 1, 0, 0, 0, 11 }, /* reciv-ru */
    { TRANSLATE("Ocean", "RU"), 1, 2, 1, 0, 2, 0, 1 }, /* reciv-ru */
    { TRANSLATE("River", "RU"), 1, 3, 2, 1, 1, 2, 9 }, /* reciv-ru */
    { TRANSLATE("Oasis", "RU"), 1, 2, 3, 1, 0, 1, 14 }, /* reciv-ru */
    { TRANSLATE("Horses", "RU"), 1, 2, 1, 3, 0, 1, 6 }, /* reciv-ru */
    { TRANSLATE("Grassland", "RU"), 1, 2, 2, 1, 0, 1, 10 }, /* reciv-ru */
    { TRANSLATE("Game", "RU"), 2, 3, 3, 2, 0, 2, 2 }, /* reciv-ru */
    { TRANSLATE("Coal", "RU"), 2, 4, 1, 2, 0, 2, 12 }, /* reciv-ru */
    { TRANSLATE("Gold", "RU"), 3, 6, 0, 1, 6, 3, 13 }, /* reciv-ru */
    { TRANSLATE("Game", "RU"), 1, 2, 3, 0, 0, 0, 7 }, /* reciv-ru */
    { TRANSLATE("Seals", "RU"), 2, 2, 2, 0, 0, 0, 15 }, /* reciv-ru */
    { TRANSLATE("Oil", "RU"), 2, 3, 1, 4, 0, 0, 3 }, /* reciv-ru */
    { TRANSLATE("Gems", "RU"), 2, 3, 1, 0, 4, 0, 11 }, /* reciv-ru */
    { TRANSLATE("Fish", "RU"), 1, 2, 3, 0, 2, 0, 1 }, /* reciv-ru */
    { TRANSLATE("River", "RU"), 1, 3, 2, 1, 1, 2, 9 }, /* reciv-ru */
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
    { TRANSLATE("Alphabet", "RU"), -1, -1 }, /* reciv-ru */
    { TRANSLATE("Code of Laws", "RU"), 0, -1 }, /* reciv-ru */
    { TRANSLATE("Currency", "RU"), 17, -1 }, /* reciv-ru */
    { TRANSLATE("Atomic Theory", "RU"), 51, 11 }, /* reciv-ru */
    { TRANSLATE("Democracy", "RU"), 28, 30 }, /* reciv-ru */
    { TRANSLATE("Monarchy", "RU"), 25, 1 }, /* reciv-ru */
    { TRANSLATE("Astronomy", "RU"), 26, 9 }, /* reciv-ru */
    { TRANSLATE("MapMaking", "RU"), 0, -1 }, /* reciv-ru */
    { TRANSLATE("Navigation", "RU"), 7, 6 }, /* reciv-ru */
    { TRANSLATE("Mathematics", "RU"), 0, 16 }, /* reciv-ru */
    { TRANSLATE("Medicine", "RU"), 28, 24 }, /* reciv-ru */
    { TRANSLATE("Physics", "RU"), 9, 8 }, /* reciv-ru */
    { TRANSLATE("Engineering", "RU"), 33, 45 }, /* reciv-ru */
    { TRANSLATE("University", "RU"), 9, 28 }, /* reciv-ru */
    { TRANSLATE("Magnetism", "RU"), 8, 11 }, /* reciv-ru */
    { TRANSLATE("Electronics", "RU"), 12, 54 }, /* reciv-ru */
    { TRANSLATE("Masonry", "RU"), -1, -1 }, /* reciv-ru */
    { TRANSLATE("Bronze Working", "RU"), -1, -1 }, /* reciv-ru */
    { TRANSLATE("Iron Working", "RU"), 17, -1 }, /* reciv-ru */
    { TRANSLATE("Bridge Building", "RU"), 18, 45 }, /* reciv-ru */
    { TRANSLATE("Invention", "RU"), 12, 30 }, /* reciv-ru */
    { TRANSLATE("Computers", "RU"), 9, 15 }, /* reciv-ru */
    { TRANSLATE("Writing", "RU"), 0, -1 }, /* reciv-ru */
    { TRANSLATE("Steam Engine", "RU"), 11, 20 }, /* reciv-ru */
    { TRANSLATE("Trade", "RU"), 2, 1 }, /* reciv-ru */
    { TRANSLATE("Ceremonial Burial", "RU"), -1, -1 }, /* reciv-ru */
    { TRANSLATE("Mysticism", "RU"), 25, -1 }, /* reciv-ru */
    { TRANSLATE("Nuclear Fission", "RU"), 41, 3 }, /* reciv-ru */
    { TRANSLATE("Philosophy", "RU"), 26, 30 }, /* reciv-ru */
    { TRANSLATE("Religion", "RU"), 28, 22 }, /* reciv-ru */
    { TRANSLATE("Literacy", "RU"), 22, 1 }, /* reciv-ru */
    { TRANSLATE("Horseback Riding", "RU"), -1, -1 }, /* reciv-ru */
    { TRANSLATE("Feudalism", "RU"), 16, 5 }, /* reciv-ru */
    { TRANSLATE("The Wheel", "RU"), -1, -1 }, /* reciv-ru */
    { TRANSLATE("Gunpowder", "RU"), 20, 18 }, /* reciv-ru */
    { TRANSLATE("Industrialization", "RU"), 49, 53 }, /* reciv-ru */
    { TRANSLATE("Chemistry", "RU"), 13, 10 }, /* reciv-ru */
    { TRANSLATE("Combustion", "RU"), 55, 56 }, /* reciv-ru */
    { TRANSLATE("Flight", "RU"), 37, 11 }, /* reciv-ru */
    { TRANSLATE("Advanced Flight", "RU"), 38, 54 }, /* reciv-ru */
    { TRANSLATE("Space Flight", "RU"), 21, 46 }, /* reciv-ru */
    { TRANSLATE("Mass Production", "RU"), 58, 47 }, /* reciv-ru */
    { TRANSLATE("Pottery", "RU"), -1, -1 }, /* reciv-ru */
    { TRANSLATE("Communism", "RU"), 28, 35 }, /* reciv-ru */
    { TRANSLATE("The Republic", "RU"), 1, 30 }, /* reciv-ru */
    { TRANSLATE("Construction", "RU"), 16, 2 }, /* reciv-ru */
    { TRANSLATE("Rocketry", "RU"), 39, 15 }, /* reciv-ru */
    { TRANSLATE("The Corporation", "RU"), 53, 35 }, /* reciv-ru */
    { TRANSLATE("Metallurgy", "RU"), 34, 13 }, /* reciv-ru */
    { TRANSLATE("RailRoad", "RU"), 23, 19 }, /* reciv-ru */
    { TRANSLATE("Nuclear Power", "RU"), 27, 15 }, /* reciv-ru */
    { TRANSLATE("Theory of Gravity", "RU"), 6, 13 }, /* reciv-ru */
    { TRANSLATE("Steel", "RU"), 48, 35 }, /* reciv-ru */
    { TRANSLATE("Banking", "RU"), 24, 44 }, /* reciv-ru */
    { TRANSLATE("Electricity", "RU"), 48, 14 }, /* reciv-ru */
    { TRANSLATE("Refining", "RU"), 36, 47 }, /* reciv-ru */
    { TRANSLATE("Explosives", "RU"), 34, 36 }, /* reciv-ru */
    { TRANSLATE("SuperConductor", "RU"), 60, 41 }, /* reciv-ru */
    { TRANSLATE("Automobile", "RU"), 37, 52 }, /* reciv-ru */
    { TRANSLATE("Genetic Engineering", "RU"), 10, 47 }, /* reciv-ru */
    { TRANSLATE("Plastics", "RU"), 55, 40 }, /* reciv-ru */
    { TRANSLATE("Recycling", "RU"), 41, 4 }, /* reciv-ru */
    { TRANSLATE("Chivalry", "RU"), 32, 31 }, /* reciv-ru */
    { TRANSLATE("Robotics", "RU"), 60, 21 }, /* reciv-ru */
    { TRANSLATE("Conscription", "RU"), 44, 56 }, /* reciv-ru */
    { TRANSLATE("Labor Union", "RU"), 41, 43 }, /* reciv-ru */
    { TRANSLATE("Fusion Power", "RU"), 50, 57 }, /* reciv-ru */
    { "1", 127, 127 },
    { "2", 127, 127 },
    { "3", 127, 127 },
    { "4", 127, 127 },
    { TRANSLATE("Future Tech.", "RU"), 127, 127 } /* reciv-ru */
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
    { TRANSLATE("NONE", "RU"), 8, 0, -1 }, /* reciv-ru */
    { TRANSLATE("Palace", "RU"), 20, 5, 16 }, /* reciv-ru */
    { TRANSLATE("Barracks", "RU"), 4, 0, -1 }, /* reciv-ru */
    { TRANSLATE("Granary", "RU"), 6, 1, 42 }, /* reciv-ru */
    { TRANSLATE("Temple", "RU"), 4, 1, 25 }, /* reciv-ru */
    { TRANSLATE("MarketPlace", "RU"), 8, 1, 2 }, /* reciv-ru */
    { TRANSLATE("Library", "RU"), 8, 1, 22 }, /* reciv-ru */
    { TRANSLATE("Courthouse", "RU"), 8, 1, 1 }, /* reciv-ru */
    { TRANSLATE("City Walls", "RU"), 12, 2, 16 }, /* reciv-ru */
    { TRANSLATE("Aqueduct", "RU"), 12, 2, 45 }, /* reciv-ru */
    { TRANSLATE("Bank", "RU"), 12, 3, 53 }, /* reciv-ru */
    { TRANSLATE("Cathedral", "RU"), 16, 3, 29 }, /* reciv-ru */
    { TRANSLATE("University", "RU"), 16, 3, 13 }, /* reciv-ru */
    { TRANSLATE("Mass Transit", "RU"), 16, 4, 41 }, /* reciv-ru */
    { TRANSLATE("Colosseum", "RU"), 10, 4, 45 }, /* reciv-ru */
    { TRANSLATE("Factory", "RU"), 20, 4, 35 }, /* reciv-ru */
    { TRANSLATE("Mfg. Plant", "RU"), 32, 6, 63 }, /* reciv-ru */
    { TRANSLATE("SDI Defense", "RU"), 20, 4, 57 }, /* reciv-ru */
    { TRANSLATE("Recycling Cntr.", "RU"), 20, 2, 61 }, /* reciv-ru */
    { TRANSLATE("Power Plant", "RU"), 16, 4, 55 }, /* reciv-ru */
    { TRANSLATE("Hydro Plant", "RU"), 24, 4, 15 }, /* reciv-ru */
    { TRANSLATE("Nuclear Plant", "RU"), 16, 2, 50 }, /* reciv-ru */
    { TRANSLATE("SS Structural", "RU"), 8, 0, 40 }, /* reciv-ru */
    { TRANSLATE("SS Component", "RU"), 16, 0, 60 }, /* reciv-ru */
    { TRANSLATE("SS Module", "RU"), 32, 0, 63 }, /* reciv-ru */
    { TRANSLATE("Pyramids", "RU"), 30, 0, 16 }, /* reciv-ru */
    { TRANSLATE("Hanging Gardens", "RU"), 30, 0, 42 }, /* reciv-ru */
    { TRANSLATE("Colossus", "RU"), 20, 0, 17 }, /* reciv-ru */
    { TRANSLATE("Lighthouse", "RU"), 20, 0, 7 }, /* reciv-ru */
    { TRANSLATE("Great Library", "RU"), 30, 0, 30 }, /* reciv-ru */
    { TRANSLATE("Oracle", "RU"), 30, 0, 26 }, /* reciv-ru */
    { TRANSLATE("Great Wall", "RU"), 30, 0, 16 }, /* reciv-ru */
    { TRANSLATE("Magellan's Expedition", "RU"), 40, 0, 8 }, /* reciv-ru */
    { TRANSLATE("Michelangelo's Chapel", "RU"), 30, 0, 29 }, /* reciv-ru */
    { TRANSLATE("Copernicus' Observatory", "RU"), 30, 0, 6 }, /* reciv-ru */
    { TRANSLATE("Shakespeare's Theatre", "RU"), 40, 0, 10 }, /* reciv-ru */
    { TRANSLATE("Isaac Newton's College", "RU"), 40, 0, 51 }, /* reciv-ru */
    { TRANSLATE("J.S.Bach's Cathedral", "RU"), 40, 0, 29 }, /* reciv-ru */
    { TRANSLATE("Darwin's Voyage", "RU"), 30, 0, 49 }, /* reciv-ru */
    { TRANSLATE("Hoover Dam", "RU"), 60, 0, 15 }, /* reciv-ru */
    { TRANSLATE("Women's Suffrage", "RU"), 60, 0, 35 }, /* reciv-ru */
    { TRANSLATE("Manhattan Project", "RU"), 60, 0, 27 }, /* reciv-ru */
    { TRANSLATE("United Nations", "RU"), 60, 0, 43 }, /* reciv-ru */
    { TRANSLATE("Apollo Program", "RU"), 60, 0, 40 }, /* reciv-ru */
    { TRANSLATE("SETI Program", "RU"), 60, 0, 21 }, /* reciv-ru */
    { TRANSLATE("Cure for Cancer", "RU"), 60, 0, 59 } /* reciv-ru */
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
    { TRANSLATE("Settlers", "RU"), 127, 0, 1, 0, 0, 1, 4, 0, 0, 0, -1 }, /* reciv-ru */
    { TRANSLATE("Militia", "RU"), 34, 0, 1, 0, 1, 1, 1, 0, 0, 2, -1 }, /* reciv-ru */
    { TRANSLATE("Phalanx", "RU"), 34, 0, 1, 0, 1, 2, 2, 0, 0, 2, 17 }, /* reciv-ru */
    { TRANSLATE("Legion", "RU"), 64, 0, 1, 0, 3, 1, 2, 0, 0, 1, 18 }, /* reciv-ru */
    { TRANSLATE("Musketeers", "RU"), 64, 0, 1, 0, 2, 3, 3, 0, 0, 2, 34 }, /* reciv-ru */
    { TRANSLATE("Riflemen", "RU"), 127, 0, 1, 0, 3, 5, 3, 0, 0, 2, 64 }, /* reciv-ru */
    { TRANSLATE("Cavalry", "RU"), 64, 0, 2, 0, 2, 1, 2, 0, 0, 1, 31 }, /* reciv-ru */
    { TRANSLATE("Knights", "RU"), 58, 0, 2, 0, 4, 2, 4, 0, 0, 1, 62 }, /* reciv-ru */
    { TRANSLATE("Catapult", "RU"), 48, 0, 1, 0, 6, 1, 4, 0, 0, 1, 9 }, /* reciv-ru */
    { TRANSLATE("Cannon", "RU"), 63, 0, 1, 0, 8, 1, 4, 0, 0, 1, 48 }, /* reciv-ru */
    { TRANSLATE("Chariot", "RU"), 62, 0, 2, 0, 4, 1, 4, 0, 0, 1, 33 }, /* reciv-ru */
    { TRANSLATE("Armor", "RU"), 127, 0, 3, 0, 10, 5, 8, 0, 0, 1, 58 }, /* reciv-ru */
    { TRANSLATE("Mech. Inf.", "RU"), 127, 0, 3, 0, 6, 6, 5, 0, 0, 2, 65 }, /* reciv-ru */
    { TRANSLATE("Artillery", "RU"), 127, 0, 2, 0, 12, 2, 6, 0, 0, 1, 63 }, /* reciv-ru */
    { TRANSLATE("Fighter", "RU"), 127, 1, 10, 1, 4, 2, 6, 2, 0, 4, 38 }, /* reciv-ru */
    { TRANSLATE("Bomber", "RU"), 127, 1, 8, 2, 12, 1, 12, 2, 0, 1, 39 }, /* reciv-ru */
    { TRANSLATE("Trireme", "RU"), 8, 2, 3, 0, 1, 0, 4, 0, 2, 5, 7 }, /* reciv-ru */
    { TRANSLATE("Sail", "RU"), 14, 2, 3, 0, 1, 1, 4, 0, 3, 5, 8 }, /* reciv-ru */
    { TRANSLATE("Frigate", "RU"), 35, 2, 3, 0, 2, 2, 4, 0, 4, 5, 14 }, /* reciv-ru */
    { TRANSLATE("Ironclad", "RU"), 37, 2, 4, 0, 4, 4, 6, 0, 0, 3, 23 }, /* reciv-ru */
    { TRANSLATE("Cruiser", "RU"), 127, 2, 6, 0, 6, 6, 8, 3, 0, 3, 37 }, /* reciv-ru */
    { TRANSLATE("Battleship", "RU"), 127, 2, 4, 0, 18, 12, 16, 3, 0, 3, 52 }, /* reciv-ru */
    { TRANSLATE("Submarine", "RU"), 127, 2, 3, 0, 8, 2, 5, 3, 0, 3, 41 }, /* reciv-ru */
    { TRANSLATE("Carrier", "RU"), 127, 2, 5, 0, 1, 12, 16, 3, 0, 3, 39 }, /* reciv-ru */
    { TRANSLATE("Transport", "RU"), 127, 2, 4, 0, 0, 3, 5, 0, 8, 5, 35 }, /* reciv-ru */
    { TRANSLATE("Nuclear", "RU"), 127, 1, 16, 1, 99, 0, 16, 0, 0, 1, 46 }, /* reciv-ru */
    { TRANSLATE("Diplomat", "RU"), 127, 0, 2, 0, 0, 0, 3, 0, 0, 6, 22 }, /* reciv-ru */
    { TRANSLATE("Caravan", "RU"), 127, 0, 1, 0, 0, 1, 5, 0, 0, 6, 24 }, /* reciv-ru */
};

struct civ_type civt[16] = {
    { "", "", "", 0, 0, 0, 36, 36 },
    { TRANSLATE("Caesar", "RU"), TRANSLATE("", "RU"), TRANSLATE("Roman", "RU"), 0, 1, 1, 24, 10 }, /* reciv-ru */
    { TRANSLATE("Hammurabi", "RU"), TRANSLATE("", "RU"), TRANSLATE("Babylonian", "RU"), -1, -1, 1, 28, 14 }, /* reciv-ru */
    { TRANSLATE("Frederick", "RU"), TRANSLATE("Germans", "RU"), TRANSLATE("German", "RU"), 1, -1, 1, 32, 18 }, /* reciv-ru */
    { TRANSLATE("Ramesses", "RU"), TRANSLATE("", "RU"), TRANSLATE("Egyptian", "RU"), 0, 0, 1, 21, 7 }, /* reciv-ru */
    { TRANSLATE("Abe Lincoln", "RU"), TRANSLATE("", "RU"), TRANSLATE("American", "RU"), -1, 0, 1, 19, 5 }, /* reciv-ru */
    { TRANSLATE("Alexander", "RU"), TRANSLATE("", "RU"), TRANSLATE("Greek", "RU"), 0, 1, -1, 26, 12 }, /* reciv-ru */
    { TRANSLATE("M.Gandhi", "RU"), TRANSLATE("", "RU"), TRANSLATE("Indian", "RU"), -1, -1, 0, 31, 17 }, /* reciv-ru */
    { "", "", "", 0, 0, 0, 36, 36 },
    { TRANSLATE("Stalin", "RU"), TRANSLATE("", "RU"), TRANSLATE("Russian", "RU"), 1, 0, -1, 25, 11 }, /* reciv-ru */
    { TRANSLATE("Shaka", "RU"), TRANSLATE("", "RU"), TRANSLATE("Zulu", "RU"), 1, 0, 0, 22, 8 }, /* reciv-ru */
    { TRANSLATE("Napoleon", "RU"), TRANSLATE("French", "RU"), TRANSLATE("French", "RU"), 1, 1, 1, 23, 9 }, /* reciv-ru */
    { TRANSLATE("Montezuma", "RU"), TRANSLATE("", "RU"), TRANSLATE("Aztec", "RU"), 0, -1, 1, 20, 6 }, /* reciv-ru */
    { TRANSLATE("Mao Tse Tung", "RU"), TRANSLATE("Chinese", "RU"), TRANSLATE("Chinese", "RU"), 0, 0, 1, 29, 15 }, /* reciv-ru */
    { TRANSLATE("Elizabeth I", "RU"), TRANSLATE("English", "RU"), TRANSLATE("English", "RU"), 0, 1, 0, 27, 13 }, /* reciv-ru */
    { TRANSLATE("Genghis Khan", "RU"), TRANSLATE("", "RU"), TRANSLATE("Mongol", "RU"), 1, 1, -1, 30, 16 }, /* reciv-ru */
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
    TRANSLATE("Anarchy", "RU"), /* reciv-ru */
    TRANSLATE("Despotism", "RU"), /* reciv-ru */
    TRANSLATE("Monarchy", "RU"), /* reciv-ru */
    TRANSLATE("Communist", "RU"), /* reciv-ru */
    TRANSLATE("Republic", "RU"), /* reciv-ru */
    TRANSLATE("Democratic", "RU") /* reciv-ru */
};

char *trade_goods[8] = {
    TRANSLATE("Silk", "RU"), /* reciv-ru */
    TRANSLATE("Silver", "RU"), /* reciv-ru */
    TRANSLATE("Wine", "RU"), /* reciv-ru */
    TRANSLATE("Copper", "RU"), /* reciv-ru */
    TRANSLATE("Gems", "RU"), /* reciv-ru */
    TRANSLATE("Dye", "RU"), /* reciv-ru */
    TRANSLATE("Salt", "RU"), /* reciv-ru */
    TRANSLATE("Spice", "RU") /* reciv-ru */
};

/* reciv: one block, as the original's literals were: the save file
 * transfers all 8 * 11 bytes through civ_adjectives[0]. */
char civ_adjective_text[8][11] = {
    TRANSLATE("Barbarian ", "RU"), /* reciv-ru */
    TRANSLATE("Roman     ", "RU"), /* reciv-ru */
    TRANSLATE("Zulu      ", "RU"), /* reciv-ru */
    TRANSLATE("Mongol    ", "RU"), /* reciv-ru */
    TRANSLATE("Aztec     ", "RU"), /* reciv-ru */
    TRANSLATE("Persian   ", "RU"), /* reciv-ru */
    TRANSLATE("Muslim    ", "RU"), /* reciv-ru */
    TRANSLATE("Hittite   ", "RU") /* reciv-ru */
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
    TRANSLATE("Barbarians ", "RU"), /* reciv-ru */
    TRANSLATE("Romans     ", "RU"), /* reciv-ru */
    TRANSLATE("Babylonians", "RU"), /* reciv-ru */
    TRANSLATE("Germans    ", "RU"), /* reciv-ru */
    TRANSLATE("Egyptians  ", "RU"), /* reciv-ru */
    TRANSLATE("Americans  ", "RU"), /* reciv-ru */
    TRANSLATE("Greeks     ", "RU"), /* reciv-ru */
    TRANSLATE("Mongols    ", "RU") /* reciv-ru */
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
    TRANSLATE("Attila       ", "RU"), /* reciv-ru */
    TRANSLATE("Caesar       ", "RU"), /* reciv-ru */
    TRANSLATE("Shaka        ", "RU"), /* reciv-ru */
    TRANSLATE("Genghis Khan ", "RU"), /* reciv-ru */
    TRANSLATE("Montezuma    ", "RU"), /* reciv-ru */
    TRANSLATE("Xerxes       ", "RU"), /* reciv-ru */
    TRANSLATE("Saladin      ", "RU"), /* reciv-ru */
    TRANSLATE("Suppiluliumas", "RU") /* reciv-ru */
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
    TRANSLATE("Mr.", "RU"), /* reciv-ru */
    TRANSLATE("Emperor", "RU"), /* reciv-ru */
    TRANSLATE("King ", "RU"), /* reciv-ru */
    TRANSLATE("Comrade", "RU"), /* reciv-ru */
    TRANSLATE("President", "RU"), /* reciv-ru */
    TRANSLATE("President", "RU") /* reciv-ru */
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
        strcpy(buffer, TRANSLATE("Please insert Disk ", "RU")); /* reciv-ru */
        strcat(buffer, &disk_digit);
        strcat(buffer, TRANSLATE(".\n \n    Press Return\n    to continue.\n", "RU")); /* reciv-ru */
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
        strcpy(msgtext, TRANSLATE(" Start a New Game\n Load a Saved Game\n EARTH\n Customize World\n View Hall of Fame\n", "RU")); /* reciv-ru */
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
            strcpy(msgtext, TRANSLATE("LAND MASS:\n Small\n Normal\n Large\n", "RU")); /* reciv-ru */
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
            strcpy(msgtext, TRANSLATE("TEMPERATURE:\n Cool\n Temperate\n Warm\n", "RU")); /* reciv-ru */
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
            strcpy(msgtext, TRANSLATE("CLIMATE:\n Arid\n Normal\n Wet\n", "RU")); /* reciv-ru */
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
                strcpy(msgtext, TRANSLATE("AGE:\n 3 billion years\n 4 billion years\n 5 billion years\n", "RU")); /* reciv-ru */
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
