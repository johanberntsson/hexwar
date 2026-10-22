// The turn loop and the player's side of it.
//
// **Everything runs off one loop that goes round once a frame**, in
// game_run(): wait for the beam to leave the map window, redraw the view if
// anything changed, then read one frame's input and act on it. There is no
// nested "wait for a key" anywhere, because the mouse and the joystick have
// to be read every frame whatever the game is waiting for. The state that
// says what it is waiting for is just unit_active: a unit wanting orders, or
// UNIT_NONE and the end-of-turn prompt in the panel.

#include "game.h"
#include "gameint.h"
#include "growth.h"
#include "help.h"
#include "improve.h"
#include "ai.h"
#include "boat.h"
#include "emporium.h"
#include "city.h"
#include "cityscreen.h"
#include "topcities.h"
#include "save.h"
#include "score.h"
#include "text.h"
#include "civ.h"
#include "diplomacy.h"
#include "combat.h"
#include "sevenships.h"
#include "input.h"
#include "map.h"
#include "menu.h"
#include "minimap.h"
#include "popup.h"
#include "screen.h"
#include "sheet.h"
#include "unit.h"
#include "view.h"

#define PANEL_WIDTH VIEW_LEFT

#define TEXT_COLOUR COLOR_LIGHTGREEN

// Where the view sits on the map, in screen characters; see view_redraw().
word off_x;
word off_y;

byte dirty;              // the view wants redrawing
byte contact;            // see whether a people has come in sight
byte greet;              // peoples just met, greeted after a redraw
byte hover_dirty;        // the readout of what is under the arrow
byte hover_unit;         // ... or on hover_ax/ay; see hover.c
byte hover_ax, hover_ay;
byte game_over;
byte quit;
byte retired;            // quit by Game/Retire, for the title screen
byte loaded;             // from a saved game: no opening popup
byte game_end_wait;      // see game.h
static byte end_due;     // the last unit has had its orders: end the turn
word game_turn;
word game_year;

// **Tile select**, Civilization's keyboard stand-in for the mouse: Tab puts
// a ring on a hexagon, the cursor keys (or the unit's moving keys) move it,
// Return does what a click on it would, and Tab or Esc puts it away. While
// it is up the ring blinks rather than the active unit.
byte selecting;
byte sel_ax, sel_ay;
static byte sel_hidden;         // the ring's half of the blink

// The active unit blinks: BLINK_FRAMES drawn, BLINK_FRAMES not, half a
// second each at 50 Hz. Each change is a whole redraw, which is what a held
// cursor key already does every frame, rather than a redraw of one hexagon:
// a unit's tile overlaps five neighbours, so putting the terrain back under
// it would mean redrawing parts of all of them in painter's order.
#define BLINK_FRAMES 25
static byte blink;

// ------------------------------------------------------------------- text

// A line of text built up a piece at a time, so that no printf is linked.
char line[41];
byte line_len;

void line_clear(void)
{
    line_len = 0;
    line[0] = 0;
}

void line_add(const char *s)
{
    while (*s && line_len < sizeof line - 1) line[line_len++] = *s++;
    line[line_len] = 0;
}

// "Hittite Warrior".
void line_add_unit(byte owner, byte kind)
{
    line_add(player_adjective(owner));
    line_add(" ");
    line_add(unit_kind_name[kind]);
}

void line_add_num(word v)
{
    char digits[6];
    byte i = sizeof digits - 1;
    digits[i] = 0;
    do { digits[--i] = (char)('0' + v % 10); v /= 10; } while (v);
    line_add(digits + i);
}

// Text in the panel down the left, padded with spaces to the panel's width
// so that it overwrites whatever was there.
static void panel_text(byte row, const char *s, byte colour)
{
    byte x = 0;
    while (*s && x < PANEL_WIDTH) screen_putcxy(x++, row, *s++, colour);
    while (x < PANEL_WIDTH) screen_putcxy(x++, row, ' ', colour);
}

// Below the minimap and its frame, which take rows 1-7.
#define ROW_CIV 9
#define ROW_YEAR 10
#define ROW_TURN 11
#define ROW_UNIT 13
#define ROW_MOVES 14
#define ROW_PROMPT 16           // two rows
#define ROW_MESSAGE 19          // two rows

