#include <SDL2/SDL.h>
#include <stdbool.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

bool loadMap(const char* filename);

#define WORLD_WIDTH  3
#define WORLD_HEIGHT 3

typedef struct MapInfo {
    char* filename;
    bool exists;
} MapInfo;

MapInfo world[WORLD_HEIGHT][WORLD_WIDTH];



int currentMapX = 1;
int currentMapY = 1;

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

typedef struct {
    int x, y;
    bool active;
} Box;

#define MAX_BOXES 10
Box boxes[MAX_BOXES];
int boxCount = 0;


// 0 = sol, 1 = mur
int map[MAP_HEIGHT][MAP_WIDTH];

int playerStartX = -1;
int playerStartY = -1;



typedef enum {
    STATE_MENU,
    STATE_GAME,
    STATE_QUIT
} GameState;


typedef struct {
    int x, y;
    bool active;        // l'interrupteur existe dans la carte
    bool triggered;     // est-ce qu'une caisse est dessus ?
    int linkedDoor;     // -1 si aucun lien, sinon index de la porte à ouvrir
} Switch;

#define MAX_SWITCHES 10
Switch switches[MAX_SWITCHES];
int switchCount = 0;

SDL_Texture* boxTexture = NULL;
SDL_Texture* wallTexture = NULL;
SDL_Texture* groundTexture = NULL;
SDL_Texture* switchOffTexture = NULL;
SDL_Texture* switchOnTexture = NULL;
SDL_Texture* doorTexture = NULL;

SDL_Texture* menuBackground = NULL;

void loadMapFromWorld(int x, int y) {
    if (x < 0 || x >= WORLD_WIDTH || y < 0 || y >= WORLD_HEIGHT)
        return;

    if (!world[y][x].exists)
        return;

    currentMapX = x;
    currentMapY = y;

    loadMap(world[y][x].filename);
    SDL_Log("filename: %s", world[y][x].filename);   
    
}


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
                case '.':
                    map[y][x] = 0;
                    break;
                case '#':
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
                case 'C':
                    map[y][x] = 0;
                    if (boxCount < MAX_BOXES) {
                        boxes[boxCount++] = (Box){ x * TILE_SIZE, y * TILE_SIZE, true };
                    }
                    break; 
                    
                case 'S':    
                    if (switchCount < MAX_SWITCHES) {
                        switches[switchCount++] = (Switch){
                            .x = x * TILE_SIZE,
                            .y = y * TILE_SIZE,
                            .active = true,
                            .triggered = false,
                            .linkedDoor = -1 // tu peux lier plus tard
                        };
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



    // Liaison manuelle : interrupteur 0 ouvre porte 0
    if (switchCount > 0 && doorCount > 0) {
          SDL_Log("Set Switch to door");
        switches[0].linkedDoor = 0;
    }

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

void renderBoxes(SDL_Renderer* renderer) {
    for (int i = 0; i < boxCount; i++) {
        if (boxes[i].active) {
            SDL_Rect dest = { boxes[i].x, boxes[i].y, TILE_SIZE, TILE_SIZE };
            if (boxTexture)
                SDL_RenderCopy(renderer, boxTexture, NULL, &dest);
            else {
                SDL_SetRenderDrawColor(renderer, 150, 100, 50, 255);
                SDL_RenderFillRect(renderer, &dest);
            }
        }
    }
}

void renderWalls(SDL_Renderer* renderer) {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            SDL_Rect tileRect = {x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE};
            
            if (map[y][x] == 1) {
                if (wallTexture) {
                    SDL_RenderCopy(renderer, wallTexture, NULL, &tileRect);
                } else {
                    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
                    SDL_RenderFillRect(renderer, &tileRect);
                }                
            } else {
                if (groundTexture) {
                    SDL_RenderCopy(renderer, groundTexture, NULL, &tileRect);
                } else {
                    SDL_SetRenderDrawColor(renderer, 20, 150, 20, 255); // sol
                    SDL_RenderFillRect(renderer, &tileRect);
                }

            }
        }
    }
}


void renderDoors(SDL_Renderer* renderer) {
    for (int i = 0; i < doorCount; i++) {
        if (!doors[i].open) {
           SDL_Rect r = { doors[i].x, doors[i].y, TILE_SIZE, TILE_SIZE };
            if (boxTexture) {
                SDL_RenderCopy(renderer, doorTexture, NULL, &r);
            } else {
               SDL_SetRenderDrawColor(renderer, 0, 0, 200, 255);
               SDL_RenderFillRect(renderer, &r);
            }

        }
    }
}


