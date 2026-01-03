#include "view.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

/* Include the mixer header that matches the SDL core in use. */
#if defined(__has_include)
#if defined(SDL_MAJOR_VERSION) && (SDL_MAJOR_VERSION >= 3)
#if __has_include(<SDL3_mixer/SDL_mixer.h>)
#include <SDL3_mixer/SDL_mixer.h>
#define HAVE_SDL3_MIXER 1
#define HAVE_SDL_MIXER 1
#endif
#else
#if __has_include(<SDL_mixer.h>)
#include <SDL_mixer.h>
#define HAVE_SDL2_MIXER 1
#define HAVE_SDL_MIXER 1
#endif
#endif
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include "controller.h"

// --- CONFIGURATION ---
#define EXPLOSION_NB_FRAMES 6
#define EXPLOSION_DURATION 0.2f
#define EXPLOSION_SCALE 1.5f
#define SHIELD_PADDING 45

#define MAX_STARS 200
#define STAR_SPEED_MIN 0.5f
#define STAR_SPEED_MAX 3.0f

typedef struct
{
    float x, y;
    float speed;
    int brightness;
} Star;

static Star stars[MAX_STARS];

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

// Textures
static SDL_Texture *tex_player = NULL;
static SDL_Texture *tex_alien = NULL;
static SDL_Texture *tex_boss = NULL; 
static SDL_Texture *tex_bullet = NULL;
static SDL_Texture *tex_explosion = NULL;
static SDL_Texture *tex_items = NULL;
static SDL_Texture *tex_bouclier = NULL;

// Police
static TTF_Font *font = NULL;

#if HAVE_SDL_MIXER
static Mix_Music *bgm = NULL;
static Mix_Chunk *sfx_item = NULL;
static Mix_Chunk *sfx_explosion = NULL;
static Mix_Chunk *sfx_shoot = NULL;
#else
static pid_t bgm_pid = 0;
#endif

// --- OUTILS ---

static void init_stars()
{
    for (int i = 0; i < MAX_STARS; i++)
    {
        stars[i].x = rand() % GAME_WIDTH;
        stars[i].y = rand() % GAME_HEIGHT;
        float ratio = (float)rand() / RAND_MAX;
        stars[i].speed = STAR_SPEED_MIN + ratio * (STAR_SPEED_MAX - STAR_SPEED_MIN);
        stars[i].brightness = 100 + (int)(ratio * 155);
    }
}

static void update_and_draw_stars()
{
    for (int i = 0; i < MAX_STARS; i++)
    {
        stars[i].y += stars[i].speed;
        if (stars[i].y > GAME_HEIGHT)
        {
            stars[i].y = 0;
            stars[i].x = rand() % GAME_WIDTH;
        }
        SDL_SetRenderDrawColor(renderer, stars[i].brightness, stars[i].brightness, stars[i].brightness, 255);
        SDL_RenderPoint(renderer, stars[i].x, stars[i].y);
        if (stars[i].speed > 2.0f)
        {
            SDL_RenderPoint(renderer, stars[i].x + 1, stars[i].y);
            SDL_RenderPoint(renderer, stars[i].x, stars[i].y + 1);
            SDL_RenderPoint(renderer, stars[i].x + 1, stars[i].y + 1);
        }
    }
}

static SDL_Texture *load_texture(const char *filename)
{
    char path[256];
    snprintf(path, sizeof(path), "assets/%s", filename);

    char abs_path[PATH_MAX];
    if (realpath(path, abs_path) == NULL)
    {
        printf("❌ ERREUR : Fichier introuvable -> %s\n", path);
        return NULL;
    }

    SDL_Surface *surface = IMG_Load(abs_path);
    if (!surface)
    {
        printf("⚠️ Erreur IMG_Load : %s\n", SDL_GetError());
        return NULL;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture)
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDL_DestroySurface(surface);
    return texture;
}

// --- FONCTIONS DESSIN TEXTE AMÉLIORÉES (AVEC FALLBACK) ---