// Two lines of message in the panel. `second` may be `line`.
void message(const char *first, const char *second)
{
    panel_text(ROW_MESSAGE, first, COLOR_YELLOW);
    panel_text(ROW_MESSAGE + 1, second, COLOR_YELLOW);
}

static void draw_frame(void)
{
    menu_draw();
    panel_text(ROW_CIV, civ_adjective(player_civ[PLAYER_HUMAN]), TEXT_COLOUR);
    // Wider than the window, so it ends where the window ends rather than
    // starting where it starts.
    if (input_real_hw)
        screen_putsxy(VIEW_LEFT + VIEW_COLUMNS - 17, SCREEN_ROWS - 1,
                      "HELP for the keys", TEXT_COLOUR);
    else
        screen_putsxy(VIEW_LEFT + VIEW_COLUMNS - 15, SCREEN_ROWS - 1,
                      "F1 for the keys", TEXT_COLOUR);
}

void draw_status(void)
{
    line_clear();
    line_add_num(game_year);
    line_add(" BC");
    panel_text(ROW_YEAR, line, TEXT_COLOUR);
    line_clear();
    line_add("Turn ");
    line_add_num(game_turn);
    panel_text(ROW_TURN, line, TEXT_COLOUR);
}

// The unit waiting for orders, or the end-of-turn prompt if there is none.
static void draw_orders(void)
{
    byte u = unit_active;
    byte r;

    if (u != UNIT_NONE) {
        panel_text(ROW_UNIT, unit_kind_name[unit_kind[u]], COLOR_WHITE);
        line_clear();
        line_add("Moves: ");
        line_add_num(unit_moves[u]);
        panel_text(ROW_MOVES, line, TEXT_COLOUR);
        for (r = 0; r < 2; ++r) panel_text(ROW_PROMPT + r, "", TEXT_COLOUR);
        return;
    }
    panel_text(ROW_UNIT, "", TEXT_COLOUR);
    panel_text(ROW_MOVES, "", TEXT_COLOUR);
    if (game_over) {
        panel_text(ROW_PROMPT, "Game over", COLOR_WHITE);
        panel_text(ROW_PROMPT + 1, "Press a key", TEXT_COLOUR);
    } else {
        panel_text(ROW_PROMPT, "End of turn", COLOR_WHITE);
        panel_text(ROW_PROMPT + 1, "Press Return", ATTR_BLINK | TEXT_COLOUR);
    }
}

// ---------------------------------------------------------------- the view

// Which hexagon is under the tip of the arrow. 0 if the arrow is not over
// the map window. A sprite unit is two H640 pixels, so a character is four
// units across and eight down.
byte pointer_hex(byte *ax, byte *ay)
{
    byte col = (byte)(input_x >> 2);
    byte row = (byte)(input_y >> 3);
    word cx;

    if (col < VIEW_LEFT || col >= VIEW_LEFT + VIEW_COLUMNS) return 0;
    if (row < VIEW_TOP || row >= VIEW_TOP + VIEW_ROWS) return 0;
    cx = off_x + (col - VIEW_LEFT);
    if (cx >= MAP_WIDTH_CHARS) cx -= MAP_WIDTH_CHARS;
    map_cell_at(cx, off_y + (row - VIEW_TOP), ax, ay);
    return 1;
}

// Whether all of (ax, ay)'s tile is inside the window.
static byte in_view(byte ax, byte ay)
{
    word x = MAP_CELL_X(ax) + MAP_WIDTH_CHARS - off_x;
    int y = (int)MAP_CELL_Y(ax, ay) - (int)off_y;
    if (x >= MAP_WIDTH_CHARS) x -= MAP_WIDTH_CHARS;
    return x <= VIEW_COLUMNS - TILE_EXPANDED_COLUMNS
           && y >= 0 && y <= VIEW_ROWS - TILE_ROWS;
}

static void centre_on(byte ax, byte ay)
{
    int x = (int)MAP_CELL_X(ax) + TILE_EXPANDED_COLUMNS / 2 - VIEW_COLUMNS / 2;
    int y = (int)MAP_CELL_Y(ax, ay) + TILE_ROWS / 2 - VIEW_ROWS / 2;
    if (x < 0) x += MAP_WIDTH_CHARS;
    if (y < VIEW_MIN_OFF_Y) y = VIEW_MIN_OFF_Y;
    if (y > VIEW_MAX_OFF_Y) y = VIEW_MAX_OFF_Y;
    off_x = (word)x;
    off_y = (word)y;
    dirty = 1;
}

