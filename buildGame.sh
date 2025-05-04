#!/bin/sh

gcc main.c -o SDLCommandoZombi `sdl2-config --cflags --libs` -lSDL2 -lSDL2_image -lSDL2_ttf
