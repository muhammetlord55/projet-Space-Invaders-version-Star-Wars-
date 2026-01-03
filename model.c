//
//  model.c
//
//  Created by Cakir on 24/12/2025.
//

#include <stdio.h>
#include "model.h"
#include <stdbool.h>
#include <controller.h>
#include <stdlib.h>
#include <math.h> // Pour abs()

#define PLAYER_W 90
#define PLAYER_H 100
#define ALIEN_W 40
#define ALIEN_H 50
#define BULLET_W 10
#define BULLET_H 38
#define ALIEN_DROP_DOWN 20.0f
#define EXPLOSION_SIZE 59
#define RESPAWN_DELAY 4.0f
#define ITEMS_SIZE 59

// Audio callbacks (set by the view layer)
static void (*cb_play_item)(void) = NULL;
static void (*cb_play_explosion)(void) = NULL;
static void (*cb_play_shoot)(void) = NULL;

void model_set_audio_callbacks(void (*on_item)(void), void (*on_explosion)(void), void (*on_shoot)(void))
{
    cb_play_item = on_item;
    cb_play_explosion = on_explosion;
    cb_play_shoot = on_shoot;
}

// Fonction pour faire apparaître le BOSS
static void spawn_boss(GameModel *game)
{
    game->boss.active = true;
    game->boss.type = ENTITY_BOSS;
    game->boss.width = BOSS_W;
    game->boss.height = BOSS_H;

    // 1. Position initiale : Hors de l'écran à droite
    game->boss.x = GAME_WIDTH + 20.0f;
    game->boss.y = 80.0f;

    // 2. PV Fixes : 100
    game->boss.hp = 100;

    // 3. Vitesse d'approche RAPIDE (Même vitesse que combat)
    // On utilise dy comme un "Flag d'état" : 0 = Approche, 1 = Combat
    // ALIEN_SPEED * 1.3f est la vitesse de combat définie plus bas
    float approach_speed = ALIEN_SPEED * 2.4f;
    game->boss.dx = -approach_speed; // Avance vers la gauche (VITE)
    game->boss.dy = 0;               // État 0 : En approche

    printf("⚠️ ALERTE : BOSS ARRIVE ! (HP: %d)\n", game->boss.hp);
}

// Helper: spawn aliens grid (same layout as init)
static void spawn_aliens(GameModel *game)
{
    int rows = 5;
    int cols = 11;
    int count = 0;

    float start_x = 100;
    float start_y = 50;
    float gap_x = 28; // espacement horizontal augmenté
    float gap_y = 22; // espacement vertical augmenté

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            if (count >= MAX_ALIENS)
                break;
            Entity *alien = &game->aliens[count];
            alien->x = start_x + c * (ALIEN_W + gap_x);
            alien->y = start_y + r * (ALIEN_H + gap_y);
            alien->width = ALIEN_W;
            alien->height = ALIEN_H;
            alien->active = true;
            alien->type = ENTITY_ALIEN;
            count++;
        }
    }
}

// Initialise toutes les variables (positions de départ)
void model_init(GameModel *game)
{
    // Reset game state
    game->score = 0;
    game->lives = 3;
    game->level = 1;
    game->game_over = 0;
    game->alien_direction = 1; // vers la droite au debut

    // Init Joueur
    game->player.x = (GAME_WIDTH - PLAYER_W) / 2.0f;
    game->player.y = (GAME_HEIGHT - PLAYER_H - 10);
    game->player.width = PLAYER_W;
    game->player.height = PLAYER_H;
    game->player.dx = 0;
    game->player.dy = 0;
    game->player.shield = false;
    game->player.active = true;
    game->player.type = ENTITY_PLAYER;

    // Pas de boss au niveau 1
    game->boss.active = false;

    // Init Aliens
    spawn_aliens(game);

    for (int i = 0; i < MAX_BULLETS; i++)
    {
        game->bullets[i].active = false;
    }
    for (int i = 0; i < EXPLOSION_MAX; i++)
    {
        game->explosions[i].active = false;
    }
    // Items
    for (int i = 0; i < ITEMS_MAX; i++)
    {
        game->items[i].active = false;
    }

    game->menu_mode = 0;
    game->menu_selection = 0;
    game->paused = false;
    game->alien_speed_multiplier = 1.0f;
}