// Where a people just met was seen, brought into view above the middle of
// the window, which the greeting's popup is about to cover -- or below it,
// near the bottom of the map, where the view cannot go far enough down.
// Row 1, so that whichever of the player's is next to it is in view too
// if it is below, as it is half the time.
#define MET_ROW 1               // the window row of the tile's top
#define MET_ROW_BELOW 17
static void show_met(void)
{
    byte ax = diplomacy_ax, ay = diplomacy_ay;
    int y = (int)MAP_CELL_Y(ax, ay) - MET_ROW;

    centre_on(ax, ay);
    if (y > VIEW_MAX_OFF_Y) y -= MET_ROW_BELOW - MET_ROW;
    if (y < VIEW_MIN_OFF_Y) y = VIEW_MIN_OFF_Y;
    if (y > VIEW_MAX_OFF_Y) y = VIEW_MAX_OFF_Y;
    off_y = (word)y;
}

// ------------------------------------------------------------------- turns

// The readout on (ax, ay) until the arrow moves.
static void hover_on(byte ax, byte ay)
{
    hover_ax = ax;
    hover_ay = ay;
    hover_unit = 1;
    hover_dirty = 1;
}

// Hand the orders to the next unit after `after` that wants some, bringing
// it into view. With none left, the panel asks for the end of the turn.
void select_unit(byte after)
{
    byte was = unit_active;
    byte u = unit_next(PLAYER_HUMAN, after);

    if (game_over || !player_alive(PLAYER_HUMAN)) {
        game_over = 1;
        u = UNIT_NONE;
    }
    unit_active = u;
    // Orders to the last unit end the turn, unless End of Turn is on; a
    // turn with nobody to give orders to still waits for Return.
    end_due = was != UNIT_NONE && u == UNIT_NONE;
    if (was != u) dirty = 1;   // another unit blinks
    if (u != UNIT_NONE) hover_on(unit_ax[u], unit_ay[u]);
    if (u != UNIT_NONE && !in_view(unit_ax[u], unit_ay[u]))
        centre_on(unit_ax[u], unit_ay[u]);
    draw_orders();
}



// --------------------------------------------------------------- orders

// The active unit u attacks (nx, ny) -- after a declaration of war, if it
// takes one -- and the panel says how it went.
static void attack(byte u, byte nx, byte ny)
{
    byte kind = unit_kind[u];
    byte o = combat_holder(nx, ny);

    if (!unit_kind_attack[kind]) {
        message(unit_kind_name[kind], "cannot attack");
        return;
    }
    if (!at_war(PLAYER_HUMAN, o)) {
        popup_begin();
        popup_add("Declare war on the ");
        popup_add(player_people(o));
        popup_add("?");
        if (!popup_ask_shown()) return;
        declare_war(PLAYER_HUMAN, o);
    }
    line_clear();
    switch (combat_attack(u, nx, ny)) {
    case COMBAT_WON:
        line_add_unit(combat_owner, combat_kind);
        message(line, "destroyed");
        break;
    case COMBAT_LOST:
        line_add("Your ");
        line_add(unit_kind_name[kind]);
        message(line, "was destroyed");
        break;
    default:
        message("Captured", city_name_of(combat_city));
        break;
    }
    dirty = 1;
    hover_dirty = 1;
    select_unit(u);
}

static void move_active(byte dir)
{
    byte u = unit_active;
    byte nx, ny, t;

    if (!map_neighbour(unit_ax[u], unit_ay[u], dir, &nx, &ny)) return;
    t = unit_can_enter(PLAYER_HUMAN, unit_kind[u], nx, ny);
    if (t == ENTER_WATER) {
        message("Cannot swim", "");
        return;
    }
    if (t == ENTER_LAND) {
        message("Ships keep", "to the shallows");
        return;
    }
    if (t == ENTER_FOREIGN) {
        attack(u, nx, ny);
        return;
    }
    message("", "");
    unit_move_to(u, nx, ny);
    --unit_moves[u];
    if (!unit_moves[u]) unit_wake_cargo(u);
    dirty = 1;
    contact = 1;
    hover_on(nx, ny);
    if (unit_moves[u]) {
        if (!in_view(nx, ny)) centre_on(nx, ny);
        draw_orders();
    } else {
        select_unit(u);
    }
}

