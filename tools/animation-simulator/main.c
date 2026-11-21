#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "big_r_animation.h"
#include "big_r_patterns.h"

#define SIZE 768
static const char *names[] = {"Rainbow horizontal", "Rainbow vertical", "Rainbow quadrant", "Rainbow centered"};

static void title(SDL_Window *window, unsigned mode, unsigned brightness, bool mapped, bool paused)
{
    char text[256];
    snprintf(text, sizeof(text), "BIG-R | %s | %s | brightness %u%% | %s | 1-4: animation, M: view, +/-: brightness, Space: pause",
             names[mode - 1], mapped ? "mapped LEDs" : "color field", brightness, paused ? "paused" : "playing");
    SDL_SetWindowTitle(window, text);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1;
    }
    SDL_Window *window = SDL_CreateWindow("BIG-R", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          SIZE, SIZE, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); SDL_Quit(); return 1; }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) { fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError()); SDL_DestroyWindow(window); SDL_Quit(); return 1; }
    SDL_RenderSetLogicalSize(renderer, SIZE, SIZE);
    bool mask[BIG_R_MATRIX_PIXELS] = {false};
    for (size_t i = 0; i < sizeof(led_to_matrix) / sizeof(led_to_matrix[0]); ++i)
        mask[led_to_matrix[i].matrix_index] = true;
    unsigned mode = 1, brightness = 25, phase = 0;
    bool mapped = true, paused = false, running = true;
    RGB_color_t pixels[BIG_R_MATRIX_PIXELS];
    Uint32 last = SDL_GetTicks();
    title(window, mode, brightness, mapped, paused);
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type != SDL_KEYDOWN || event.key.repeat) continue;
            SDL_Keycode key = event.key.keysym.sym;
            if (key == SDLK_ESCAPE || key == SDLK_q) running = false;
            else if (key >= SDLK_1 && key <= SDLK_4) mode = (unsigned)(key - SDLK_1) + 1;
            else if (key == SDLK_m) mapped = !mapped;
            else if (key == SDLK_SPACE) paused = !paused;
            else if ((key == SDLK_PLUS || key == SDLK_EQUALS || key == SDLK_KP_PLUS) && brightness < 100) brightness = brightness > 95 ? 100 : brightness + 5;
            else if ((key == SDLK_MINUS || key == SDLK_KP_MINUS) && brightness > 0) brightness = brightness < 5 ? 0 : brightness - 5;
            title(window, mode, brightness, mapped, paused);
        }
        Uint32 now = SDL_GetTicks();
        if (!paused && now - last >= 20) {
            unsigned steps = (now - last) / 20;
            phase = (phase + steps) % 360;
            last += steps * 20;
        } else if (paused) last = now;
        big_r_animation_render(mode, phase, brightness, pixels);
        SDL_SetRenderDrawColor(renderer, 12, 12, 16, 255); SDL_RenderClear(renderer);
        for (unsigned i = 0; i < BIG_R_MATRIX_PIXELS; ++i) {
            if (mapped && !mask[i]) continue;
            RGB_color_t c = pixels[i];
            SDL_SetRenderDrawColor(renderer, c.red, c.green, c.blue, 255);
            SDL_Rect rect = {(int)(i % 32) * 24, (int)(i / 32) * 24, mapped ? 20 : 24, mapped ? 20 : 24};
            SDL_RenderFillRect(renderer, &rect);
        }
        SDL_RenderPresent(renderer);
        SDL_Delay(1);
    }
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}