void  renderSwitchs(SDL_Renderer* renderer) {
    for (int i = 0; i < switchCount; i++) {
        if (switches[i].active) {
            SDL_Rect rect = { switches[i].x, switches[i].y, TILE_SIZE, TILE_SIZE };
            if (switches[i].triggered) {

                if (switchOnTexture) {
                    SDL_RenderCopy(renderer, switchOnTexture, NULL, &rect);
                } else {
                    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); // vert = activé
                    SDL_RenderFillRect(renderer, &rect);
                }

            } else {

                if (switchOffTexture) {
                    SDL_RenderCopy(renderer, switchOffTexture, NULL, &rect);
                } else {
                    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // rouge = inactif
                    SDL_RenderFillRect(renderer, &rect);
                }
            }
        }
    }
}


void renderMap(SDL_Renderer* renderer) {
    
    renderWalls(renderer);
        

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


    renderDoors(renderer);

    renderBoxes(renderer);

   renderSwitchs(renderer);
    
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

    // Vérifie s’il y a une caisse
    for (int i = 0; i < boxCount; i++) {
        if (boxes[i].active &&
            checkCollision(newX, newY, TILE_SIZE, TILE_SIZE,
                        boxes[i].x, boxes[i].y, TILE_SIZE, TILE_SIZE)) {

            // Coordonnées de destination de la caisse
            int boxNewX = boxes[i].x + dx;
            int boxNewY = boxes[i].y + dy;

            // Vérifie si la caisse peut être poussée (pas de mur ni autre caisse)
            if (!isCollision(boxNewX, boxNewY, TILE_SIZE)) {
                bool boxBlocked = false;

                for (int j = 0; j < boxCount; j++) {
                    if (i != j && boxes[j].active &&
                        checkCollision(boxNewX, boxNewY, TILE_SIZE, TILE_SIZE,
                                    boxes[j].x, boxes[j].y, TILE_SIZE, TILE_SIZE)) {
                        boxBlocked = true;
                        break;
                    }
                }

                if (!boxBlocked) {
                    // Déplace la caisse et le joueur
                    boxes[i].x = boxNewX;
                    boxes[i].y = boxNewY;
                    player->x = newX;
                    player->y = newY;
                    return;
                }
            }

            // Sinon, blocage : le joueur ne bouge pas
            return;
        }
    }


    // Détection de direction
    if (dy < 0) player->dir = DIR_UP;
    else if (dy > 0) player->dir = DIR_DOWN;
    else if (dx < 0) player->dir = DIR_LEFT;
    else if (dx > 0) player->dir = DIR_RIGHT;


    // Transition droite
    if (newX + TILE_SIZE > MAP_WIDTH * TILE_SIZE) {
        if (currentMapX + 1 < WORLD_WIDTH && world[currentMapY][currentMapX + 1].exists) {
            loadMapFromWorld(currentMapX + 1, currentMapY);
            player->x = 0;
            return;
        } else {
            newX = MAP_WIDTH * TILE_SIZE - TILE_SIZE; // blocage
        }
    }

    // Transition gauche
    if (newX < 0) {
        if (currentMapX - 1 >= 0 && world[currentMapY][currentMapX - 1].exists) {
            loadMapFromWorld(currentMapX - 1, currentMapY);
            player->x = (MAP_WIDTH - 1) * TILE_SIZE;
            return;
        } else {
            newX = 0;
        }
    }

    // Transition haut
    if (newY < 0) {
        if (currentMapY - 1 >= 0 && world[currentMapY - 1][currentMapX].exists) {
            loadMapFromWorld(currentMapX, currentMapY - 1);
            player->y = (MAP_HEIGHT - 1) * TILE_SIZE;
            return;
        } else {
            newY = 0;
        }
    }

    // Transition bas
    if (newY + TILE_SIZE > MAP_HEIGHT * TILE_SIZE) {
        if (currentMapY + 1 < WORLD_HEIGHT && world[currentMapY + 1][currentMapX].exists) {
            loadMapFromWorld(currentMapX, currentMapY + 1);
            player->y = 0;
            return;
        } else {
            newY = MAP_HEIGHT * TILE_SIZE - TILE_SIZE;
        }
    }

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

    //printf("Render frame %d at src.x=%d src.y=%d\n", player->frame, src.x, src.y);
    
    SDL_RenderCopy(renderer, player->texture, &src, &dst);
}

