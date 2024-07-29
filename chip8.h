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

#ifndef CHIP8_H
#define CHIP8_H

#include <stdint.h>

#define UPS 60
#define SIZE_MEMORY 4096
#define SIZE_DISPLAY 256
#define SIZE_STACK 16
#define SIZE_V 16

#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32

typedef enum {
    HLT_NONE = 0,
    HLT_UNKNOWN_INSTRUCTION,
    HLT_STACK_OVERFLOW,
    HLT_STACK_UNDERFLOW,
    HLT_NOT_IMPLEMENTED = 0xFF,
} HaltCode;

typedef struct {
    uint8_t memory[SIZE_MEMORY];
    uint8_t display[SIZE_DISPLAY];
    uint16_t stack[SIZE_STACK];
    uint8_t v[SIZE_V];

    uint16_t i;
    uint16_t pc;
    uint8_t sp;

    uint8_t timer_delay;
    uint8_t timer_sound;

    uint16_t keys;
    HaltCode halt_code;
    uint8_t vblank;
} Chip8;

/* Basic functions */
Chip8* chip8_new();
void chip8_free(Chip8** chip8);

/* Chip8 functions */
int chip8_load(Chip8* chip8, const char* rom);
uint8_t chip8_get_pixel(Chip8* chip8, int x, int y);
void chip8_next_instruction(Chip8* chip8);
uint16_t chip8_current_instruction(Chip8* chip8);
void chip8_vblank(Chip8* chip8);

void chip8_key_down(Chip8* chip8, uint8_t key);
void chip8_key_up(Chip8* chip8, uint8_t key);

#endif // CHIP8_H
