/*
 * MIT License
 *
 * Copyright (c) 2024 Alys Elica
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <time.h>

#include <SDL.h>

#include "chip8.h"

#define SCREEN_SCALE 10
#define SCREEN_WIDTH 64 * SCREEN_SCALE
#define SCREEN_HEIGHT 32 * SCREEN_SCALE

uint8_t sdl_key_to_chip8(SDL_Keycode key)
{
    switch (key) {
    case SDLK_0:
    case SDLK_KP_0:
        return 0x0;
    case SDLK_1:
    case SDLK_KP_1:
        return 0x1;
    case SDLK_2:
    case SDLK_KP_2:
        return 0x2;
    case SDLK_3:
    case SDLK_KP_3:
        return 0x3;
    case SDLK_4:
    case SDLK_KP_4:
        return 0x4;
    case SDLK_5:
    case SDLK_KP_5:
        return 0x5;
    case SDLK_6:
    case SDLK_KP_6:
        return 0x6;
    case SDLK_7:
    case SDLK_KP_7:
        return 0x7;
    case SDLK_8:
    case SDLK_KP_8:
        return 0x8;
    case SDLK_9:
    case SDLK_KP_9:
        return 0x9;
    case SDLK_a:
        return 0xA;
    case SDLK_b:
        return 0xB;
    case SDLK_c:
        return 0xC;
    case SDLK_d:
        return 0xD;
    case SDLK_e:
        return 0xE;
    case SDLK_f:
        return 0xF;
    default:
        return 0xFF;
    }
}

int main(int argc, char* argv[])
{
    srand(time(NULL));

    // Check arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <rom>\n", argv[0]);
        return 1;
    }

    // Init SDL
    printf("Init SDL\n");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Chip8 interpreter", 100, 100, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Init Chip8 interpreter
    printf("Init Chip8 interpreter\n");

    const char* rom = argv[1];
    Chip8* chip8 = chip8_new();
    if (chip8 == NULL) {
        fprintf(stderr, "Failed to create Chip8\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (chip8_load(chip8, rom) != 0) {
        fprintf(stderr, "Failed to load ROM\n");
        chip8_free(&chip8);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    printf("Loaded %s\n", rom);

    // Main loop
    printf("Starting Chip8 program\n");

    SDL_Event e;
    int quit = 0;
    clock_t last_time = clock();
    while (!quit) {
        // Poll events
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                quit = 1;
                break;
            case SDL_KEYDOWN:
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    quit = 1;
                }

                uint8_t key = sdl_key_to_chip8(e.key.keysym.sym);
                if (key != 0xFF) {
                    chip8_key_down(chip8, key);
                }
                break;
            case SDL_KEYUP: {
                uint8_t key = sdl_key_to_chip8(e.key.keysym.sym);
                if (key != 0xFF) {
                    chip8_key_up(chip8, key);
                }
            } break;
            }
        }

        chip8_next_instruction(chip8);

        // SDL_Delay(1);

        if (chip8->halt_code != HLT_NONE) {
            SDL_SetWindowTitle(window, "[HALTED]");
            fprintf(stderr, "Halted [%d]\n", chip8->halt_code);
            fprintf(stderr, "    PC: %04X\n", chip8->pc);
            fprintf(stderr, "    SP: %02X\n", chip8->sp);
            fprintf(stderr, "    I: %04X\n", chip8->i);

            fprintf(stderr, "    V registers:\n        ");
            for (int i = 0; i < SIZE_V; i++) {
                fprintf(stderr, "%02X ", chip8->v[i]);
            }
            fprintf(stderr, "\n");

            fprintf(stderr, "    Current instruction: %04X\n", chip8_current_instruction(chip8));
            break;
        }

        // Update timers
        clock_t current_time = clock();
        double elapsed_time_ms = ((double)(current_time - last_time)) / CLOCKS_PER_SEC * 1000.0;
        if (elapsed_time_ms >= (1000.0 / UPS)) {
            last_time = current_time;

            chip8_vblank(chip8);

            // Render
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            for (int y = 0; y < 32; y++) {
                for (int x = 0; x < 64; x++) {
                    if (chip8_get_pixel(chip8, x, y)) {
                        SDL_Rect r = { x * SCREEN_SCALE, y * SCREEN_SCALE, SCREEN_SCALE, SCREEN_SCALE };
                        SDL_RenderFillRect(renderer, &r);
                    }
                }
            }

            SDL_RenderPresent(renderer);
        }
    }

    // Cleanup
    printf("Cleanup\n");

    chip8_free(&chip8);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