// Called when player clears all aliens or kills boss
static void level_up(GameModel *game)
{
    game->level += 1;
    game->alien_speed_multiplier *= 1.15f;

    // Nettoyage des balles
    for (int i = 0; i < MAX_BULLETS; i++)
        game->bullets[i].active = false;

    game->respawn_timer = 0.0f;

    // LOGIQUE BOSS : Tous les 3 niveaux (3, 6, 9...)
    if (game->level % 3 == 0)
    {
        printf("➡️ Niveau %d : BOSS BATTLE !\n", game->level);
        // Désactiver les aliens s'il y en a (sécurité)
        for (int i = 0; i < MAX_ALIENS; i++)
            game->aliens[i].active = false;

        spawn_boss(game);
    }
    else
    {
        printf("➡️ Niveau suivant ! Level %d\n", game->level);
        game->boss.active = false; // S'assurer que le boss est parti
        spawn_aliens(game);
    }
}

static bool check_collision(Entity *a, Entity *b)
{
    if (!a->active || !b->active)
        return false;

    return (a->x < b->x + b->width &&
            a->x + a->width > b->x &&
            a->y < b->y + b->height &&
            a->y + a->height > b->y);
}

void spawn_explosion(GameModel *game, float x, float y)
{
    for (int i = 0; i < EXPLOSION_MAX; i++)
    {
        if (!game->explosions[i].active)
        {
            game->explosions[i].active = true;
            game->explosions[i].type = ENTITY_EXPLOSION;
            game->explosions[i].x = x;
            game->explosions[i].y = y;
            game->explosions[i].width = EXPLOSION_SIZE;
            game->explosions[i].height = EXPLOSION_SIZE;
            game->explosions[i].dx = EXPLOSION_TIME;
            if (cb_play_explosion)
                cb_play_explosion();
            break;
        }
    }
}

void init_items(GameModel *game, float x, float y)
{
    for (int i = 0; i < ITEMS_MAX; i++)
    {
        if (!game->items[i].active)
        {
            game->items[i].active = true;
            game->items[i].type = ENTITY_ITEMS;
            game->items[i].x = x - ITEMS_SIZE / 2.0f;
            game->items[i].y = y - ITEMS_SIZE / 2.0f;
            game->items[i].width = ITEMS_SIZE;
            game->items[i].height = ITEMS_SIZE;
            game->items[i].dx = 0;
            game->items[i].dy = 900.0f;
        }
    }
}

