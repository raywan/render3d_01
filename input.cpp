#include "input.h"
#include <SDL.h>
#include <unordered_map>

std::unordered_map<SDL_Scancode, bool> held_state;
std::unordered_map<SDL_Scancode, bool> already_pressed_state;
std::unordered_map<Uint8, bool> mouse_held_state;
std::unordered_map<Uint8, bool> mouse_already_pressed_state;
int scroll_dir = 0;

bool is_pressed(SDL_Scancode scancode) {
  if (already_pressed_state[scancode] == false && held_state[scancode] == true) {
    return true;
  }
  return false;
}

bool is_down(SDL_Scancode scancode) {
  return held_state[scancode];
}

bool is_up(SDL_Scancode scancode) {
  return !held_state[scancode];
}

bool is_mouse_pressed(Uint8 event) {
  if (mouse_already_pressed_state[event] == false && mouse_held_state[event] == true) {
    return true;
  }
  return false;
}

bool is_mouse_down(Uint8 event) {
  return mouse_held_state[event];
}

bool is_mouse_up(Uint8 event) {
  return !mouse_held_state[event];
}

int is_mouse_scrolling() {
  return scroll_dir;
}

void process_raw_input(bool *quit) {
  scroll_dir = 0;
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    switch (e.type) {
      case SDL_QUIT:
        *quit = true;
        break;
      case SDL_MOUSEMOTION:
        break;
      case SDL_MOUSEWHEEL:
        if (e.wheel.y > 0) {
          scroll_dir = 1;
        } else if (e.wheel.y < 0) {
          scroll_dir = -1;
        }
        break;
      case SDL_WINDOWEVENT:
        switch (e.window.event) {
          case SDL_WINDOWEVENT_RESIZED:
            puts("window resized");
            break;
        }
        break;
      default:
        break;
    }
  }
  // Preprocess the keyboard state
  int num_keys;
  const Uint8 *key_state = SDL_GetKeyboardState(&num_keys);
  for (int i = 0; i < num_keys; i++) {
    SDL_Scancode scancode = (SDL_Scancode) i;
    // printf("scancode %d\n", scancode);
    if (key_state[scancode]) {
      // Check if we're already pressing
      if (!already_pressed_state[scancode] && held_state[scancode]) {
        already_pressed_state[scancode] = true;
      }
      held_state[scancode] = true;
    } else if (!key_state[scancode]) {
      held_state[scancode] = false;
      already_pressed_state[scancode] = false;
    }
  }
  // Go through all the mouse buttons that SDL supports
  for (int i = 1; i <= 5; i++) {
    if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(i)) {
      if (!mouse_already_pressed_state[i] && mouse_held_state[i]) {
        mouse_already_pressed_state[i] = true;
      }
      mouse_held_state[e.button.button] = true;
    } else if (!(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(i))) {
      mouse_held_state[i] = false;
      mouse_already_pressed_state[i] = false;
    }
  }
}