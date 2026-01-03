
#ifndef CONTROLLER_H
#define CONTROLLER_H

// commandes abstraites (independantes du clavier)
typedef enum {
    CMD_NONE,
    CMD_MOVE_LEFT,
    CMD_MOVE_RIGHT,
    CMD_SHOOT,
    CMD_QUIT,
    CMD_PAUSE
} GameCommand;

typedef enum {
    BTN_LEFT,
    BTN_RIGHT,
    BTN_FIRE,
    BTN_QUIT,
    BTN_NONE,
    BTN_PAUSE,
    BTN_SELECT,
    BTN_UP,
    BTN_DOWN
} KEY_BOUTONS;

#endif // CONTROLLER_H
