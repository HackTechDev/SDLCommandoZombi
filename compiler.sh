#!/bin/sh

gcc main.c -o zelda_like `sdl2-config --cflags --libs` -lSDL2 -lSDL2_image
