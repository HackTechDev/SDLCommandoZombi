#include <SDL2/SDL.h>
#include <stdbool.h>
#include <SDL2/SDL_image.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 576
#define TILE_SIZE 32
#define MAP_WIDTH (SCREEN_WIDTH / TILE_SIZE)
#define MAP_HEIGHT (SCREEN_HEIGHT / TILE_SIZE)
#define PLAYER_SPEED 4

#define FRAME_WIDTH 32
#define FRAME_HEIGHT 32
#define FRAME_COUNT 9
#define ANIM_SPEED 8  

typedef enum {
    DIR_UP = 0,
    DIR_LEFT = 1,
    DIR_DOWN = 2,
    DIR_RIGHT = 3,
} Direction;

typedef struct {
    int x, y;
    int frame;            // 0 → 8
    int frameTimer;
    Direction dir;        // Direction actuelle
    SDL_Texture* texture; // Sprite sheet

} Player;

#define MAX_ENTITIES 64

typedef struct {
    int x, y;
} Enemy;

typedef struct {
    int x, y;
    bool collected;
    int doorIndex; 
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

int keysCollected = 0;

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
                        keys[keyCount] = (Key){ x * TILE_SIZE, y * TILE_SIZE, false, keyCount };
                        keyCount++;
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

bool checkCollision(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2) {
    return !(x1 + w1 <= x2 || x1 >= x2 + w2 || y1 + h1 <= y2 || y1 >= y2 + h2);
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
            SDL_SetRenderDrawColor(renderer, 128, 128, 0, 255);
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


void movePlayer(Player* player, int dx, int dy) {
    int newX = player->x + dx;
    int newY = player->y + dy;

    for (int i = 0; i < doorCount; i++) {
        if (!doors[i].open &&
            checkCollision(newX, newY, TILE_SIZE, TILE_SIZE,
                           doors[i].x, doors[i].y, TILE_SIZE, TILE_SIZE)) {
            // Collision avec une porte FERMÉE
            SDL_Log("Bloqué par une porte fermée !");
            return; // Ne pas bouger
        }
    }



    // Détection de direction
    if (dy < 0) player->dir = DIR_UP;
    else if (dy > 0) player->dir = DIR_DOWN;
    else if (dx < 0) player->dir = DIR_LEFT;
    else if (dx > 0) player->dir = DIR_RIGHT;


    // Déplacement horizontal
    if (!isCollision(newX, player->y, TILE_SIZE)) {
        player->x = newX;
    }

    // Déplacement vertical
    if (!isCollision(player->x, newY, TILE_SIZE)) {
        player->y = newY;
    }

      // Animation
      if (dx != 0 || dy != 0) {
        player->frameTimer++;
        if (player->frameTimer >= ANIM_SPEED) {
            player->frame = (player->frame + 1) % FRAME_COUNT;
            player->frameTimer = 0;
        }
    } else {
        player->frame = 0; // frame fixe si inactif
    }
}

void handleKeyPress(SDL_KeyboardEvent* key, Player* player, bool* running) {
    int dx = 0, dy = 0;

    switch (key->keysym.scancode) {
        case SDL_SCANCODE_ESCAPE:
            *running = false;
            break;
        case SDL_SCANCODE_UP:
            dy = -PLAYER_SPEED;
            break;
        case SDL_SCANCODE_DOWN:
            dy = PLAYER_SPEED;
            break;
        case SDL_SCANCODE_LEFT:
            dx = -PLAYER_SPEED;
            break;
        case SDL_SCANCODE_RIGHT:
            dx = PLAYER_SPEED;
            break;
        default:
            break;
    }

    if (dx != 0 || dy != 0) {
        movePlayer(player, dx, dy);
    }
}

void renderPlayer(SDL_Renderer* renderer, Player* player) {
    SDL_Rect src = {
        player->frame * FRAME_WIDTH,
        (8 + player->dir) * FRAME_HEIGHT,
        FRAME_WIDTH,
        FRAME_HEIGHT
    };
    SDL_Rect dst = {
        player->x,
        player->y,
        FRAME_WIDTH,
        FRAME_HEIGHT
    };

    printf("Render frame %d at src.x=%d src.y=%d\n", player->frame, src.x, src.y);
    SDL_RenderCopy(renderer, player->texture, &src, &dst);
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

    SDL_Surface* surface = IMG_Load("assets/player_without_sword.png");
    if (!surface) {
        SDL_Log("Erreur de chargement joueur : %s", IMG_GetError());
    }
    player.texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);


    if (playerStartX == -1 || playerStartY == -1) {
        SDL_Log("Position du joueur non définie dans la carte !");
        SDL_Quit();
        return 1;
    }
    player.x = playerStartX * TILE_SIZE;
    player.y = playerStartY * TILE_SIZE;
    player.frame = 0;       // frame 1 (index 0)
    player.dir = DIR_DOWN;  // direction vers le bas
    player.frameTimer = 0;


    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                handleKeyPress(&event.key, &player, &running);
            }
        }

        

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        renderMap(renderer);
        renderPlayer(renderer, &player);

        for (int i = 0; i < keyCount; i++) {
            if (!keys[i].collected &&
                checkCollision(player.x, player.y, TILE_SIZE, TILE_SIZE,
                               keys[i].x, keys[i].y, TILE_SIZE, TILE_SIZE)) {
                
                keys[i].collected = true;

                keysCollected++;
                SDL_Log("Clé ramassée ! (%d/%d)", keysCollected, keyCount);


                int doorToOpen = keys[i].doorIndex;
        
                if (doorToOpen >= 0 && doorToOpen < doorCount) {
                    doors[doorToOpen].open = true;
                    SDL_Log("Porte %d ouverte par clé %d !", doorToOpen, i);
                }
            }
        }



        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60 FPS
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

