#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL3/SDL.h> // Nécessaire pour le launcher SDL3
#include "view.h"
#include "controller.h"

// ==========================================
// --- GESTION DU HIGHSCORE (JSON) ---
// ==========================================

#define HIGHSCORE_FILE "highscore.json"

// Sauvegarde le score dans un fichier au format JSON
void save_high_score(int score)
{
    FILE *f = fopen(HIGHSCORE_FILE, "w");
    if (f)
    {
        fprintf(f, "{\n  \"high_score\": %d\n}\n", score);
        fclose(f);
        printf("💾 High score sauvegardé : %d\n", score);
    }
    else
    {
        fprintf(stderr, "⚠️ Erreur : Impossible de sauvegarder le high score.\n");
    }
}

// Charge le score depuis le fichier JSON
int load_high_score()
{
    FILE *f = fopen(HIGHSCORE_FILE, "r");
    if (!f)
        return 0; // Si le fichier n'existe pas, score = 0

    int score = 0;
    char line[256];
    // Lecture ligne par ligne pour trouver "high_score"
    while (fgets(line, sizeof(line), f))
    {
        if (strstr(line, "\"high_score\""))
        {
            // On cherche le séparateur ':'
            char *colon = strchr(line, ':');
            if (colon)
            {
                score = atoi(colon + 1); // Conversion du texte en entier
                break;
            }
        }
    }
    fclose(f);
    printf("📂 High score chargé : %d\n", score);
    return score;
}

// ==========================================
// --- STARTUP LAUNCHER (Menu de choix) ---
// ==========================================

#define LAUNCHER_WIDTH 800
#define LAUNCHER_HEIGHT 600
#define NUM_STARS 150

typedef enum
{
    STARTUP_CHOICE_SDL,
    STARTUP_CHOICE_NCURSES,
    STARTUP_CHOICE_EXIT
} StartupResult;

// Structure pour le fond étoilé
typedef struct
{
    float x, y;
    float speed;
    uint8_t brightness;
} Star;

typedef struct
{
    SDL_FRect rect;
    SDL_Color outline_color; // Couleur de la bordure néon
    const char *label;
    bool is_hovered;
} LauncherButton;

// --- FONCTION UTILITAIRE POUR LE TEXTE (SDL3 DEBUG TEXT) ---
// Permet d'afficher du texte sans charger de police, en le grossissant
void draw_scaled_text(SDL_Renderer *renderer, float x, float y, const char *text, float scale, SDL_Color color)
{
    // 1. Sauvegarder l'échelle actuelle
    float old_sx, old_sy;
    SDL_GetRenderScale(renderer, &old_sx, &old_sy);

    // 2. Définir la nouvelle échelle et la couleur
    SDL_SetRenderScale(renderer, scale, scale);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    // 3. Dessiner le texte (On divise x/y par le scale car SDL applique le scale aux coordonnées)
    SDL_RenderDebugText(renderer, x / scale, y / scale, text);

    // 4. Restaurer l'échelle
    SDL_SetRenderScale(renderer, old_sx, old_sy);
}

// Fonction utilitaire pour dessiner un bouton rétro avec TEXTE
void draw_retro_button(SDL_Renderer *renderer, LauncherButton *btn)
{
    // 1. Remplissage sombre (arrière-plan du bouton)
    SDL_SetRenderDrawColor(renderer, btn->outline_color.r / 5, btn->outline_color.g / 5, btn->outline_color.b / 5, 220);
    SDL_RenderFillRect(renderer, &btn->rect);

    // 2. Bordure épaisse "Néon"
    if (btn->is_hovered)
    {
        SDL_SetRenderDrawColor(renderer, btn->outline_color.r, btn->outline_color.g, btn->outline_color.b, 255);
    }
    else
    {
        SDL_SetRenderDrawColor(renderer, btn->outline_color.r / 2, btn->outline_color.g / 2, btn->outline_color.b / 2, 255);
    }

    // Dessiner plusieurs rectangles non remplis pour faire une bordure épaisse
    SDL_FRect outline = btn->rect;
    for (int i = 0; i < 3; i++)
    {
        SDL_RenderRect(renderer, &outline);
        outline.x += 1.0f;
        outline.y += 1.0f;
        outline.w -= 2.0f;
        outline.h -= 2.0f;
    }

    // 3. DESSINER LE TEXTE DU BOUTON
    // On estime la taille du texte (font width approx 8px * scale)
    float text_scale = 3.0f;
    float char_width = 8.0f * text_scale;
    float text_width = strlen(btn->label) * char_width;
    float text_height = 8.0f * text_scale;

    // Calculer la position centrée
    float text_x = btn->rect.x + (btn->rect.w - text_width) / 2.0f;
    float text_y = btn->rect.y + (btn->rect.h - text_height) / 2.0f;

    // Couleur du texte (Blanc si survolé, Gris clair sinon)
    SDL_Color text_col = btn->is_hovered ? (SDL_Color){255, 255, 255, 255} : (SDL_Color){180, 180, 180, 255};

    draw_scaled_text(renderer, text_x, text_y, btn->label, text_scale, text_col);
}

