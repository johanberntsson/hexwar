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
#include "help.h"
#include "ai.h"
#include "city.h"
#include "cityscreen.h"
#include "topcities.h"
#include "save.h"
#include "text.h"
#include "civ.h"
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

#define START_YEAR 1600         // BC; the Crisis is due about 1200
#define YEARS_PER_TURN 10

#define TEXT_COLOUR COLOR_LIGHTGREEN

// Where the view sits on the map, in screen characters; see view_redraw().
static word off_x;
static word off_y;

static byte dirty;              // the view wants redrawing
static byte hover_dirty;        // the readout of what is under the arrow
static byte game_over;
static byte quit;
static byte retired;            // quit by Game/Retire, for the title screen
static byte loaded;             // from a saved game: no opening popup
word game_turn;
word game_year;

// Frames since the game began, counted by game_run(), and the last click on
// one of the player's cities: a second click on the same hexagon within
// DOUBLE_CLICK frames opens it. Longer than the click debounce in input.c.
#define DOUBLE_CLICK 25
static word frame;
static word click_frame;
static byte click_ax = 0xff, click_ay;

// The active unit blinks: BLINK_FRAMES drawn, BLINK_FRAMES not, half a
// second each at 50 Hz. Each change is a whole redraw, which is what a held
// cursor key already does every frame, rather than a redraw of one hexagon:
// a unit's tile overlaps five neighbours, so putting the terrain back under
// it would mean redrawing parts of all of them in painter's order.
#define BLINK_FRAMES 25
static byte blink;

static const char *const terrain_name[] = {
    "Hills", "Grassland", "Mountains", "Ocean", "Shallows", "Tundra",
};

// ------------------------------------------------------------------- text

// A line of text built up a piece at a time, so that no printf is linked.
static char line[41];
static byte line_len;

static void line_clear(void)
{
    line_len = 0;
    line[0] = 0;
}

static void line_add(const char *s)
{
    while (*s && line_len < sizeof line - 1) line[line_len++] = *s++;
    line[line_len] = 0;
}

// "Hittite Warrior".
static void line_add_unit(byte owner, byte kind)
{
    line_add(player_adjective(owner));
    line_add(" ");
    line_add(unit_kind_name[kind]);
}

static void line_add_num(word v)
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

// Below the minimap and its frame, which take rows 1-10.
#define ROW_CIV 12
#define ROW_YEAR 13
#define ROW_TURN 14
#define ROW_UNIT 16
#define ROW_MOVES 17
#define ROW_PROMPT 19           // two rows
#define ROW_MESSAGE 22          // two rows

// Two lines of message in the panel. `second` may be `line`.
static void message(const char *first, const char *second)
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

static void draw_status(void)
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
static byte pointer_hex(byte *ax, byte *ay)
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

