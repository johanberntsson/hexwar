// The title screen, its menu, and the two screens that set up a new game.
//
// **Every screen here is one menu run by menu_run(), which has its own
// once-a-frame loop, the same shape as game_run()'s**: wait for the beam,
// read one frame's input, act on it. The mouse and joystick have to be read
// every frame here as much as in the game, and input_poll() counts its calls
// as frames.
//
// It all runs on the plain text screen, before view_init() writes the map
// window's character numbers, so nothing here has to keep out of the window.

#include "title.h"
#include "civ.h"
#include "input.h"
#include "screen.h"
#include "text.h"

#define TEXT_COLOUR COLOR_LIGHTGREEN

#define MENU_ROW 11             // the first item; they are two rows apart
#define NOTE_ROW 21

// What menu_run() returns besides an item: Esc or RUN/STOP.
#define MENU_BACK 0xfe

// The menu menu_run() is showing: a list text, an item a line.
static byte menu_list;
static void (*menu_describe)(byte i);   // draws a line or two, or 0
static byte menu_count;
static byte menu_left;                  // the band of columns the arrow
static byte menu_right;                 // counts as being on an item
static byte selected;

static byte length(const char *s)
{
    byte n = 0;
    while (s[n]) ++n;
    return n;
}

static void centre(byte row, const char *s, byte colour)
{
    screen_putsxy((byte)((SCREEN_COLUMNS - length(s)) / 2), row, s, colour);
}

// A whole row of spaces, so a shorter text overwrites a longer one.
static void blank_row(byte row)
{
    byte x;
    for (x = 0; x < SCREEN_COLUMNS; ++x) screen_putcxy(x, row, ' ', TEXT_COLOUR);
}

// --------------------------------------------------------------- the menu

static void draw_item(byte i)
{
    const char *s = text_line(menu_list, i);
    byte len = length(s);
    byte x = (byte)((SCREEN_COLUMNS - len) / 2);
    byte row = MENU_ROW + 2 * i;
    byte on = (i == selected);

    screen_putcxy(x - 2, row, on ? '>' : ' ', COLOR_YELLOW);
    screen_putsxy(x, row, s, on ? COLOR_WHITE : TEXT_COLOUR);
    screen_putcxy(x + len + 1, row, on ? '<' : ' ', COLOR_YELLOW);
}

static void draw_note(void)
{
    blank_row(NOTE_ROW);
    blank_row(NOTE_ROW + 1);
    if (menu_describe) menu_describe(selected);
}

static void select_item(byte i)
{
    byte was = selected;
    if (i == was) return;
    selected = i;
    draw_item(was);
    draw_item(i);
    draw_note();
}

// Which item the tip of the arrow is on, or TITLE_NONE. A character is four
// sprite units across and eight down; see input.h.
static byte item_under_pointer(void)
{
    byte col = (byte)(input_x >> 2);
    byte row = (byte)(input_y >> 3);
    byte i;

    if (col < menu_left || col >= menu_right) return TITLE_NONE;
    for (i = 0; i < menu_count; ++i)
        if (row == MENU_ROW + 2 * i) return i;
    return TITLE_NONE;
}