static void found_city(void)
{
    byte u = unit_active;
    byte c;

    if (unit_kind[u] == KIND_MERCHANT) {
        emporium_order(u);
        return;
    }
    if (unit_kind[u] != KIND_SETTLER) {
        message("Only settlers", "found cities");
        return;
    }
    c = city_found(PLAYER_HUMAN, unit_ax[u], unit_ay[u]);
    if (c == CITY_NONE) {
        message("Cannot found", "a city here");
        return;
    }
    unit_remove(u);
    message("Founded", city_name_of(c));
    dirty = 1;
    hover_dirty = 1;
    select_unit(u);
}

// The keypad in xemu, as res/xemu-keymap.cfg delivers it: 7 9 4 6 1 3.
static const byte pad_dir[] = { DIR_NW, DIR_NE, DIR_W, DIR_E, DIR_SW, DIR_SE };

// The direction a moving key stands for, or DIR_NONE.
byte key_dir(byte key)
{
    if (!input_real_hw && key >= KEY_PAD_FIRST && key <= KEY_PAD_LAST)
        return pad_dir[(byte)(key - KEY_PAD_FIRST)];

    switch (key) {
    // The same pattern as q e / a d / z c, on the right of the keyboard
    // beside the cursor keys. The key left of = is : unshifted and [
    // shifted, so both are taken.
    case '0': return DIR_NW;
    case '-': return DIR_NE;
    case 'p': return DIR_W;
    case '*': return DIR_E;
    case ':':
    case '[': return DIR_SW;
    case '=': return DIR_SE;
    }
    return DIR_NONE;
}

// The active unit, a builder, starts irrigating or mining where it stands.
static void improve(byte orders)
{
    byte u = unit_active;
    const char *what = orders == ORDERS_IRRIGATE ? "irrigate" : "mine";

    if (unit_kind[u] != KIND_BUILDER) {
        message("Only builders", what);
        return;
    }
    if (!improve_can(orders, unit_ax[u], unit_ay[u])) {
        line_clear();
        line_add("Cannot ");
        line_add(what);
        message(line, "here");
        return;
    }
    improve_start(u, orders);
    select_unit(u);
}

static void unit_key(byte key)
{
    byte u = unit_active;
    byte dir = key_dir(key);

    if (dir != DIR_NONE) {
        move_active(dir);
        return;
    }

    switch (key) {
    case 'b': found_city(); break;
    case KEY_SPACE:                         // no orders this turn
        unit_moves[u] = 0;
        unit_wake_cargo(u);
        select_unit(u);
        break;
    case 'w':                               // ask again after the others
        select_unit(u);
        break;
    case 'f':
        unit_orders[u] = ORDERS_FORTIFY;
        select_unit(u);
        break;
    case 'i':
        // A fishing boat works the fish it is on.
        if (unit_kind[u] == KIND_BOAT) boat_order(u);
        else improve(ORDERS_IRRIGATE);
        break;
    case 'm': improve(ORDERS_MINE); break;
    }
}

// The map screen, all of it, as game_run() first puts it up -- after the
// city screen, which wrote over every cell of it. The pixels of the minimap
// are still in place, so only its cells come back; the view is redrawn on
// the next frame, as the patch the city screen drew has replaced part of it.
static void draw_screen(void)
{
    screen_clear();
    view_init();
    minimap_show();
    draw_frame();
    draw_status();
    draw_orders();
    dirty = 1;
    hover_dirty = 1;
}

static void open_city(byte c)
{
    city_screen(c);
    // Its unit list may have woken a unit, with the turn otherwise over,
    // or fortified the one that had the orders.
    if (unit_active == UNIT_NONE || unit_orders[unit_active] != ORDERS_NONE)
        select_unit(unit_active);
    draw_screen();
}

static void open_help(byte id)
{
    help_screen(id);
    draw_screen();
}

static void open_top_cities(void)
{
    top_cities_screen();
    draw_screen();
}

