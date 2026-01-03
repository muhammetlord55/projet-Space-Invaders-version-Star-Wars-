//
//  model.h

//
//  Created by Cakir on 24/12/2025.
//
#define GAME_WIDTH 1280
#define GAME_HEIGHT 800
#include <stdbool.h>
#define PLAYER_SPEED 7000.0f
#define ALIEN_SPEED 800.f
#define BULLET_SPEED 4000.0f

// CONFIGURATION DU BOSS
#define BOSS_W 180
#define BOSS_H 160
#define BOSS_HP_BASE 50
#define BOSS_SPEED 600.0f

#define MAX_ALIENS 55 // 5 rangeesde 11 aliens
#define MAX_BULLETS 100

#define EXPLOSION_MAX 20
#define EXPLOSION_TIME 0.2f
#define ITEMS_MAX 10

typedef enum
{
    ENTITY_PLAYER,
    ENTITY_ALIEN,
    ENTITY_BOSS, // Nouveau type
    ENTITY_ITEMS,
    ENTITY_BULLET_PLAYER,
    ENTITY_BULLET_ALIEN,
    ENTITY_BULLET_BOSS, // Nouveau type de balle
    ENTITY_EXPLOSION
} EntityType;

typedef struct
{
    float x, y;
    float dx, dy;      // vitesse
    int width, height; // Hitbox logique
    int hp;
    int active; // vivant ou mort
    EntityType type;
    bool shield; // bouclier actif pour le joueur/items
} Entity;

typedef struct
{
    Entity player;
    Entity boss; // L'entité du Boss
    Entity aliens[MAX_ALIENS];
    Entity bullets[MAX_BULLETS];
    Entity explosions[EXPLOSION_MAX];
    Entity items[ITEMS_MAX];
    int score;
    int lives;
    int level;
    bool game_over;

    // Pour gérer le mouvement de groupe des aliens
    float alien_move_timer;
    int alien_direction;
    float respawn_timer;
    int menu_mode;      // 0 = none, 1 = start menu, 2 = settings, 3 = highscores, 4 = paused
    int menu_selection; // index sélectionné dans le menu
    int high_score;     // meilleur score enregistré (simple mémoire en RAM)
    bool paused;
    float alien_speed_multiplier; // multiplie ALIEN_SPEED pour augmenter la difficulté

} GameModel;

// Initialise toutes les variables (positions de départ)
void model_init(GameModel *game);

// Met à jour la position de tout le monde en fonction du temps écoulé (dt)
void model_update(GameModel *game, float delta_time);

// Déplace le joueur (-1 pour gauche, +1 pour droite, 0 pour stop)
void model_move_player(GameModel *game, float dx, float dy);

// Tire une balle (depuis le joueur ou un alien)
void model_fire_bullet(GameModel *game, float x, float y, EntityType type);
void alien_tire(GameModel *game);
void spawn_explosion(GameModel *game, float x, float y);

// Audio callback registration (model triggers events; view registers handlers)
typedef void (*AudioCallback)(void);
void init_items(GameModel *game, float x, float y);
void model_set_audio_callbacks(void (*on_item)(void), void (*on_explosion)(void), void (*on_shoot)(void));
void model_set_audio_callbacks(AudioCallback on_item, AudioCallback on_explosion, AudioCallback on_shoot);