// Met à jour la position de tout le monde en fonction du temps écoulé (dt)
void model_update(GameModel *game, float delta_time)
{
    if (game->game_over)
        return;

    // --- A. JOUEUR ---
    game->player.x += game->player.dx * delta_time;
    game->player.y += game->player.dy * delta_time;

    if (game->player.x < 0)
        game->player.x = 0;
    if (game->player.x + game->player.width > GAME_WIDTH)
        game->player.x = GAME_WIDTH - game->player.width;
    if (game->player.y < 0)
        game->player.y = 0;
    if (game->player.y + game->player.height > GAME_HEIGHT)
        game->player.y = GAME_HEIGHT - game->player.height;

    // --- GESTION BOSS OU ALIENS ---
    if (game->boss.active)
    {
        game->boss.x += game->boss.dx * delta_time;

        // PHASE 1 : APPROCHE (Boss arrive de la droite)
        if (game->boss.dy == 0)
        {
            // Vérifie si le Boss a atteint le milieu de l'écran
            if (game->boss.x + game->boss.width / 2 <= GAME_WIDTH / 2)
            {
                // ACTIVATION DU BOSS !
                game->boss.dy = 1; // Passe en mode Combat (État 1)

                // Vitesse augmentée : Plus rapide que les Aliens normaux (800)
                // Par exemple 1000 pixels/sec
                float combat_speed = ALIEN_SPEED * 1.3f;

                // Continue vers la gauche ou repart aléatoirement ? On continue pour la fluidité
                game->boss.dx = -combat_speed;

                printf("⚠️ BOSS ACTIVÉ ! MODE COMBAT ENGAGÉ !\n");
            }
        }
        // PHASE 2 : COMBAT (Boss activé)
        else
        {
            // Rebond sur TOUS les murs (Gauche et Droite)
            if (game->boss.x <= 0)
            {
                game->boss.x = 0;
                game->boss.dx = fabsf(game->boss.dx); // Vers la droite
            }
            else if (game->boss.x + game->boss.width >= GAME_WIDTH)
            {
                game->boss.x = GAME_WIDTH - game->boss.width;
                game->boss.dx = -fabsf(game->boss.dx); // Vers la gauche
            }

            // TIRS : Maintenant qu'il est activé, il tire n'importe où
            if ((rand() % 100) < 5)
            {
                float x = game->boss.x + game->boss.width / 2.0f;
                float y = game->boss.y + game->boss.height;
                model_fire_bullet(game, x, y, ENTITY_BULLET_BOSS);
            }
        }
    }
    else
    {
        // LOGIQUE ALIENS CLASSIQUE (seulement si pas de boss)
        for (int i = 0; i < MAX_ALIENS; i++)
        {
            if (game->aliens[i].active)
            {
                game->aliens[i].x += (ALIEN_SPEED * game->alien_speed_multiplier * game->alien_direction) * delta_time;
            }
        }

        bool touch_edge = false;
        for (int i = 0; i < MAX_ALIENS; i++)
        {
            if (!game->aliens[i].active)
                continue;
            if (game->alien_direction == 1 && game->aliens[i].x + game->aliens[i].width >= GAME_WIDTH - 10)
            {
                touch_edge = true;
                break;
            }
            if (game->alien_direction == -1 && game->aliens[i].x <= 10)
            {
                touch_edge = true;
                break;
            }
        }

        if (touch_edge)
        {
            game->alien_direction *= -1;
            for (int i = 0; i < MAX_ALIENS; i++)
            {
                if (game->aliens[i].active)
                {
                    game->aliens[i].y += ALIEN_DROP_DOWN;
                    game->aliens[i].x += (game->alien_direction * 5);
                }
            }
        }

        if ((rand() % 100) < 4)
        {
            int random_index = rand() % MAX_ALIENS;
            if (game->aliens[random_index].active)
            {
                float x = game->aliens[random_index].x + ALIEN_W / 2;
                float y = game->aliens[random_index].y + ALIEN_H;
                model_fire_bullet(game, x, y, ENTITY_BULLET_ALIEN);
            }

            // Game Over si alien touche le bas
            if (game->aliens[random_index].active && (game->aliens[random_index].y + ALIEN_H >= game->player.y))
            {
                if (game->player.shield)
                {
                    game->player.shield = false;
                    game->aliens[random_index].active = false;
                    spawn_explosion(game, game->aliens[random_index].x, game->aliens[random_index].y);
                }
                else
                {
                    game->game_over = true;
                    if (game->score > game->high_score)
                        game->high_score = game->score;
                }
            }
        }
    }

    // --- C. BALLES & COLLISIONS ---
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        Entity *b = &game->bullets[i];

        if (b->active)
        {
            b->y += b->dy * delta_time;

            if (b->y < 0 || b->y > GAME_HEIGHT)
            {
                b->active = false;
                continue;
            }

            // TIR DU JOUEUR
            if (b->type == ENTITY_BULLET_PLAYER)
            {
                // Contre BOSS
                if (game->boss.active && check_collision(b, &game->boss))
                {
                    b->active = false;
                    spawn_explosion(game, b->x, b->y); // Petite explosion impact
                    game->boss.hp--;

                    if (game->boss.hp <= 0)
                    {
                        game->boss.active = false;
                        game->score += 10000; // BONUS 10,000 POINTS
                        spawn_explosion(game, game->boss.x, game->boss.y);
                        // Chance de drop item
                        init_items(game, game->boss.x + BOSS_W / 2, game->boss.y + BOSS_H / 2);
                        // Niveau suivant immédiat
                        level_up(game);
                    }
                    continue; // Balle détruite, on passe à la suivante
                }

                // Contre ALIENS
                if (!game->boss.active)
                {
                    for (int j = 0; j < MAX_ALIENS; j++)
                    {
                        Entity *alien = &game->aliens[j];
                        if (alien->active && check_collision(b, alien))
                        {
                            alien->active = false;
                            b->active = false;
                            spawn_explosion(game, alien->x, alien->y);
                            game->score += 100;
                            if ((rand() % 100) < 5)
                                init_items(game, alien->x + alien->width / 2.0f, alien->y + alien->height / 2.0f);
                            break;
                        }
                    }
                }
            }

            // TIR ENNEMIS (Alien ou Boss)
            if (b->type == ENTITY_BULLET_ALIEN || b->type == ENTITY_BULLET_BOSS)
            {
                Entity *player = &game->player;
                if (player->active && check_collision(b, player))
                {
                    if (player->shield)
                    {
                        player->shield = false;
                        spawn_explosion(game, player->x, player->y);
                        b->active = false;
                    }
                    else
                    {
                        player->active = false;
                        spawn_explosion(game, player->x, player->y);
                        b->active = false;
                        game->lives -= 1;
                        if (game->lives <= 0)
                        {
                            game->game_over = true;
                            if (game->score > game->high_score)
                                game->high_score = game->score;
                        }
                        else
                        {
                            game->player.x = (GAME_WIDTH - PLAYER_W) / 2.0f;
                            game->player.y = (GAME_HEIGHT - PLAYER_H - 10);
                            game->player.active = true;
                            game->respawn_timer = RESPAWN_DELAY;
                        }
                    }
                }
            }
        }
    }

    // Mise à jour explosions
    for (int i = 0; i < EXPLOSION_MAX; i++)
    {
        if (game->explosions[i].active)
        {
            game->explosions[i].dx -= delta_time;
            if (game->explosions[i].dx <= 0)
                game->explosions[i].active = false;
        }
    }

    // --- D. ITEMS ---
    for (int i = 0; i < ITEMS_MAX; i++)
    {
        Entity *it = &game->items[i];
        if (!it->active)
            continue;
        it->y += it->dy * delta_time;
        if (it->y > GAME_HEIGHT)
        {
            it->active = false;
            continue;
        }

        if (game->player.active && check_collision(it, &game->player))
        {
            game->player.shield = true;
            game->score += 50;
            it->active = false;
            if (cb_play_item)
                cb_play_item();
        }
    }

    // --- E. LEVEL CHECK (Seulement si pas de boss actif) ---
    // Si on est dans un niveau normal (pas multiple de 3) et qu'il n'y a plus d'aliens
    if (!game->boss.active && (game->level % 3 != 0))
    {
        int alive_count = 0;
        for (int i = 0; i < MAX_ALIENS; i++)
            if (game->aliens[i].active)
                alive_count++;

        if (alive_count == 0)
        {
            level_up(game);
        }
    }
}

// Déplace le jouer ()
void model_move_player(GameModel *game, float dx, float dy)
{
    game->player.dx = dx * PLAYER_SPEED;
    game->player.dy = dy * PLAYER_SPEED;
}

// Tire une balle (depuis le joueur ou un alien)
void model_fire_bullet(GameModel *game, float x, float y, EntityType type)
{
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (!game->bullets[i].active)
        {
            game->bullets[i].active = true;
            game->bullets[i].type = type;
            game->bullets[i].x = x;
            game->bullets[i].y = y;
            game->bullets[i].width = BULLET_W;
            game->bullets[i].height = BULLET_H;

            if (type == ENTITY_BULLET_PLAYER)
            {
                if (cb_play_shoot)
                    cb_play_shoot();
                game->bullets[i].dy = -BULLET_SPEED;
            }
            else
            {
                // Aliens et Boss tirent vers le bas
                // La balle du boss est un peu plus rapide
                float speed = (type == ENTITY_BULLET_BOSS) ? BULLET_SPEED * 1.5f : BULLET_SPEED;
                game->bullets[i].dy = speed * game->alien_speed_multiplier;
            }
            break;
        }
    }
}