// Fill in the Orders menu for the unit that has the orders.
static void set_orders_menu(void)
{
    byte command = MENU_NONE, second = MENU_NONE;
    if (unit_active != UNIT_NONE) {
        byte kind = unit_kind[unit_active];
        if (kind == KIND_SETTLER) command = MENU_FOUND_CITY;
        else if (kind == KIND_WARRIOR || kind == KIND_SPEARMAN)
            command = MENU_FORTIFY;
        else if (kind == KIND_BUILDER) {
            command = MENU_IRRIGATE;
            second = MENU_MINE;
        } else if (kind == KIND_BOAT) {
            command = MENU_FISH;
        } else if (kind == KIND_MERCHANT) {
            command = MENU_EMPORIUM;
        }
    }
    menu_orders(command, second);
}

// Do what a menu item asks for, picked by click or by key. Whatever it
// does, the view is redrawn afterwards, which puts back the minimap's
// outline the dropdown hid.
static void menu_command(byte command)
{
    switch (command) {
    case MENU_RETIRE:
        if (popup_ask("Do you want to quit this game?")) {
            score_screen(SCORE_RETIRED);
            retired = 1;
            quit = 1;
        }
        break;
    case MENU_SCORE:
        score_screen(SCORE_NOW);
        draw_screen();
        break;
#ifdef DEBUG_MENU
    case MENU_SHOW_MAP:
        // The whole minimap changes, so it is drawn again; the view is
        // redrawn below anyway.
        map_show ^= MAP_EXPLORED;
        minimap_init();
        message(map_show ? "Map shown" : "Map hidden", "");
        hover_dirty = 1;
        break;
    case MENU_PLAY_50:
        game_autoplay(AUTOPLAY_TURNS);
        break;
#endif
    case MENU_END_OF_TURN:
        game_end_wait ^= 1;
        message("End of Turn", game_end_wait ? "waits" : "automatic");
        break;
    case MENU_SAVE:
        message("Saving", "");
        message(save_write() ? "Game saved" : "Could not save", "");
        break;
    case MENU_FOUND_CITY:
    case MENU_EMPORIUM:
        unit_key('b');
        break;
    case MENU_FORTIFY:
        unit_key('f');
        break;
    case MENU_IRRIGATE:
        unit_key('i');
        break;
    case MENU_MINE:
        unit_key('m');
        break;
    case MENU_FISH:
        unit_key('i');
        break;
    case MENU_TOP_CITIES:
        open_top_cities();
        break;
    case MENU_DIPLOMACY:
        // Anybody in sight is met before the list, and greeted after it.
        greet |= diplomacy_contact();
        if (greet) show_met();
        diplomacy_screen();
        draw_screen();
        break;
    case MENU_KEYS:
        open_help(TEXT_HELP);
        break;
    default:
        if (command >= MENU_UNIT)
            open_help(TEXT_PEDIA_SETTLER + (command - MENU_UNIT - KIND_SETTLER));
        else if (command >= MENU_TERRAIN)
            open_help(TEXT_PEDIA_HILLS + (command - MENU_TERRAIN));
        break;
    }
    dirty = 1;
}

// A click on hexagon (ax, ay) of the map, by the arrow or by Return in tile
// select. Next to the unit with orders it moves the unit there; on one of
// the player's cities it opens the city screen; on one of the player's own
// units that still has moves it gives that unit the orders, waking it if it
// was fortified. Anywhere else it centres the view there.
static void click_hex(byte ax, byte ay)
{
    byte u, dir, c;

    if (unit_active != UNIT_NONE) {
        dir = map_direction(unit_ax[unit_active], unit_ay[unit_active],
                            ax, ay);
        if (dir != DIR_NONE) {
            move_active(dir);
            return;
        }
    }
    c = city_at(ax, ay);
    if (c != CITY_NONE && city_owner[c] == PLAYER_HUMAN) {
        open_city(c);
        return;
    }
    if (unit_active != UNIT_NONE
        && unit_ax[unit_active] == ax && unit_ay[unit_active] == ay) return;
    for (u = 0; u < MAX_UNITS; ++u) {
        if (unit_kind[u] != KIND_NONE && unit_owner[u] == PLAYER_HUMAN
            && unit_ax[u] == ax && unit_ay[u] == ay && unit_moves[u]) {
            unit_orders[u] = ORDERS_NONE;
            unit_active = u;
            dirty = 1;
            hover_on(ax, ay);
            draw_orders();
            return;
        }
    }
    centre_on(ax, ay);
}