// Fonction qui affiche le sélecteur de mode au démarrage
StartupResult run_startup_menu(void)
{
    // Initialisation locale pour le launcher
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        fprintf(stderr, "Erreur SDL Init (Launcher): %s\n", SDL_GetError());
        return STARTUP_CHOICE_EXIT;
    }

    SDL_Window *window = SDL_CreateWindow("Space Game Launcher", LAUNCHER_WIDTH, LAUNCHER_HEIGHT, 0);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, SDL_SOFTWARE_RENDERER);
    if (!renderer)
        renderer = SDL_CreateRenderer(window, NULL);

    // --- Initialisation des étoiles ---
    Star stars[NUM_STARS];
    for (int i = 0; i < NUM_STARS; i++)
    {
        stars[i].x = (float)(rand() % LAUNCHER_WIDTH);
        stars[i].y = (float)(rand() % LAUNCHER_HEIGHT);
        stars[i].speed = 0.5f + ((float)(rand() % 10) / 10.0f) * 2.0f;
        stars[i].brightness = 100 + (rand() % 155);
    }

    // --- Définition des boutons (Labels simplifiés pour le rendu pixel) ---
    // SDL : Cyan Néon
    LauncherButton btn_sdl = {{200.0f, 250.0f, 400.0f, 80.0f}, {0, 255, 255, 255}, "MODE SDL", false};
    // Ncurses : Orange/Ambre Néon
    LauncherButton btn_ncurses = {{200.0f, 380.0f, 400.0f, 80.0f}, {255, 165, 0, 255}, "MODE NCURSES", false};

    StartupResult result = STARTUP_CHOICE_EXIT;
    bool running = true;
    SDL_Event event;
    float mouse_x = 0, mouse_y = 0;

    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
                result = STARTUP_CHOICE_EXIT;
            }
            else if (event.type == SDL_EVENT_KEY_DOWN)
            {
                if (event.key.key == SDLK_ESCAPE)
                {
                    running = false;
                    result = STARTUP_CHOICE_EXIT;
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_MOTION)
            {
                mouse_x = event.motion.x;
                mouse_y = event.motion.y;
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            {
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    if (btn_sdl.is_hovered)
                    {
                        result = STARTUP_CHOICE_SDL;
                        running = false;
                    }
                    else if (btn_ncurses.is_hovered)
                    {
                        result = STARTUP_CHOICE_NCURSES;
                        running = false;
                    }
                }
            }
        }

        // --- Mise à jour ---

        // 1. Etoiles
        for (int i = 0; i < NUM_STARS; i++)
        {
            stars[i].y += stars[i].speed;
            if (stars[i].y > LAUNCHER_HEIGHT)
            {
                stars[i].y = 0;
                stars[i].x = (float)(rand() % LAUNCHER_WIDTH);
            }
        }

        // 2. Hover
        btn_sdl.is_hovered = (mouse_x >= btn_sdl.rect.x && mouse_x <= btn_sdl.rect.x + btn_sdl.rect.w &&
                              mouse_y >= btn_sdl.rect.y && mouse_y <= btn_sdl.rect.y + btn_sdl.rect.h);

        btn_ncurses.is_hovered = (mouse_x >= btn_ncurses.rect.x && mouse_x <= btn_ncurses.rect.x + btn_ncurses.rect.w &&
                                  mouse_y >= btn_ncurses.rect.y && mouse_y <= btn_ncurses.rect.y + btn_ncurses.rect.h);

        // --- Rendu ---
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Fond Noir
        SDL_RenderClear(renderer);

        // 1. Dessiner les étoiles
        for (int i = 0; i < NUM_STARS; i++)
        {
            SDL_SetRenderDrawColor(renderer, stars[i].brightness, stars[i].brightness, stars[i].brightness, 255);
            SDL_RenderPoint(renderer, stars[i].x, stars[i].y);
        }

        // 2. TITRE DU JEU (Style Star Wars)
        const char *title = "STAR LAUNCHER";
        float title_scale = 6.0f;
        float title_w = strlen(title) * 8.0f * title_scale;
        float title_x = (LAUNCHER_WIDTH - title_w) / 2.0f;
        // Effet d'ombre rouge pour le titre
        draw_scaled_text(renderer, title_x + 4, 54.0f, title, title_scale, (SDL_Color){100, 0, 0, 255});
        // Titre Jaune
        draw_scaled_text(renderer, title_x, 50.0f, title, title_scale, (SDL_Color){255, 230, 0, 255});

        // SOUS-TITRE
        const char *subtitle = "- SELECT MISSION MODE -";
        float sub_scale = 2.0f;
        float sub_w = strlen(subtitle) * 8.0f * sub_scale;
        float sub_x = (LAUNCHER_WIDTH - sub_w) / 2.0f;
        draw_scaled_text(renderer, sub_x, 130.0f, subtitle, sub_scale, (SDL_Color){0, 200, 255, 255});

        // 3. Dessiner les boutons
        draw_retro_button(renderer, &btn_sdl);
        draw_retro_button(renderer, &btn_ncurses);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return result;
}