// What is under the arrow, right-aligned at the end of the top row: a city's
// name or whose unit it is, the terrain (or "Unexplored", which hides both), and the cell -- on
// the map window or on the minimap. Blank when the arrow is on neither.
static void draw_hover(void)
{
    byte ax, ay, c, u, x;

    line_clear();
    if (pointer_hex(&ax, &ay) || minimap_hex(input_x, input_y, &ax, &ay)) {
        if (!MAP_IS_EXPLORED(ax, ay)) {
            line_add("Unexplored");
        } else {
            // A city's name says whose it is; a unit needs telling.
            c = city_at(ax, ay);
            u = unit_at(ax, ay);
            if (c != CITY_NONE) {
                line_add(city_name_of(c));
                line_add("  ");
            } else if (u != UNIT_NONE) {
                line_add_unit(unit_owner[u], unit_kind[u]);
                line_add("  ");
            }
            line_add(terrain_name[MAP_TERRAIN(ax, ay)]);
        }
        line_add(" ");
        line_add_num(ax);
        line_add(",");
        line_add_num(ay);
    }
    // Padded on the left to a fixed field, so a shorter text overwrites a
    // longer one. The menu line ends at column 38.
    for (x = 40; x < SCREEN_COLUMNS - line_len; ++x)
        screen_putcxy(x, 0, ' ', COLOR_WHITE);
    screen_putsxy(SCREEN_COLUMNS - line_len, 0, line, COLOR_WHITE);
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

// ------------------------------------------------------------------- turns

// Hand the orders to the next unit after `after` that wants some, bringing
// it into view. With none left, the panel asks for the end of the turn.
static void select_unit(byte after)
{
    byte was = unit_active;
    byte u = unit_next(PLAYER_HUMAN, after);

    if (!player_alive(PLAYER_HUMAN)) {
        game_over = 1;
        u = UNIT_NONE;
    }
    unit_active = u;
    if (was != u) dirty = 1;   // another unit blinks
    if (u != UNIT_NONE && !in_view(unit_ax[u], unit_ay[u]))
        centre_on(unit_ax[u], unit_ay[u]);
    draw_orders();
}


// Then every city's production, the computer's included, so that a unit
// finished now is ready for the turn about to begin. With more than one
// city finishing, the panel names the last.
static void end_turn(void)
{
    byte c, kind;

    message("", "");
    news = NEWS_NONE;
    for (c = PLAYER_FIRST_AI; c < PLAYERS; ++c) ai_turn(c);
    ai_barbarians(game_turn);
    for (c = 0; c < city_count; ++c) {
        kind = city_produce(c);
        if (kind != KIND_NONE && city_owner[c] == PLAYER_HUMAN) {
            line_clear();
            line_add("built ");
            line_add(unit_kind_name[kind]);
            message(city_name_of(c), line);
            dirty = 1;
        }
    }
    // Being attacked matters more than what was built.
    if (news != NEWS_NONE) {
        static const char *const what[] = {
            "", "repelled", "a unit lost", "a city lost"
        };
        line_clear();
        line_add(player_adjective(news_owner));
        line_add(" attack");
        message(line, what[news]);
    }
    dirty = 1;
    hover_dirty = 1;
    ++game_turn;
    game_year -= YEARS_PER_TURN;
    unit_begin_turn(PLAYER_HUMAN);
    draw_status();
    select_unit(UNIT_NONE);
}

#ifdef DEBUG_MENU
// Debug: the computer plays the human's side for `n` turns, through the
// same ai_turn() as everybody else, or until Esc or the game is over. The
// panel's turn and the minimap keep up as it goes; the view is redrawn
// once, at the end, by the caller.
#define AUTOPLAY_TURNS 50

static void autoplay(byte n)
{
    while (n-- && !game_over) {
        ai_turn(PLAYER_HUMAN);
        end_turn();
        if (input_poll() == KEY_ESC) break;
    }
}
#endif

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
    t = unit_can_enter(PLAYER_HUMAN, nx, ny);
    if (t == ENTER_WATER) {
        message("Cannot swim", "");
        return;
    }
    if (t == ENTER_FOREIGN) {
        attack(u, nx, ny);
        return;
    }
    message("", "");
    unit_move_to(u, nx, ny);
    --unit_moves[u];
    dirty = 1;
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

static void unit_key(byte key)
{
    byte u = unit_active;

    if (!input_real_hw && key >= KEY_PAD_FIRST && key <= KEY_PAD_LAST) {
        move_active(pad_dir[(byte)(key - KEY_PAD_FIRST)]);
        return;
    }

    switch (key) {
    // The same pattern as q e / a d / z c, on the right of the keyboard
    // beside the cursor keys. The key left of = is : unshifted and [
    // shifted, so both are taken.
    case '0': move_active(DIR_NW); break;
    case '-': move_active(DIR_NE); break;
    case 'p': move_active(DIR_W); break;
    case '*': move_active(DIR_E); break;
    case ':':
    case '[': move_active(DIR_SW); break;
    case '=': move_active(DIR_SE); break;
    case 'b': found_city(); break;
    case KEY_SPACE:                         // no orders this turn
        unit_moves[u] = 0;
        select_unit(u);
        break;
    case 'w':                               // ask again after the others
        select_unit(u);
        break;
    case 'f':
        unit_orders[u] = ORDERS_FORTIFY;
        select_unit(u);
        break;
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
    draw_screen();
}

static void open_help(void)
{
    help_screen();
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
    byte command = MENU_NONE;
    if (unit_active != UNIT_NONE) {
        byte kind = unit_kind[unit_active];
        if (kind == KIND_SETTLER) command = MENU_FOUND_CITY;
        else if (kind == KIND_WARRIOR) command = MENU_FORTIFY;
    }
    menu_orders(command);
}

// Do what a menu item asks for, picked by click or by key. Whatever it
// does, the view is redrawn afterwards, which puts back the minimap's
// outline the dropdown hid.
static void menu_command(byte command)
{
    switch (command) {
    case MENU_RETIRE:
        if (popup_ask("Do you want to quit this game?")) {
            retired = 1;
            quit = 1;
        }
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
        autoplay(AUTOPLAY_TURNS);
        break;
#endif
    case MENU_SAVE:
        message("Saving", "");
        message(save_write() ? "Game saved" : "Could not save", "");
        break;
    case MENU_FOUND_CITY:
        unit_key('b');
        break;
    case MENU_FORTIFY:
        unit_key('f');
        break;
    case MENU_TOP_CITIES:
        open_top_cities();
        break;
    case MENU_KEYS:
        open_help();
        break;
    }
    dirty = 1;
}

// A click on a hexagon next to the unit with orders moves it there. A click
// on one of the player's own units that still has moves gives it the orders
// instead, waking it if it was fortified. Anywhere else on the map, or on
// the minimap, centres the view there; elsewhere, with the turn over, it
// ends the turn.
static void click(void)
{
    byte ax, ay, u, dir, c, own;

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
        if (unit_active == UNIT_NONE && !game_over) end_turn();
        return;
    }

    // A double click on one of the player's cities opens it. The first
    // click of the two does what a click does, except that it never centres
    // the view -- which would move the city out from under the second.
    c = city_at(ax, ay);
    own = c != CITY_NONE && city_owner[c] == PLAYER_HUMAN;
    if (own && ax == click_ax && ay == click_ay
        && (word)(frame - click_frame) < DOUBLE_CLICK) {
        click_ax = 0xff;
        open_city(c);
        return;
    }
    click_ax = own ? ax : 0xff;
    click_ay = ay;
    click_frame = frame;

    if (unit_active != UNIT_NONE) {
        dir = map_direction(unit_ax[unit_active], unit_ay[unit_active],
                            ax, ay);
        if (dir != DIR_NONE) {
            move_active(dir);
            return;
        }
        if (unit_ax[unit_active] == ax && unit_ay[unit_active] == ay) return;
    }
    for (u = 0; u < MAX_UNITS; ++u) {
        if (unit_kind[u] != KIND_NONE && unit_owner[u] == PLAYER_HUMAN
            && unit_ax[u] == ax && unit_ay[u] == ay && unit_moves[u]) {
            unit_orders[u] = ORDERS_NONE;
            unit_active = u;
            dirty = 1;
            draw_orders();
            return;
        }
    }
    if (!own) centre_on(ax, ay);
}

// --------------------------------------------------------------- the game

// The land cell nearest (cx, cy) of the given terrain, searching outwards in
// square rings of the array. Not hexagon distance, but this only has to find
// somewhere sensible to start.
static byte find_terrain(byte cx, byte cy, byte want, byte *rx, byte *ry)
{
    int r, dx, dy, y;
    byte x;

    for (r = 0; r < MAP_COLUMNS / 2; ++r) {
        for (dy = -r; dy <= r; ++dy) {
            y = (int)cy + dy;
            if (y < 0 || y >= MAP_ROWS) continue;
            for (dx = -r; dx <= r; ++dx) {
                if (dx != -r && dx != r && dy != -r && dy != r) continue;
                x = (byte)((int)cx + dx + MAP_COLUMNS) & (MAP_COLUMNS - 1);
                if (MAP_TERRAIN(x, y) == want) {
                    *rx = x;
                    *ry = (byte)y;
                    return 1;
                }
            }
        }
    }
    return 0;
}

void game_init(void)
{
    byte cx, cy, ax, ay;

    // Nothing left over from a game before this one.
    map_reset();
    unit_reset();
    city_reset();
    combat_reset();
    game_over = quit = retired = loaded = 0;
    click_ax = 0xff;

    // START_OFF_X / START_OFF_Y are what let a headless run check
    // scrolling: `make START_OFF_X=41` against a plain build must shift the
    // map exactly eight pixels and leave the panel alone. The settler goes
    // on the grassland nearest the middle of that view, so it is the same
    // settler in both builds and neither has to recentre on it.
    off_x = START_OFF_X;
    off_y = START_OFF_Y;
    map_cell_at(off_x + VIEW_COLUMNS / 2, off_y + VIEW_ROWS / 2, &cx, &cy);
    if (!find_terrain(cx, cy, TILE_GRASS, &ax, &ay)
        && !find_terrain(cx, cy, TILE_DESERT_HILL, &ax, &ay)) {
        ax = cx;
        ay = cy;
    }
    unit_create(KIND_SETTLER, PLAYER_HUMAN, ax, ay);
    ai_place();

    game_turn = 1;
    game_year = START_YEAR;
    dirty = 1;
    hover_dirty = 1;
}

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
        ++frame;
        // Anything that changed the view shows the active unit at once and
        // starts its blink again, so that it is there to see as soon as it
        // has moved or been selected, and while the view is scrolled.
        if (dirty) {
            unit_hidden = 0;
            blink = 0;
        } else if (unit_active != UNIT_NONE && ++blink == BLINK_FRAMES) {
            unit_hidden ^= 1;
            blink = 0;
            dirty = 1;
        }
        if (dirty) {
            view_redraw(off_x, off_y);
            minimap_frame(off_x, off_y);
            dirty = 0;
            hover_dirty = 1;
        }

        key = input_poll();
        if (input_moved()) hover_dirty = 1;
        if (hover_dirty) {
            draw_hover();
            hover_dirty = 0;
        }
        if (!key) continue;
        if (key >= 'A' && key <= 'Z') key += 'a' - 'A';

        if (game_over) {
            quit = 1;
            continue;
        }

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
        case KEY_HELP:
            open_help();
            break;
        default:
            if (KEY_IS_MEGA(key)) {
                set_orders_menu();
                menu_command(menu_key(key));
            }
            else if (unit_active != UNIT_NONE) unit_key(key);
            else if (key == KEY_RETURN) end_turn();
            break;
        }
    }

    // The arrow stays for the title screen; the minimap's outline does not.
    sprite_hide(2);
    sprite_hide(3);
    return retired ? GAME_RETIRE : GAME_QUIT;
}