// A click of the mouse or fire: on the menu bar, the minimap or the map, or
// elsewhere, with the turn over, it ends the turn.
static void click(void)
{
    byte ax, ay;

    if ((byte)(input_y >> 3) == 0) {
        set_orders_menu();
        menu_command(menu_click());
        return;
    }
    if (minimap_hex(input_x, input_y, &ax, &ay)) {
        centre_on(ax, ay);
        return;
    }
    if (!pointer_hex(&ax, &ay)) {
        if (unit_active == UNIT_NONE && !game_over) game_end_turn();
        return;
    }
    click_hex(ax, ay);
}

// ------------------------------------------------------------- tile select

// Tab: the ring goes on the unit with orders, or the middle of the view.
static void select_begin(void)
{
    word cx = off_x + VIEW_COLUMNS / 2;

    if (unit_active != UNIT_NONE) {
        sel_ax = unit_ax[unit_active];
        sel_ay = unit_ay[unit_active];
    } else {
        if (cx >= MAP_WIDTH_CHARS) cx -= MAP_WIDTH_CHARS;
        map_cell_at(cx, off_y + VIEW_ROWS / 2, &sel_ax, &sel_ay);
    }
    selecting = 1;
    message("Tile select:", "Tab to leave");
    dirty = 1;
    hover_dirty = 1;
}

static void select_end(void)
{
    selecting = 0;
    message("", "");
    dirty = 1;
    hover_dirty = 1;
}

// One hexagon in direction dir, bringing it into view. Off the top or
// bottom of the map it stays where it is.
static void select_move(byte dir)
{
    byte nx, ny;

    if (!map_neighbour(sel_ax, sel_ay, dir, &nx, &ny)) return;
    sel_ax = nx;
    sel_ay = ny;
    if (!in_view(nx, ny)) centre_on(nx, ny);
    dirty = 1;
    hover_dirty = 1;
}

// A key while tile select is up. Returns 0 for one it leaves to the usual
// handling -- the unit's orders, the menus, HELP.
static byte select_key(byte key)
{
    byte dir = key_dir(key);
    // Odd columns sit half a hexagon lower, so going straight up or down
    // zigzags between a column and the one east of it, and back the same
    // way: every hexagon can be reached with the four cursor keys.
    byte odd = sel_ax & 1;

    switch (key) {
    case KEY_LEFT: dir = DIR_W; break;
    case KEY_RIGHT: dir = DIR_E; break;
    case KEY_UP: dir = odd ? DIR_NW : DIR_NE; break;
    case KEY_DOWN: dir = odd ? DIR_SW : DIR_SE; break;
    case KEY_RETURN: click_hex(sel_ax, sel_ay); return 1;
    case KEY_TAB:
    case KEY_ESC: select_end(); return 1;
    }
    if (dir == DIR_NONE) return 0;
    select_move(dir);
    return 1;
}

// Where view_redraw() is to draw the ring, if it is to draw one: one
// hexagon row and a column of the window that may be off either side of
// it, which view_draw_tile() clips.
static void place_mark(void)
{
    int x;

    view_mark_hy = 0xff;
    if (!selecting || sel_hidden) return;
    x = (int)(MAP_CELL_X(sel_ax) + MAP_WIDTH_CHARS - off_x);
    if (x >= MAP_WIDTH_CHARS) x -= MAP_WIDTH_CHARS;
    if (x > MAP_WIDTH_CHARS - TILE_EXPANDED_COLUMNS) x -= MAP_WIDTH_CHARS;
    view_mark_x = x;
    view_mark_hy = 2 * sel_ay + (sel_ax & 1);
}

// --------------------------------------------------------------- the game


// The popup a new game opens with.
byte game_load(void)
{
    return loaded = save_read();
}

// res/text/intro.txt, with the people and their leader filled in.
static void intro(void)
{
    byte civ = player_civ[PLAYER_HUMAN];
    const char *args[2];

    args[0] = civ_people(civ);
    args[1] = civ_leader(civ);
    popup_begin();
    popup_add_text(TEXT_INTRO, args);
    popup_show();
}

