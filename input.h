#pragma once

#include <SDL.h>

bool is_pressed(SDL_Scancode scancode);
bool is_down(SDL_Scancode scancode);
bool is_up(SDL_Scancode scancode);
bool is_mouse_pressed(Uint8 event);
bool is_mouse_down(Uint8 event);
bool is_mouse_up(Uint8 event);
int is_mouse_scrolling();
void process_raw_input(bool *quit);