// Show `count` items from MENU_ROW down, `first` selected, and return the
// one the player picks, or MENU_BACK. The arrow selects by hovering and a
// click picks; the cursor keys and Return or space do the same, and each
// item can also be picked by its letter in `keys`. The items are the lines
// of the list text `list` (res/text/list_*.txt). `describe`, if not 0,
// writes something about the selected item at NOTE_ROW and the row below.
static byte menu_run(byte list, void (*describe)(byte i),
                     byte count, const char *keys, byte first)
{
    byte key, i, widest = 0;

    menu_list = list;
    menu_describe = describe;
    menu_count = count;
    selected = first;
    for (i = 0; i < count; ++i) {
        byte len = length(text_line(list, i));
        if (len > widest) widest = len;
        blank_row(MENU_ROW + 2 * i);
        draw_item(i);
    }
    // The longest item and its markers, with a column to spare either side.
    menu_left = (byte)((SCREEN_COLUMNS - widest) / 2 - 3);
    menu_right = (byte)(menu_left + widest + 6);
    if (describe) draw_note();

    for (;;) {
        // Once a frame; the line itself does not matter here, as long as it
        // is below 256 and above the 56 that $D012 repeats.
        screen_wait_raster(SCREEN_ROW_LINE(SCREEN_ROWS - 1));
        key = input_poll();
        if (input_moved()) {
            i = item_under_pointer();
            if (i != TITLE_NONE) select_item(i);
        }
        if (!key) continue;
        if (key >= 'A' && key <= 'Z') key += 'a' - 'A';

        switch (key) {
        case KEY_UP:
            if (selected > 0) select_item(selected - 1);
            continue;
        case KEY_DOWN:
            if (selected < count - 1) select_item(selected + 1);
            continue;
        case KEY_RETURN:
        case KEY_SPACE:
            return selected;
        case KEY_CLICK:
            i = item_under_pointer();
            if (i == TITLE_NONE) continue;
            select_item(i);
            return i;
        case KEY_ESC:
        case KEY_RUNSTOP:
            return MENU_BACK;
        }
        for (i = 0; i < count; ++i) {
            if (keys[i] == key) {
                select_item(i);
                return i;
            }
        }
    }
}

// ----------------------------------------------------------- the screens

// What is behind the menus is res/text/title.txt, people.txt and
// level.txt, which keep clear of MENU_ROW and NOTE_ROW.
static void draw_screen(byte id)
{
    screen_clear();
    text_show(id);
}

// "Led by Thutmose III, King of Egypt", put together here so that civ.c
// keeps the name and the title apart for the letters.
static void describe_civ(byte i)
{
    static char line[SCREEN_COLUMNS + 1];
    const char *parts[4];
    byte n = 0, p;
    const char *s;

    parts[0] = "Led by ";
    parts[1] = civ_leader(i);   // two text_line()s at once, and no more
    parts[2] = ", ";
    parts[3] = civ_title(i);
    for (p = 0; p < 4; ++p)
        for (s = parts[p]; *s && n < SCREEN_COLUMNS; ++s) line[n++] = *s;
    line[n] = 0;

    centre(NOTE_ROW, civ_note(i), COLOR_LIGHTGREY);
    centre(NOTE_ROW + 1, line, COLOR_WHITE);
}

static void describe_level(byte i)
{
    centre(NOTE_ROW, level_note(i), COLOR_LIGHTGREY);
}

// The people, then the difficulty. 1 once both are chosen, 0 if the player
// backs out to the title. Esc on the second goes back to the first.
static byte new_game(void)
{
    static byte civ, level;     // kept, so going back remembers them
    byte pick;

    for (;;) {
        draw_screen(TEXT_CHOOSE_PEOPLE);
        pick = menu_run(TEXT_LIST_PEOPLES, describe_civ, CIVS, "mhec", civ);
        if (pick == MENU_BACK) return 0;
        civ = pick;

        draw_screen(TEXT_CHOOSE_LEVEL);
        pick = menu_run(TEXT_LIST_LEVELS, describe_level, LEVELS, "12345", level);
        if (pick != MENU_BACK) {
            level = pick;
            civ_choose(civ);
            game_level = level;
            return 1;
        }
    }
}

void title_show(void)
{
    draw_screen(TEXT_TITLE);
    centre(MENU_ROW, "Loading...", TEXT_COLOUR);
}

byte title_run(byte load_failed)
{
    byte pick = TITLE_START;

    if (load_failed) {
        centre(NOTE_ROW, "No saved game could be loaded", COLOR_YELLOW);
        pick = TITLE_LOAD;
    }

    for (;;) {
        pick = menu_run(TEXT_LIST_TITLE_MENU, 0, TITLE_ITEMS, "slq", pick);
        if (pick == MENU_BACK || pick == TITLE_QUIT) return TITLE_QUIT;
        if (pick == TITLE_LOAD) return TITLE_LOAD;
        if (new_game()) return TITLE_START;
        draw_screen(TEXT_TITLE);
    }
}