void initWorld() {
    memset(world, 0, sizeof(world));

    world[1][1] = (MapInfo){ "world/map_1_1.txt", true };
    world[1][0] = (MapInfo){ "world/map_1_0.txt", true };
    world[1][2] = (MapInfo){ "world/map_1_2.txt", true };
    world[0][1] = (MapInfo){ "world/map_0_1.txt", true };
    world[2][1] = (MapInfo){ "world/map_2_1.txt", true };
}

void renderText(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderText_Solid(font, text, color);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dest = { x, y, surface->w, surface->h };
    SDL_RenderCopy(renderer, texture, NULL, &dest);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}


void activateSwitch(Player* player) {
    SDL_Log("Activate Swich");


    for (int i = 0; i < switchCount; i++) {
        switches[i].triggered = false;
    
        for (int j = 0; j < boxCount; j++) {
            SDL_Log("i: %d, j: %d, %s", i, j, (boxes[j].active ? "true" : "false"));
            SDL_Log("box: %d, %d, %d / switch: %d, %d, %d", j, boxes[j].x, boxes[j].y, i, switches[i].x, switches[i].y);
            if (boxes[j].active && checkCollision(boxes[j].x, boxes[j].y, TILE_SIZE, TILE_SIZE, switches[i].x, switches[i].y, TILE_SIZE, TILE_SIZE)) {
                SDL_Log("switch triggered");
                switches[i].triggered = true;
    
                // Si lié à une porte, l'ouvrir
                if (switches[i].linkedDoor >= 0 && switches[i].linkedDoor < doorCount) {
                    doors[switches[i].linkedDoor].open = true;
                }
            }
        }
    }
}


void renderTextCentered(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, bool selected) {
    SDL_Color color = selected ? (SDL_Color){255, 255, 0, 255} : (SDL_Color){255, 255, 255, 255};

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dst = { x - surface->w / 2, y, surface->w, surface->h };
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}


