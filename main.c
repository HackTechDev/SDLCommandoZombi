#include <SDL2/SDL.h>
#include <stdbool.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 576
#define TILE_SIZE 32
#define MAP_WIDTH (SCREEN_WIDTH / TILE_SIZE)
#define MAP_HEIGHT (SCREEN_HEIGHT / TILE_SIZE)
#define PLAYER_SPEED 4

typedef struct {
    int x, y;
    SDL_Rect rect;
} Player;

#define MAX_ENTITIES 64

typedef struct {
    int x, y;
} Enemy;

typedef struct {
    int x, y;
    bool collected;
} Key;

typedef struct {
    int x, y;
    bool open;
} Door;

Enemy enemies[MAX_ENTITIES];
int enemyCount = 0;

Key keys[MAX_ENTITIES];
int keyCount = 0;

Door doors[MAX_ENTITIES];
int doorCount = 0;


// 0 = sol, 1 = mur
int map[MAP_HEIGHT][MAP_WIDTH];

int playerStartX = -1;
int playerStartY = -1;

bool loadMap(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        SDL_Log("Impossible d'ouvrir %s", filename);
        return false;
    }

    char line[MAP_WIDTH + 2]; // +2 pour '\n' et '\0'

    for (int y = 0; y < MAP_HEIGHT; y++) {
        if (!fgets(line, sizeof(line), file)) {
            SDL_Log("Erreur de lecture ligne %d (fichier trop court ?)", y + 1);
            fclose(file);
            return false;
        }

        if (strlen(line) < MAP_WIDTH) {
            SDL_Log("Ligne %d trop courte : %zu caractères", y + 1, strlen(line));
            fclose(file);
            return false;
        }

        for (int x = 0; x < MAP_WIDTH; x++) {
            switch (line[x]) {
                case '0':
                    map[y][x] = 0;
                    break;
                case '1':
                    map[y][x] = 1;
                    break;
                case 'P':
                    map[y][x] = 0;
                    playerStartX = x;
                    playerStartY = y;
                    break;
                case 'E':
                    map[y][x] = 0;
                    if (enemyCount < MAX_ENTITIES) {
                        enemies[enemyCount++] = (Enemy){ x * TILE_SIZE, y * TILE_SIZE };
                    }
                    break;
                case 'K':
                    map[y][x] = 0;
                    if (keyCount < MAX_ENTITIES) {
                        keys[keyCount++] = (Key){ x * TILE_SIZE, y * TILE_SIZE, false };
                    }
                    break;
                case 'D':
                    map[y][x] = 0;
                    if (doorCount < MAX_ENTITIES) {
                        doors[doorCount++] = (Door){ x * TILE_SIZE, y * TILE_SIZE, false };
                    }
                    break;
                default:
                    SDL_Log("Caractère inconnu '%c' à (%d, %d)", line[x], y, x);
                    map[y][x] = 0;
                    break;
            }
        }
    }
    fclose(file);
    return true;
}

bool isBlockedAt(int x, int y) {
    int tileX = x / TILE_SIZE;
    int tileY = y / TILE_SIZE;

    if (tileX < 0 || tileX >= MAP_WIDTH || tileY < 0 || tileY >= MAP_HEIGHT)
        return true;

    return map[tileY][tileX] == 1;
}

bool isCollision(int x, int y, int size) {
    return isBlockedAt(x, y) ||
           isBlockedAt(x + size - 1, y) ||
           isBlockedAt(x, y + size - 1) ||
           isBlockedAt(x + size - 1, y + size - 1);
}

void renderMap(SDL_Renderer* renderer) {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            SDL_Rect tileRect = {x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE};
            if (map[y][x] == 1)
                SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255); // mur
            else
                SDL_SetRenderDrawColor(renderer, 20, 150, 20, 255); // sol
            SDL_RenderFillRect(renderer, &tileRect);
        }
    }

    // Ennemis : rouge
    for (int i = 0; i < enemyCount; i++) {
        SDL_Rect r = { enemies[i].x, enemies[i].y, TILE_SIZE, TILE_SIZE };
        SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255);
        SDL_RenderFillRect(renderer, &r);
    }

    // Clés : jaune
    for (int i = 0; i < keyCount; i++) {
        if (!keys[i].collected) {
            SDL_Rect r = { keys[i].x, keys[i].y, TILE_SIZE, TILE_SIZE };
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            SDL_RenderFillRect(renderer, &r);
        }
    }

    // Portes : bleu foncé
    for (int i = 0; i < doorCount; i++) {
        if (!doors[i].open) {
            SDL_Rect r = { doors[i].x, doors[i].y, TILE_SIZE, TILE_SIZE };
            SDL_SetRenderDrawColor(renderer, 0, 0, 200, 255);
            SDL_RenderFillRect(renderer, &r);
        }
    }
}


void handleInput(bool* running, const Uint8* keystate, Player* player) {
    if (keystate[SDL_SCANCODE_ESCAPE])
        *running = false;

    int newX = player->x;
    int newY = player->y;

    if (keystate[SDL_SCANCODE_UP])    newY -= PLAYER_SPEED;
    if (keystate[SDL_SCANCODE_DOWN])  newY += PLAYER_SPEED;
    if (keystate[SDL_SCANCODE_LEFT])  newX -= PLAYER_SPEED;
    if (keystate[SDL_SCANCODE_RIGHT]) newX += PLAYER_SPEED;

    if (!isCollision(newX, player->y, TILE_SIZE))
        player->x = newX;

    if (!isCollision(player->x, newY, TILE_SIZE))
        player->y = newY;
}

void renderPlayer(SDL_Renderer* renderer, Player* player) {
    player->rect.x = player->x;
    player->rect.y = player->y;
    player->rect.w = TILE_SIZE;
    player->rect.h = TILE_SIZE;

    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255); // joueur jaune
    SDL_RenderFillRect(renderer, &player->rect);
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("Zelda-Like", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (!loadMap("map.txt")) {
        SDL_Quit();
        return 1;
    }

    Player player;
    if (playerStartX == -1 || playerStartY == -1) {
        SDL_Log("Position du joueur non définie dans la carte !");
        SDL_Quit();
        return 1;
    }
    player.x = playerStartX * TILE_SIZE;
    player.y = playerStartY * TILE_SIZE;

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event))
            if (event.type == SDL_QUIT) running = false;

        const Uint8* keystate = SDL_GetKeyboardState(NULL);
        handleInput(&running, keystate, &player);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        renderMap(renderer);
        renderPlayer(renderer, &player);

        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60 FPS
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

