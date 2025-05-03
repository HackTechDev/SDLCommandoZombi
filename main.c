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

// 0 = sol, 1 = mur
int map[MAP_HEIGHT][MAP_WIDTH] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,1,0,1,0,0,1,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};


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

    Player player = {TILE_SIZE * 2, TILE_SIZE * 2}; // position initiale

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