void renderMenu(SDL_Renderer* renderer, TTF_Font* font, SDL_Texture* background, SDL_Texture* cursorTexture, int* hoveredIndex) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    if (background) {
        SDL_Rect bgRect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
        SDL_RenderCopy(renderer, background, NULL, &bgRect);
    }

    const char* menuItems[] = { "MISSION", "QUITTER" };
    int menuX = SCREEN_WIDTH / 2;
    int menuY = 250;
    int lineSpacing = 60;

    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);
    *hoveredIndex = -1;

    for (int i = 0; i < 2; i++) {
        SDL_Color color = {255, 255, 255, 255}; // blanc par défaut

        // Création surface et détection hover
        SDL_Surface* surface = TTF_RenderUTF8_Blended(font, menuItems[i], color);
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

        SDL_Rect textRect = {
            menuX - surface->w / 2,
            menuY + i * lineSpacing,
            surface->w,
            surface->h
        };

        // Hover detection
        if (mouseX >= textRect.x && mouseX <= textRect.x + textRect.w &&
            mouseY >= textRect.y && mouseY <= textRect.y + textRect.h) {
            color = (SDL_Color){255, 255, 0, 255}; // Jaune si survolé
            *hoveredIndex = i;
        }

        // Recréer texture avec la bonne couleur si survol
        SDL_FreeSurface(surface);
        surface = TTF_RenderUTF8_Blended(font, menuItems[i], color);
        SDL_DestroyTexture(texture);
        texture = SDL_CreateTextureFromSurface(renderer, surface);

        SDL_RenderCopy(renderer, texture, NULL, &textRect);

        // Curseur graphique facultatif
        if (*hoveredIndex == i && cursorTexture) {
            SDL_Rect cursorRect = { menuX - 120, textRect.y, 32, 32 };
            SDL_RenderCopy(renderer, cursorTexture, NULL, &cursorRect);
        }

        SDL_FreeSurface(surface);
        SDL_DestroyTexture(texture);
    }

    SDL_RenderPresent(renderer);
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("SDLCommandoZombi", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    initWorld();

    loadMapFromWorld(currentMapX, currentMapY);


    SDL_Surface* tempSurface;

    tempSurface = IMG_Load("assets/box.png");
    if (!tempSurface) {
        SDL_Log("Erreur chargement box.png : %s", IMG_GetError());
    } else {
        boxTexture = SDL_CreateTextureFromSurface(renderer, tempSurface);
        SDL_FreeSurface(tempSurface);
    }
    
    tempSurface = IMG_Load("assets/wall.png");
    if (!tempSurface) {
        SDL_Log("Erreur chargement wall.png : %s", IMG_GetError());
    } else {
        wallTexture = SDL_CreateTextureFromSurface(renderer, tempSurface);
        SDL_FreeSurface(tempSurface);
    }

    tempSurface = IMG_Load("assets/ground.png");
    if (!tempSurface) {
        SDL_Log("Erreur chargement ground.png : %s", IMG_GetError());
    } else {
        groundTexture = SDL_CreateTextureFromSurface(renderer, tempSurface);
        SDL_FreeSurface(tempSurface);
    }

    tempSurface = IMG_Load("assets/door.png");
    if (!tempSurface) {
        SDL_Log("Erreur chargement door.png : %s", IMG_GetError());
    } else {
        doorTexture = SDL_CreateTextureFromSurface(renderer, tempSurface);
        SDL_FreeSurface(tempSurface);
    }

    tempSurface = IMG_Load("assets/switchOff.png");
    if (!tempSurface) {
        SDL_Log("Erreur chargement switchOff.png : %s", IMG_GetError());
    } else {
        switchOffTexture = SDL_CreateTextureFromSurface(renderer, tempSurface);
        SDL_FreeSurface(tempSurface);
    }

    tempSurface = IMG_Load("assets/switchOn.png");
    if (!tempSurface) {
        SDL_Log("Erreur chargement switchOn.png : %s", IMG_GetError());
    } else {
        switchOnTexture = SDL_CreateTextureFromSurface(renderer, tempSurface);
        SDL_FreeSurface(tempSurface);
    }


    SDL_Surface* menu_background = IMG_Load("assets/menu_background.png");
    if (!menu_background) {
        SDL_Log("Erreur chargement fond menu : %s", IMG_GetError());
    } else {
        menuBackground = SDL_CreateTextureFromSurface(renderer, menu_background);
        SDL_FreeSurface(menu_background);
    }

    SDL_Texture* cursorTexture = NULL;

    SDL_Surface* cursor = IMG_Load("assets/cursor.png");
    if (!cursor) {
        SDL_Log("Erreur chargement curseur : %s", IMG_GetError());
    } else {
        cursorTexture = SDL_CreateTextureFromSurface(renderer, cursor);
        SDL_FreeSurface(cursor);
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
   
    GameState gameState = STATE_MENU;

    if (TTF_Init() == -1) {
        SDL_Log("Erreur initialisation SDL_ttf: %s", TTF_GetError());
        return 1;
    }


    TTF_Font* font = TTF_OpenFont("assets/font.ttf", 28);
    if (!font) {
        SDL_Log("Erreur chargement police: %s", TTF_GetError());
        return 1;
    }

    int selected = -1;
    int hovered = -1;
    
    while (running) {
        SDL_Event event;
    
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
    
            if (gameState == STATE_MENU) {
                if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                    if (event.key.keysym.sym == SDLK_RETURN) {
                        if (selected == 0) gameState = STATE_GAME;
                        else if (selected == 1) gameState = STATE_QUIT;
                    } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                        gameState = STATE_QUIT;
                    }
                } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                    if (selected == 0) gameState = STATE_GAME;
                    else if (selected == 1) gameState = STATE_QUIT;
                }
            } else if (gameState == STATE_GAME) {
                if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        gameState = STATE_MENU;  // Retour au menu au lieu de quitter
                    } else if (event.key.keysym.sym == SDLK_a) {
                        activateSwitch(&player);
                    } else {
                        handleKeyPress(&event.key, &player, &running);
                    }
                }
            }
        }
    
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
    
        if (gameState == STATE_MENU) {
            renderMenu(renderer, font, menuBackground, cursorTexture, &selected);
        } else if (gameState == STATE_GAME) {
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
        } else if (gameState == STATE_QUIT) {
            running = false;
        }
    
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    if (boxTexture) SDL_DestroyTexture(boxTexture);
    if (wallTexture) SDL_DestroyTexture(wallTexture);
    

    TTF_CloseFont(font);

    if (menuBackground) {
        SDL_DestroyTexture(menuBackground);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}