static void draw_text_internal(const char *text, float x, float y, SDL_Color color, bool centered, float fallback_scale)
{
    if (font)
    {
        SDL_Surface *surface = TTF_RenderText_Solid(font, text, strlen(text), color);
        if (surface)
        {
            SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture)
            {
                SDL_FRect dest_rect;
                dest_rect.w = (float)surface->w;
                dest_rect.h = (float)surface->h;
                dest_rect.x = centered ? (x - dest_rect.w / 2.0f) : x;
                dest_rect.y = y;
                SDL_RenderTexture(renderer, texture, NULL, &dest_rect);
                SDL_DestroyTexture(texture);
            }
            SDL_DestroySurface(surface);
        }
    }
    else
    {
        // Fallback SDL3 Debug Text
        float old_sx, old_sy;
        SDL_GetRenderScale(renderer, &old_sx, &old_sy);

        SDL_SetRenderScale(renderer, fallback_scale, fallback_scale);
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

        float text_w = strlen(text) * 8.0f * fallback_scale;
        float final_x = centered ? (x - text_w / 2.0f) : x;

        SDL_RenderDebugText(renderer, final_x / fallback_scale, y / fallback_scale, text);

        SDL_SetRenderScale(renderer, old_sx, old_sy);
    }
}

static void draw_text(const char *text, float x, float y, SDL_Color color)
{
    draw_text_internal(text, x, y, color, false, 2.0f);
}

static void draw_text_centered(const char *text, float center_x, float y, SDL_Color color)
{
    draw_text_internal(text, center_x, y, color, true, 3.0f);
}

// --- SYSTEME AUDIO FALLBACK ---
static void play_sound_fallback(const char *filename)
{
    char path[PATH_MAX];
    if (realpath(filename, path) == NULL)
        return;

    pid_t pid = fork();
    if (pid == 0)
    {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
#if defined(__APPLE__)
        execlp("afplay", "afplay", path, (char *)NULL);
#else
        if (strstr(path, ".mp3") || strstr(path, ".MP3"))
        {
            execlp("mpg123", "mpg123", "-q", path, (char *)NULL);
        }
        else
        {
            execlp("aplay", "aplay", "-q", path, (char *)NULL);
        }
#endif
        _exit(1);
    }
}

static void sdl_init(void)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        fprintf(stderr, "Erreur SDL_Init: %s\n", SDL_GetError());
        return;
    }

    if (!TTF_Init())
    {
        fprintf(stderr, "⚠️ Erreur TTF_Init: %s\n", SDL_GetError());
    }

#if HAVE_SDL_MIXER
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
    {
        printf("⚠️ Erreur SDL_Mixer Init: %s. Passage en mode audio dégradé.\n", Mix_GetError());
    }
    else
    {
        printf("🔊 Audio initialisé avec SDL_Mixer.\n");
    }
#endif

    window = SDL_CreateWindow("Star Launcher", GAME_WIDTH, GAME_HEIGHT, 0);
    renderer = SDL_CreateRenderer(window, NULL);

    tex_player = load_texture("vaisseau.png");
    tex_alien = load_texture("vaisseauEnnemie.png");
    tex_bullet = load_texture("bullets.png");
    tex_explosion = load_texture("explosion.png");
    tex_items = load_texture("items.png");
    tex_bouclier = load_texture("bouclier.png");
    tex_boss = load_texture("boss.png");

    init_stars();

    char font_path[PATH_MAX];
    if (realpath("assets/font.ttf", font_path))
        font = TTF_OpenFont(font_path, 24.0f);
    else if (realpath("../assets/font.ttf", font_path))
        font = TTF_OpenFont(font_path, 24.0f);

    if (!font)
        printf("⚠️ INFO: Police non trouvée. Mode texte de secours activé.\n");

#if HAVE_SDL_MIXER
    char path[PATH_MAX];
    if (realpath("assets/background.mp3", path))
    {
        bgm = Mix_LoadMUS(path);
        if (bgm)
            Mix_PlayMusic(bgm, -1);
    }
    if (realpath("assets/alarme6.WAV", path))
        sfx_item = Mix_LoadWAV(path);
    if (realpath("assets/tielaser.WAV", path))
        sfx_shoot = Mix_LoadWAV(path);
    if (realpath("assets/explosionaudio.WAV", path))
        sfx_explosion = Mix_LoadWAV(path);
