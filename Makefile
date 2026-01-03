# ==========================================
#  Makefile Universel - Space Invaders
#  Compatible : Linux, macOS (Homebrew), WSL
# ==========================================

BIN = space-invaders
CC = gcc

# --- 1. Configuration de base (Universelle) ---
# Options communes à tous les systèmes
CFLAGS  = -Wall -Wextra -std=c99 -g -Iinclude
LDFLAGS = -lncurses -lm

# Liste des paquets nécessaires via pkg-config
# Ne pas forcer sdl3-mixer ici : on détectera le mixer séparément si disponible
PKGS = sdl3 sdl3-image sdl3-ttf

# --- 2. Détection Automatique des Librairies ---

# On tente de récupérer les infos via pkg-config (Standard Linux/Pro)
# 2>/dev/null cache les erreurs si pkg-config n'est pas installé ou ne trouve rien
SDL_CFLAGS := $(shell pkg-config --cflags $(PKGS) 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs $(PKGS) 2>/dev/null)

# --- 3. Logique de Décision (Le Cerveau du Makefile) ---

ifneq ($(strip $(SDL_CFLAGS)),)
    # CAS 1 : pkg-config a trouvé les librairies (Cas idéal Linux/PC configuré)
    # On ajoute simplement les flags trouvés
    CFLAGS  += $(SDL_CFLAGS)
    LDFLAGS += $(SDL_LIBS)
else
    # CAS 2 : pkg-config a échoué (Cas probable sur ton Mac actuel)
    # On active le mode "Secours" avec les chemins en dur
    
    # Détection de l'OS pour savoir quels chemins ajouter
    UNAME_S := $(shell uname -s)
    
    ifeq ($(UNAME_S),Darwin)
        # Chemins spécifiques macOS (Homebrew & Local)
        CFLAGS  += -I/opt/homebrew/include -I/usr/local/include
        LDFLAGS += -L/opt/homebrew/lib -L/usr/local/lib
    endif
    
    # On ajoute les librairies SDL3 de base (ne pas forcer SDL3_mixer ici)
    LDFLAGS += -lSDL3 -lSDL3_image -lSDL3_ttf
endif

# --- 4. Gestion des Fichiers ---
SRC_DIR = src
OBJ_DIR = build

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# --- 5. Cibles de Compilation ---

all: directories $(BIN)

$(BIN): $(OBJS)
	@echo "🔗 Édition des liens..."
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "✅ Compilation terminée avec succès !"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "🔨 Compilation de $<..."
	$(CC) $(CFLAGS) -c $< -o $@

directories:
	@mkdir -p $(OBJ_DIR)

clean:
	@echo "🧹 Nettoyage..."
	rm -rf $(OBJ_DIR) $(BIN)

# --- 6. Commandes de lancement ---

run-sdl: all
	@echo "🚀 Lancement SDL..."
	./$(BIN) --mode sdl

run-ncurses: all
	@echo "🚀 Lancement Ncurses..."
	./$(BIN) --mode ncurses

.PHONY: all clean run-sdl run-ncurses directories