// ==========================================
// --- MAIN ---
// ==========================================

int main(int argc, char *argv[])
{
    // Initialisation du générateur de nombres aléatoires
    srand(time(NULL));

    // A. Vérification des arguments (Mode CLI forcé ?)
    int cli_forced_mode = -1; // -1: Pas d'argument, 0: Ncurses, 1: SDL
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "sdl") == 0 || strcmp(argv[i], "--mode=sdl") == 0 || (strcmp(argv[i], "--mode") == 0 && i + 1 < argc && strcmp(argv[i + 1], "sdl") == 0))
        {
            cli_forced_mode = 1;
            break;
        }
        else if (strcmp(argv[i], "ncurses") == 0 || strcmp(argv[i], "--mode=ncurses") == 0)
        {
            cli_forced_mode = 0;
            break;
        }
    }

    bool app_running = true;

    // CHARGEMENT DU HIGH SCORE DEPUIS LE JSON
    int saved_high_score = load_high_score();

    // --- BOUCLE GLOBALE DE L'APPLICATION (Launcher -> Game -> Launcher) ---
    while (app_running)
    {

        int current_mode = cli_forced_mode;

        // B. Si aucun argument n'est donné, afficher le MENU DE DÉMARRAGE
        if (current_mode == -1)
        {
            StartupResult res = run_startup_menu();

            if (res == STARTUP_CHOICE_EXIT)
            {
                printf("Fermeture du launcher.\n");
                app_running = false;
                break; // Quitte l'application
            }
            else if (res == STARTUP_CHOICE_SDL)
            {
                current_mode = 1;
            }
            else
            {
                current_mode = 0;
            }
        }

        // C. Initialisation du Jeu (Modèle & Vue) pour cette session
        GameModel game;
        game.high_score = saved_high_score; // Restaure le high score dans le jeu
        model_init(&game);

        // Sécurité : Si model_init réinitialise à 0 alors qu'on avait une save, on restaure
        if (game.high_score == 0 && saved_high_score > 0)
            game.high_score = saved_high_score;

        GameView view;
        int is_sdl = (current_mode == 1);

        if (is_sdl)
        {
            view = view_sdl_get_interface();
        }
        else
        {
            view = view_ncurses_get_interface();
        }

        printf(is_sdl ? "🎮 Mode GRAPHIQUE activé (SDL3)\n" : "💻 Mode TEXTE activé (Ncurses)\n");

        // Démarrage de l'affichage
        view.init();

        // Variable pour contrôler si on est toujours dans la SESSION DE JEU
        bool session_running = true;

        // --- PHASE 1 : MENU DU JEU (Start / HighScore / Quit) ---
        game.menu_mode = 1;
        while (session_running && game.menu_mode == 1)
        {
            view.render(&game);

            KEY_BOUTONS m = view.get_input();
            if (m == BTN_QUIT)
            {
                session_running = false; // Retour Launcher
            }
            else if (m == BTN_UP)
            {
                if (game.menu_selection > 0)
                    game.menu_selection--;
            }
            else if (m == BTN_DOWN)
            {
                game.menu_selection++;
                if (game.menu_selection > 3)
                    game.menu_selection = 3;
            }
            else if (m == BTN_SELECT)
            {
                switch (game.menu_selection)
                {
                case 0:                 // Start
                    game.menu_mode = 0; // Sort de la boucle menu, entre en jeu
                    break;
                case 1: // Settings
                    game.menu_mode = 2;
                    break;
                case 2: // High Scores
                    game.menu_mode = 3;
                    break;
                case 3:                      // Quit
                    session_running = false; // Retour Launcher
                    break;
                }
            }

            // Gestion des sous-menus (Settings & High Scores)
            while (game.menu_mode == 2 || game.menu_mode == 3)
            {
                view.render(&game);
                KEY_BOUTONS sub = view.get_input();

                // IMPORTANT: Utiliser ESC (BTN_QUIT) pour revenir, pas ENTER
                if (sub == BTN_QUIT || sub == BTN_FIRE)
                {
                    game.menu_mode = 1; // Retour au menu principal
                }
                struct timespec ts_sub = {0, 100000000};
                nanosleep(&ts_sub, NULL);
            }

            struct timespec ts = {0, 100000000};
            nanosleep(&ts, NULL);
        }

        // --- PHASE 2 : BOUCLE DE JEU (Gameplay) ---
        // On n'entre ici que si on a quitté le menu par "Start" (menu_mode == 0)
        int game_loop_running = (session_running && game.menu_mode == 0) ? 1 : 0;

        // Gestion du temps
        const int64_t FRAME_NANOS = 16666667LL; // ~60 FPS
        struct timespec t_last, t_now, t_after;
        clock_gettime(CLOCK_MONOTONIC, &t_last);
        float fire_timer = 0.0f;

        while (game_loop_running)
        {
            // Delta Time
            clock_gettime(CLOCK_MONOTONIC, &t_now);
            int64_t delta_ns = (t_now.tv_sec - t_last.tv_sec) * 1000000000LL + (t_now.tv_nsec - t_last.tv_nsec);
            float delta_s = (float)delta_ns / 1e9f;
            if (delta_s <= 0.0f)
                delta_s = 0.001f;
            if (delta_s > 0.1f)
                delta_s = 0.1f;

            if (fire_timer > 0.0f)
                fire_timer -= delta_s;

            // Inputs
            KEY_BOUTONS input = view.get_input();

            switch (input)
            {
            case BTN_PAUSE:
                game.menu_mode = 4;
                game.menu_selection = 0; // Reset sélection au premier élément "Resume"

                while (game.menu_mode == 4)
                {
                    view.render(&game);
                    KEY_BOUTONS pk = view.get_input();

                    if (pk == BTN_UP)
                    {
                        if (game.menu_selection > 0)
                            game.menu_selection--;
                    }
                    else if (pk == BTN_DOWN)
                    {
                        if (game.menu_selection < 3)
                            game.menu_selection++;
                    }
                    else if (pk == BTN_SELECT)
                    {
                        switch (game.menu_selection)
                        {
                        case 0:                 // Resume
                            game.menu_mode = 0; // Reprendre le jeu
                            break;
                        case 1: // Settings (Sous-menu)
                            game.menu_mode = 2;
                            break;
                        case 2: // High Scores (Sous-menu)
                            game.menu_mode = 3;
                            break;
                        case 3: // Quit
                            game_loop_running = 0;
                            game.menu_mode = 0; // Sortir de la boucle pause
                            break;
                        }
                    }
                    else if (pk == BTN_QUIT)
                    {
                        // ESC en pause -> Reprendre le jeu
                        game.menu_mode = 0;
                    }

                    // Gestion des sous-menus PENDANT LA PAUSE (Settings / HighScores)
                    while (game.menu_mode == 2 || game.menu_mode == 3)
                    {
                        view.render(&game);
                        KEY_BOUTONS sub = view.get_input();
                        // Retour au menu pause avec ESC
                        if (sub == BTN_QUIT || sub == BTN_FIRE)
                        {
                            game.menu_mode = 4;
                        }
                        struct timespec ts_sub = {0, 100000000};
                        nanosleep(&ts_sub, NULL);
                    }

                    // Si on a choisi Quit, on sort de la boucle while(menu_mode==4)
                    if (!game_loop_running)
                        break;

                    struct timespec ts2 = {0, 100000000};
                    nanosleep(&ts2, NULL);
                }

                if (!game_loop_running)
                    break;
                // Si on sort du menu pause (resume), on réinitialise l'horloge pour éviter un saut temporel
                clock_gettime(CLOCK_MONOTONIC, &t_last);
                continue;

            case BTN_QUIT:
                game_loop_running = 0; // Retour Launcher
                break;

            case BTN_LEFT:
                model_move_player(&game, -1, 0);
                break;
            case BTN_RIGHT:
                model_move_player(&game, 1, 0);
                break;
            case BTN_DOWN:
                model_move_player(&game, 0, 1);
                break;
            case BTN_UP:
                model_move_player(&game, 0, -1);
                break;
            case BTN_FIRE:
                model_move_player(&game, 0, 0);
                if (fire_timer <= 0.0f)
                {
                    float x = game.player.x + (game.player.width / 2);
                    float y = game.player.y;
                    model_fire_bullet(&game, x, y, ENTITY_BULLET_PLAYER);
                    fire_timer = 0.01f;
                }
                break;
            default:
                model_move_player(&game, 0, 0);
                break;
            }

            if (!game_loop_running)
                break;

            // Update
            model_update(&game, delta_s);

            // Render
            view.render(&game);

            // Game Over Loop
            if (game.game_over)
            {
                while (game_loop_running)
                {
                    view.render(&game);
                    KEY_BOUTONS post = view.get_input();
                    if (post == BTN_QUIT)
                    {
                        game_loop_running = 0; // Retour Launcher
                        break;
                    }
                    if (post == BTN_SELECT)
                    {
                        // Restart
                        model_init(&game);
                        game.high_score = saved_high_score; // Garder le score
                        fire_timer = 0.0f;
                        clock_gettime(CLOCK_MONOTONIC, &t_last);
                        break;
                    }
                    struct timespec ts = {0, 100000000};
                    nanosleep(&ts, NULL);
                }
                if (!game_loop_running)
                    break;
                continue;
            }

            // FPS Cap
            clock_gettime(CLOCK_MONOTONIC, &t_after);
            int64_t frame_elapsed = (t_after.tv_sec - t_now.tv_sec) * 1000000000000LL + (t_after.tv_nsec - t_now.tv_nsec);
            int64_t sleep_ns = FRAME_NANOS - frame_elapsed;
            if (sleep_ns > 0)
            {
                struct timespec ts_sleep;
                ts_sleep.tv_sec = sleep_ns / 1000000000000LL;
                ts_sleep.tv_nsec = sleep_ns % 1000000000000LL;
                nanosleep(&ts_sleep, NULL);
            }
            clock_gettime(CLOCK_MONOTONIC, &t_last);
        }

        // Fin de la session de jeu
        view.close();

        // SAUVEGARDE DU HIGH SCORE SI BATTU
        if (game.score > saved_high_score)
        {
            saved_high_score = game.score;
            save_high_score(saved_high_score); // Sauvegarde immédiate dans le JSON
        }

        // Si on était en mode CLI forcé (arguments), on quitte l'app au lieu de relancer le launcher
        if (cli_forced_mode != -1)
        {
            app_running = false;
        }
    }

    printf("👋 Fin de l'application.\n");
    return 0;
}