#else
    if (bgm_pid == 0)
    {
        char path[PATH_MAX];
        if (realpath("assets/background.mp3", path))
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                freopen("/dev/null", "w", stdout);
                freopen("/dev/null", "w", stderr);
#if defined(__APPLE__)
                execlp("afplay", "afplay", path, (char *)NULL);
#else
                execlp("mpg123", "mpg123", "-q", "--loop", "-1", path, (char *)NULL);
#endif
                _exit(1);
            }
            else
            {
                bgm_pid = pid;
            }
        }
    }
#endif

    model_set_audio_callbacks(play_item_sound, play_explosion_sound, play_shoot_sound);
}

static void sdl_close(void)
{
    if (tex_player)
        SDL_DestroyTexture(tex_player);
    if (tex_alien)
        SDL_DestroyTexture(tex_alien);
    if (tex_boss)
        SDL_DestroyTexture(tex_boss);
    if (tex_bullet)
        SDL_DestroyTexture(tex_bullet);
    if (tex_explosion)
        SDL_DestroyTexture(tex_explosion);
    if (tex_items)
        SDL_DestroyTexture(tex_items);
    if (tex_bouclier)
        SDL_DestroyTexture(tex_bouclier);

    if (font)
        TTF_CloseFont(font);

#if HAVE_SDL_MIXER
    if (bgm)
        Mix_FreeMusic(bgm);
    if (sfx_item)
        Mix_FreeChunk(sfx_item);
    if (sfx_explosion)
        Mix_FreeChunk(sfx_explosion);
    if (sfx_shoot)
        Mix_FreeChunk(sfx_shoot);
    Mix_CloseAudio();
#else
    if (bgm_pid > 0)
    {
        kill(bgm_pid, SIGTERM);
        waitpid(bgm_pid, NULL, 0);
        bgm_pid = 0;
    }
#endif

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}

static KEY_BOUTONS sdl_get_input(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EVENT_QUIT)
            return BTN_QUIT;

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                return BTN_SELECT;
            }
        }
    }

    const bool *state = SDL_GetKeyboardState(NULL);
    if (state[SDL_SCANCODE_ESCAPE])
        return BTN_QUIT;
    if (state[SDL_SCANCODE_SPACE])
        return BTN_FIRE;
    if (state[SDL_SCANCODE_RETURN] || state[SDL_SCANCODE_KP_ENTER])
        return BTN_SELECT;
    if (state[SDL_SCANCODE_P])
        return BTN_PAUSE;
    if (state[SDL_SCANCODE_LEFT])
        return BTN_LEFT;
    if (state[SDL_SCANCODE_RIGHT])
        return BTN_RIGHT;
    if (state[SDL_SCANCODE_UP])
        return BTN_UP;
    if (state[SDL_SCANCODE_DOWN])
        return BTN_DOWN;
    return BTN_NONE;
}

