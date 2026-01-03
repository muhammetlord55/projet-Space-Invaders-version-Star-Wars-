//
//  view.h
//
//
//  Created by Cakir on 24/12/2025.
//
#ifndef VIEW_H
#define VIEW_H

#include "model.h"
#include "controller.h" // Indispensable pour connaître KEY_BOUTONS

typedef struct
{
    void (*init)(void);
    void (*close)(void);
    void (*render)(const GameModel *model);

    KEY_BOUTONS (*get_input)(void);
} GameView;

GameView view_ncurses_get_interface(void);
GameView view_sdl_get_interface(void);
void play_item_sound(void);
void play_explosion_sound(void);
void play_shoot_sound(void);

#endif