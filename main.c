#include <SDL2/SDL.h>
#include <stdbool.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define PLAYER_SPEED 4

typedef struct {
    int x, y;
    SDL_Texture* texture;
    SDL_Rect srcRect, destRect;
} Player;

bool init(SDL_Window** window, SDL_Renderer** renderer) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("SDL_Init Error: %s", SDL_GetError());
        return false;
    }

    *window = SDL_CreateWindow("Zelda-Like", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!*window) {
        SDL_Log("SDL_CreateWindow Error: %s", SDL_GetError());
        return false;
    }

    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
    if (!*renderer) {
        SDL_Log("SDL_CreateRenderer Error: %s", SDL_GetError());
        return false;
    }

    return true;
}

SDL_Texture* loadTexture(const char* path, SDL_Renderer* renderer) {
    SDL_Surface* surface = SDL_LoadBMP(path);
    if (!surface) {
        SDL_Log("SDL_LoadBMP Error: %s", SDL_GetError());
        return NULL;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

void handleInput(bool* running, const Uint8* keystate, Player* player) {
    if (keystate[SDL_SCANCODE_ESCAPE]) {
        *running = false;
    }
    if (keystate[SDL_SCANCODE_UP])    player->y -= PLAYER_SPEED;
    if (keystate[SDL_SCANCODE_DOWN])  player->y += PLAYER_SPEED;
    if (keystate[SDL_SCANCODE_LEFT])  player->x -= PLAYER_SPEED;
    if (keystate[SDL_SCANCODE_RIGHT]) player->x += PLAYER_SPEED;
}

void render(SDL_Renderer* renderer, Player* player) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    player->destRect.x = player->x;
    player->destRect.y = player->y;

    SDL_RenderCopy(renderer, player->texture, &player->srcRect, &player->destRect);

    SDL_RenderPresent(renderer);
}

int main() {
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;

    if (!init(&window, &renderer)) return 1;

    Player player = {100, 100};
    player.texture = loadTexture("assets/player.bmp", renderer);
    player.srcRect = (SDL_Rect){0, 0, 32, 32};
    player.destRect = (SDL_Rect){player.x, player.y, 32, 32};

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
        }

        const Uint8* keystate = SDL_GetKeyboardState(NULL);
        handleInput(&running, keystate, &player);
        render(renderer, &player);
        SDL_Delay(16); // ~60 FPS
    }

    SDL_DestroyTexture(player.texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