static void sdl_render(const GameModel *model)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    update_and_draw_stars();

    SDL_FRect rect;

    // --- Rendu du Jeu ---

    // JOUEUR
    if (model->player.active)
    {
        rect = (SDL_FRect){model->player.x, model->player.y, (float)model->player.width, (float)model->player.height};

        if (model->player.shield)
        {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_FRect srect = {rect.x - SHIELD_PADDING, rect.y - SHIELD_PADDING, rect.w + SHIELD_PADDING * 2, rect.h + SHIELD_PADDING * 2};
            if (tex_bouclier)
                SDL_RenderTexture(renderer, tex_bouclier, NULL, &srect);
            else
            {
                SDL_SetRenderDrawColor(renderer, 0, 160, 255, 100);
                SDL_RenderFillRect(renderer, &srect);
            }
        }

        if (tex_player)
            SDL_RenderTexture(renderer, tex_player, NULL, &rect);
        else
        {
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    // BOSS
    if (model->boss.active)
    {
        rect = (SDL_FRect){model->boss.x, model->boss.y, (float)model->boss.width, (float)model->boss.height};
        if (tex_boss)
            SDL_RenderTexture(renderer, tex_boss, NULL, &rect);
        else
        {
            // Affichage de secours (Carré Magenta) si l'image bosss.png ne charge pas
            SDL_SetRenderDrawColor(renderer, 200, 0, 200, 255);
            SDL_RenderFillRect(renderer, &rect);
        }

        // Barre de vie du Boss
        SDL_FRect hp_bar_bg = {rect.x, rect.y - 15, rect.w, 10};
        SDL_SetRenderDrawColor(renderer, 50, 0, 0, 255);
        SDL_RenderFillRect(renderer, &hp_bar_bg);

        // PV max = 100 (fixe selon votre demande)
        float hp_percent = (float)model->boss.hp / 100.0f;
        SDL_FRect hp_bar_fg = {rect.x, rect.y - 15, rect.w * hp_percent, 10};
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderFillRect(renderer, &hp_bar_fg);
    }

    // ALIENS
    // Important : Ne les dessiner que s'ils sont actifs. (Normalement désactivés durant le boss)
    for (int i = 0; i < MAX_ALIENS; i++)
    {
        if (model->aliens[i].active)
        {
            rect = (SDL_FRect){model->aliens[i].x, model->aliens[i].y, (float)model->aliens[i].width, (float)model->aliens[i].height};
            if (tex_alien)
                SDL_RenderTexture(renderer, tex_alien, NULL, &rect);
            else
            {
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }

    // BALLES
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (model->bullets[i].active)
        {
            rect = (SDL_FRect){model->bullets[i].x, model->bullets[i].y, (float)model->bullets[i].width, (float)model->bullets[i].height};
            if (tex_bullet)
            {
                // On peut varier la couleur ou la texture selon le type, ici on utilise la même
                SDL_RenderTexture(renderer, tex_bullet, NULL, &rect);
            }
            else
            {
                if (model->bullets[i].type == ENTITY_BULLET_BOSS)
                    SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
                else if (model->bullets[i].type == ENTITY_BULLET_PLAYER)
                    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
                else
                    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }

    // ITEMS
    for (int i = 0; i < ITEMS_MAX; i++)
    {
        if (model->items[i].active)
        {
            rect = (SDL_FRect){model->items[i].x, model->items[i].y, (float)model->items[i].width, (float)model->items[i].height};
            if (tex_items)
                SDL_RenderTexture(renderer, tex_items, NULL, &rect);
            else
            {
                SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }

    // EXPLOSIONS
    for (int i = 0; i < EXPLOSION_MAX; i++)
    {
        if (model->explosions[i].active)
        {
            float cx = model->explosions[i].x + model->explosions[i].width / 2.0f;
            float cy = model->explosions[i].y + model->explosions[i].height / 2.0f;
            float dw = model->explosions[i].width * EXPLOSION_SCALE;
            float dh = model->explosions[i].height * EXPLOSION_SCALE;
            rect = (SDL_FRect){cx - dw / 2.0f, cy - dh / 2.0f, dw, dh};

            if (tex_explosion)
            {
                float p = 1.0f - (model->explosions[i].dx / EXPLOSION_DURATION);
                int idx = (int)(p * EXPLOSION_NB_FRAMES);
                if (idx >= EXPLOSION_NB_FRAMES)
                    idx = EXPLOSION_NB_FRAMES - 1;
                float tw, th;
                SDL_GetTextureSize(tex_explosion, &tw, &th);
                float fw = tw / EXPLOSION_NB_FRAMES;
                SDL_FRect src = {idx * fw, 0, fw, th};
                SDL_RenderTexture(renderer, tex_explosion, &src, &rect);
            }
            else
            {
                SDL_SetRenderDrawColor(renderer, 255, 128, 0, 255);
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }

    // --- HUD ---
    char buffer[64];
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color red = {255, 50, 50, 255};
    SDL_Color gold = {255, 215, 0, 255};

    snprintf(buffer, sizeof(buffer), "SCORE: %d", model->score);
    draw_text(buffer, 10, 5, white);

    snprintf(buffer, sizeof(buffer), "LIVES: %d", model->lives);
    draw_text(buffer, 20, 40, red);

    snprintf(buffer, sizeof(buffer), "LEVEL: %d", model->level);
    draw_text(buffer, GAME_WIDTH - 240, 5, gold);

    if (model->game_over)
    {
        draw_text_centered("GAME OVER", GAME_WIDTH / 2.0f, GAME_HEIGHT / 2.0f - 20, red);
        draw_text_centered("Press ENTER or CLICK to Restart", GAME_WIDTH / 2.0f, GAME_HEIGHT / 2.0f + 20, white);
        draw_text_centered("ESC to Quit", GAME_WIDTH / 2.0f, GAME_HEIGHT / 2.0f + 50, white);
    }

    // --- MENUS ---
    if (model->menu_mode != 0)
    {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
        SDL_FRect full = {0, 0, GAME_WIDTH, GAME_HEIGHT};
        SDL_RenderFillRect(renderer, &full);

        float cx = GAME_WIDTH / 2.0f;
        float cy = GAME_HEIGHT / 2.0f;

        if (model->menu_mode == 1) // MENU PRINCIPAL
        {
            float title_y = cy - 80.0f;
            float items_base_y = cy - 10.0f;
            float items_spacing = 50.0f;

            draw_text_centered("STAR LAUNCHER", cx, title_y, gold);

            const char *items[] = {"Start Game", "Parametre", "High Scores", "Quit"};
            for (int i = 0; i < 4; ++i)
            {
                SDL_Color col = (i == model->menu_selection) ? (SDL_Color){0, 255, 255, 255} : (SDL_Color){150, 150, 150, 255};
                if (i == model->menu_selection)
                {
                    char sel_buf[64];
                    snprintf(sel_buf, sizeof(sel_buf), "> %s <", items[i]);
                    draw_text_centered(sel_buf, cx, items_base_y + i * items_spacing, col);
                }
                else
                {
                    draw_text_centered(items[i], cx, items_base_y + i * items_spacing, col);
                }
            }
            draw_text_centered("ENTER or CLICK to select", cx, items_base_y + 4 * items_spacing + 15, white);
        }
        else if (model->menu_mode == 2) // SETTINGS
        {
            draw_text_centered("PARAMETRE", cx, cy - 50, white);
            draw_text_centered("Music: ON", cx, cy, white);
            draw_text_centered("(Press ESC to return)", cx, cy + 50, (SDL_Color){100, 100, 100, 255});
        }
        else if (model->menu_mode == 3) // HIGH SCORES
        {
            draw_text_centered("--- HALL OF FAME ---", cx, cy - 100, red);

            char buf[64];
            snprintf(buf, sizeof(buf), "%06d", model->high_score);

            draw_text_centered("TOP SCORE", cx, cy - 30, white);
            draw_text_centered(buf, cx, cy + 10, gold);

            draw_text_centered("(Press ESC to return)", cx, cy + 100, (SDL_Color){100, 100, 100, 255});
        }
        else if (model->menu_mode == 4) // PAUSE
        {
            float title_y = cy - 80.0f;
            float items_base_y = cy - 10.0f;
            float items_spacing = 50.0f;

            draw_text_centered("PAUSE MENU", cx, title_y, gold);

            const char *items[] = {"Resume Game", "Parametre", "High Scores", "Quit"};
            for (int i = 0; i < 4; ++i)
            {
                SDL_Color col = (i == model->menu_selection) ? (SDL_Color){0, 255, 255, 255} : (SDL_Color){150, 150, 150, 255};
                if (i == model->menu_selection)
                {
                    char sel_buf[64];
                    snprintf(sel_buf, sizeof(sel_buf), "> %s <", items[i]);
                    draw_text_centered(sel_buf, cx, items_base_y + i * items_spacing, col);
                }
                else
                {
                    draw_text_centered(items[i], cx, items_base_y + i * items_spacing, col);
                }
            }
            draw_text_centered("ENTER or CLICK to select", cx, items_base_y + 4 * items_spacing + 15, white);
        }
    }

    SDL_RenderPresent(renderer);
}

GameView view_sdl_get_interface(void)
{
    GameView v;
    v.init = sdl_init;
    v.close = sdl_close;
    v.render = sdl_render;
    v.get_input = sdl_get_input;
    return v;
}

void play_item_sound(void)
{
#if HAVE_SDL_MIXER
    if (sfx_item)
        Mix_PlayChannel(-1, sfx_item, 0);
#else
    play_sound_fallback("assets/alarme6.WAV");
#endif
}
void play_explosion_sound(void)
{
#if HAVE_SDL_MIXER
    if (sfx_explosion)
        Mix_PlayChannel(-1, sfx_explosion, 0);
#else
    play_sound_fallback("assets/explosionaudio.WAV");
#endif
}
void play_shoot_sound(void)
{
#if HAVE_SDL_MIXER
    if (sfx_shoot)
        Mix_PlayChannel(-1, sfx_shoot, 0);
#else
    play_sound_fallback("assets/tielaser.WAV");
#endif
}