byte game_run(void)
{
    byte key;

    draw_frame();
    minimap_init();
    draw_status();
    select_unit(UNIT_NONE);

    // Over the map rather than over a blank window, so the map is drawn
    // first. A build without the title is for screenshots of the view, and
    // has no popup to get in their way either.
#ifndef NO_TITLE
    view_wait_for_redraw();
    place_mark();
    view_redraw(off_x, off_y);
    minimap_frame(off_x, off_y);
    dirty = 0;
    if (!loaded) intro();
#endif

    while (!quit) {
        // Start the redraw the moment the beam leaves the map window, so
        // that it is rewriting rows the beam has already passed. Without
        // this the redraw lands in the middle of a displayed frame and the
        // map tears while a cursor key is held. Waiting here every frame is
        // also what makes one trip round this loop one frame, which
        // input_poll() counts on.
        view_wait_for_redraw();
        // Anything that changed the view shows the active unit at once and
        // starts its blink again, so that it is there to see as soon as it
        // has moved or been selected, and while the view is scrolled. In
        // tile select the ring blinks instead, and the unit stays.
        if (dirty) {
            unit_hidden = 0;
            sel_hidden = 0;
            blink = 0;
        } else if ((selecting || unit_active != UNIT_NONE)
                   && ++blink == BLINK_FRAMES) {
            if (selecting) sel_hidden ^= 1;
            else unit_hidden ^= 1;
            blink = 0;
            dirty = 1;
        }
        if (dirty) {
            place_mark();
            view_redraw(off_x, off_y);
            minimap_frame(off_x, off_y);
            dirty = 0;
            hover_dirty = 1;
        }
        // Whoever has come in sight is brought into view first, and
        // greeted after the redraw, so that they are on the map behind the
        // greeting -- the popup that said so used to leave the player to
        // find them.
        // The letters wait for the greetings, which come first.
        if (contact) {
            contact = 0;
            greet |= diplomacy_contact();
            if (greet) show_met();
        } else if (greet || diplomacy_waiting) {
            diplomacy_greet(greet);
            greet = 0;
        } else if (end_due) {
            // After the greetings and the letters the last move brought.
            end_due = 0;
            if (!game_end_wait && unit_active == UNIT_NONE && !game_over)
                game_end_turn();
        }

        key = input_poll();
        if (input_moved()) {
            hover_dirty = 1;
            hover_unit = 0;
        }
        if (hover_dirty) {
            game_draw_hover();
            hover_dirty = 0;
        }
        if (!key) continue;
        if (key >= 'A' && key <= 'Z') key += 'a' - 'A';

        // The game is over: the final score, then the title screen.
        if (game_over) {
            score_screen(SCORE_END);
            retired = 1;
            quit = 1;
            continue;
        }
        if (selecting && select_key(key)) continue;

        // Two characters a step across and one row down, because that is
        // the same distance on the glass: at H640/V200 a character is twice
        // as tall as it is wide. A hexagon is 8 characters wide and 3 rows
        // apart, so that is four sub-steps per column and three per row.
        //
        // Across there is no limit: the map wraps east-west, so off_x runs
        // round 0..MAP_WIDTH_CHARS-1 and view_redraw() draws across the seam.
        switch (key) {
        case KEY_LEFT:
            if (off_x < 2) off_x += MAP_WIDTH_CHARS;
            off_x -= 2;
            dirty = 1;
            break;
        case KEY_RIGHT:
            off_x += 2;
            if (off_x >= MAP_WIDTH_CHARS) off_x -= MAP_WIDTH_CHARS;
            dirty = 1;
            break;
        case KEY_UP:
            if (off_y > VIEW_MIN_OFF_Y) { --off_y; dirty = 1; }
            break;
        case KEY_DOWN:
            if (off_y < VIEW_MAX_OFF_Y) { ++off_y; dirty = 1; }
            break;
        case KEY_ESC:
        case KEY_RUNSTOP:
            // Asked, so that a stray Esc does not end the game. Esc again
            // is No.
            if (popup_ask("Do you want to quit to BASIC?")) quit = 1;
            break;
        case KEY_CLICK:
            click();
            break;
        case KEY_TAB:
            select_begin();
            break;
        case KEY_HELP:
            open_help(TEXT_HELP);
            break;
        default:
            if (KEY_IS_MEGA(key)) {
                set_orders_menu();
                menu_command(menu_key(key));
            }
            else if (unit_active != UNIT_NONE) unit_key(key);
            else if (key == KEY_RETURN) game_end_turn();
            break;
        }
    }

    // The arrow stays for the title screen; the minimap's outline does not.
    sprite_hide(2);
    sprite_hide(3);
    return retired ? GAME_RETIRE : GAME_QUIT;
}
