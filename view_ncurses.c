//
//  view_ncurses.c
//
//
//  Created by Cakir on 24/12/2025.
//
#include "view.h"
#include <ncurses.h>
#include <string.h>

// Convertit les coordonnées du jeu (800x600) vers le terminal
static void transform_coords(float gx, float gy, int *tx, int *ty)
{
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    // Simple règle de trois
    *tx = (int)(gx * max_x / GAME_WIDTH);
    *ty = (int)(gy * max_y / GAME_HEIGHT);
}

// --- Implémentation de l'Interface ---

static void ncurses_init(void)
{
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    timeout(0); // Non-bloquant
    keypad(stdscr, TRUE);
}

static void ncurses_close(void)
{
    endwin();
}

static KEY_BOUTONS ncurses_get_input(void)
{
    int ch = getch();
    if (ch == ERR)
        return BTN_NONE;
    if (ch == 'q' || ch == 'Q' || ch == 27)
        return BTN_QUIT;
    if (ch == ' ')
        return BTN_FIRE;
    if (ch == '\n' || ch == '\r' || ch == KEY_ENTER)
        return BTN_SELECT;
    if (ch == 'p' || ch == 'P')
        return BTN_PAUSE;
    if (ch == KEY_LEFT)
        return BTN_LEFT;
    if (ch == KEY_RIGHT)
        return BTN_RIGHT;
    if (ch == KEY_UP)
        return BTN_UP;
    if (ch == KEY_DOWN)
        return BTN_DOWN;
    return BTN_NONE;
}

static void ncurses_render(const GameModel *model)
{
    clear();

    int tx, ty;

    // 1. Dessiner le Joueur
    transform_coords(model->player.x, model->player.y, &tx, &ty);
    mvaddch(ty, tx, 'A' | A_BOLD);

    // Afficher le bouclier si actif
    if (model->player.shield)
    {
        mvaddch(ty - 1, tx, 'O');
        mvaddch(ty + 1, tx, 'O');
        mvaddch(ty, tx - 1, 'O');
        mvaddch(ty, tx + 1, 'O');
    }

    // 2. Dessiner le BOSS (si actif)
    if (model->boss.active)
    {
        transform_coords(model->boss.x + model->boss.width / 2, model->boss.y + model->boss.height / 2, &tx, &ty);
        mvprintw(ty, tx, "[ BOSS (%d) ]", model->boss.hp);
    }
    else
    {
        // Dessiner les Aliens (seulement si pas de boss)
        for (int i = 0; i < MAX_ALIENS; i++)
        {
            if (model->aliens[i].active)
            {
                transform_coords(model->aliens[i].x, model->aliens[i].y, &tx, &ty);
                mvaddch(ty, tx, '@');
            }
        }
    }

    // 3. Dessiner les Balles
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (model->bullets[i].active)
        {
            transform_coords(model->bullets[i].x, model->bullets[i].y, &tx, &ty);
            char c = '|';
            if (model->bullets[i].type == ENTITY_BULLET_BOSS)
                c = '!'; // Balle de boss
            mvaddch(ty, tx, c);
        }
    }

    // Dessiner les items
    for (int i = 0; i < ITEMS_MAX; i++)
    {
        if (model->items[i].active)
        {
            transform_coords(model->items[i].x, model->items[i].y, &tx, &ty);
            mvaddch(ty, tx, '*');
        }
    }

    // Infos
    mvprintw(0, 0, "Score: %d  Vies: %d  Level: %d", model->score, model->lives, model->level);

    // Menu / Pause
    if (model->menu_mode != 0)
    {
        int mid_y = LINES / 2 + 4;
        int mid_x = COLS / 2;
        if (model->menu_mode == 1)
        {
            const char *title = "=== SPACE INVADERS ===";
            mvprintw(mid_y - 2, mid_x - (int)strlen(title) / 2, "%s", title);
            const char *items[] = {"Start Game", "Settings", "High Scores", "Quit"};
            for (int i = 0; i < 4; ++i)
            {
                int tx = mid_x - (int)strlen(items[i]) / 2;
                if (i == model->menu_selection)
                    attron(A_REVERSE);
                mvprintw(mid_y + i, tx, "%s", items[i]);
                if (i == model->menu_selection)
                    attroff(A_REVERSE);
            }
        }
        else if (model->menu_mode == 2)
        {
            const char *s = "SETTINGS - Press ESC to return";
            mvprintw(mid_y, mid_x - (int)strlen(s) / 2, "%s", s);
        }
        else if (model->menu_mode == 3)
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "HIGH SCORE: %d", model->high_score);
            mvprintw(mid_y, mid_x - (int)strlen(buf) / 2, "%s", buf);
            const char *r = "Press ESC to return";
            mvprintw(mid_y + 1, mid_x - (int)strlen(r) / 2, "%s", r);
        }
        else if (model->menu_mode == 4)
        {
            const char *p = "PAUSED";
            mvprintw(mid_y - 2, mid_x - (int)strlen(p) / 2, "%s", p);
            const char *items[] = {"Resume", "Settings", "High Scores", "Quit"};
            for (int i = 0; i < 4; ++i)
            {
                int tx = mid_x - (int)strlen(items[i]) / 2;
                if (i == model->menu_selection)
                    attron(A_REVERSE);
                mvprintw(mid_y + i, tx, "%s", items[i]);
                if (i == model->menu_selection)
                    attroff(A_REVERSE);
            }
        }
    }

    refresh();
}

// Fonction publique pour récupérer l'interface
GameView view_ncurses_get_interface(void)
{
    GameView v;
    v.init = ncurses_init;
    v.close = ncurses_close;
    v.render = ncurses_render;
    v.get_input = ncurses_get_input;
    return v;
}

void play_item_sound(void) __attribute__((weak));
void play_item_sound(void) {}

void play_explosion_sound(void) __attribute__((weak));
void play_explosion_sound(void) {}

void play_shoot_sound(void) __attribute__((weak));
void play_shoot_sound(void) {}