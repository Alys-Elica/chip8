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

#include "chip8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Memory map
#define ADDRESS_CODE_BEG 0x0200 // 0x0200-0x0FFF: Program ROM and work RAM

// Font data
const uint8_t FONT_DATA[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80, // F
};

/* Private functions */
void clear_chip8(Chip8* chip8)
{
    memset(chip8->memory, 0, SIZE_MEMORY);
    memset(chip8->display, 0, SIZE_DISPLAY);
    memset(chip8->stack, 0, SIZE_STACK);
    memset(chip8->v, 0, SIZE_V);

    chip8->i = 0;
    chip8->pc = ADDRESS_CODE_BEG;
    chip8->sp = 0;
    chip8->halt_code = HLT_NONE;

    chip8->timer_delay = 0;
    chip8->timer_sound = 0;

    chip8->keys = 0;
    chip8->vblank = 0;

    // Load font data
    memcpy(chip8->memory, FONT_DATA, sizeof(FONT_DATA));
}

void op_0x0NNN(Chip8* chip8, uint16_t opcode)
{
    switch (opcode & 0x00FF) {
    case 0xE0: // Instr 0x00E0: Clear screen
        memset(chip8->display, 0, SIZE_DISPLAY);
        break;
    case 0xEE: // Instr 0x00EE: Return from subroutine
        if (chip8->sp == 0) {
            fprintf(stderr, "Stack underflow\n");
            chip8->halt_code = HLT_STACK_UNDERFLOW;
            return;
        }

        chip8->pc = chip8->stack[--chip8->sp];
        break;
    default: // Instr 0x0NNN: Execute machine language subroutine at address NNN
             // Ignore this instruction
        fprintf(stderr, "Opcode 0x0NNN ignored\n");
        break;
    }
}

void op_0x2000(Chip8* chip8, uint16_t opcode)
{
    uint16_t nnn = opcode & 0x0FFF;
    if (chip8->sp == SIZE_STACK) {
        fprintf(stderr, "Stack overflow\n");
        chip8->halt_code = HLT_STACK_OVERFLOW;
        return;
    }

    chip8->stack[chip8->sp++] = chip8->pc;
    chip8->pc = nnn;
}

void op_0x8XYN(Chip8* chip8, uint16_t opcode)
{
    uint8_t x = (opcode & 0x0F00) >> 8;
    uint8_t y = (opcode & 0x00F0) >> 4;
    switch (opcode & 0x000F) {
    case 0x0: // Instr 0x8XY0: Store value of register VY in register VX
        chip8->v[x] = chip8->v[y];
        break;
    case 0x1: // Instr 0x8XY1: Set VX to VX OR VY
        chip8->v[x] |= chip8->v[y];
        chip8->v[0xF] = 0;
        break;
    case 0x2: // Instr 0x8XY2: Set VX to VX AND VY
        chip8->v[x] &= chip8->v[y];
        chip8->v[0xF] = 0;
        break;
    case 0x3: // Instr 0x8XY3: Set VX to VX XOR VY
        chip8->v[x] ^= chip8->v[y];
        chip8->v[0xF] = 0;
        break;
    case 0x4: // Instr 0x8XY4: Add VY to VX, set VF to 0x01 if carry, else 0x00
        uint16_t sum = chip8->v[x] + chip8->v[y];
        chip8->v[x] = sum & 0xFF;
        chip8->v[0xF] = (sum > 0xFF) ? 0x01 : 0x00;
        break;
    case 0x5: // Instr 0x8XY5: Subtract VY from VX, set VF to 0x00 if borrow, else 0x01
        uint8_t tmp = (chip8->v[x] >= chip8->v[y]) ? 0x01 : 0x00;
        chip8->v[x] -= chip8->v[y];
        chip8->v[0xF] = tmp;
        break;
    case 0x6: { // Instr 0x8XY6: Store value of register VY shifted right one bit in register VX
                // Set VF to least significant bit of VY before shift
        uint8_t tmp = chip8->v[y] & 0x01;
        chip8->v[x] = chip8->v[y] >> 1;
        chip8->v[0xF] = tmp;
    } break;
    case 0x7: { // Instr 0x8XY7: Set VX to VY minus VX, set VF to 0x00 if borrow, else 0x01
        uint8_t tmp = (chip8->v[y] >= chip8->v[x]) ? 0x01 : 0x00;
        chip8->v[x] = chip8->v[y] - chip8->v[x];
        chip8->v[0xF] = tmp;
    } break;
    case 0xE: { // Instr 0x8XYE: Store value of register VY shifted left one bit in register VX
                // Set VF to most significant bit of VY before shift
        uint8_t tmp = (chip8->v[y] >> 7) & 0x01;
        chip8->v[x] = chip8->v[y] << 1;
        chip8->v[0xF] = tmp;
    } break;
    default:
        fprintf(stderr, "Unknown opcode: 0x%X\n", opcode);
        chip8->halt_code = HLT_UNKNOWN_INSTRUCTION;
        return;
    }
}

void op_0xDXYN(Chip8* chip8, uint16_t opcode)
{
    uint8_t x = (opcode & 0x0F00) >> 8;
    uint8_t y = (opcode & 0x00F0) >> 4;

    if (chip8->vblank == 0) {
        chip8->pc -= 2;
        return;
    }

    chip8->vblank = 0;

    uint8_t vx = chip8->v[x] % DISPLAY_WIDTH;
    uint8_t vy = chip8->v[y] % DISPLAY_HEIGHT;
    uint8_t count = opcode & 0x000F;
    uint16_t address = chip8->i;
    uint8_t unset = 0;

    for (int i = 0; i < count; i++) {
        uint8_t sprite = chip8->memory[address + i];
        if (vy + i >= DISPLAY_HEIGHT) {
            break;
        }
        for (int j = 0; j < 8; j++) {
            if (vx + j >= DISPLAY_WIDTH) {
                break;
            }
            uint8_t pixel = (sprite >> (7 - j)) & 1;
            uint16_t index = (vx + j) + (vy + i) * DISPLAY_WIDTH;
            uint8_t old_pixel = (chip8->display[index / 8] >> (7 - (index % 8))) & 1;
            chip8->display[index / 8] ^= (pixel << (7 - (index % 8)));
            if (old_pixel == 1 && pixel == 1 && ((chip8->display[index / 8] >> (7 - (index % 8))) & 1) == 0) {
                unset = 1;
            }
        }
    }

    chip8->v[0xF] = unset;
}

void op_0xEXNN(Chip8* chip8, uint16_t opcode)
{
    uint8_t x = (opcode & 0x0F00) >> 8;
    switch (opcode & 0x00FF) {
    case 0x9E: // Instr 0xEX9E: Skip next instruction if key with the value of VX is pressed
        if (chip8->keys & (1 << chip8->v[x])) {
            chip8->pc += 2;
        }
        break;
    case 0xA1: // Instr 0xEXA1: Skip next instruction if key with the value of VX is not pressed
        if (!(chip8->keys & (1 << chip8->v[x]))) {
            chip8->pc += 2;
        }
        break;
    default:
        fprintf(stderr, "Unknown opcode: 0x%X\n", opcode);
        chip8->halt_code = HLT_UNKNOWN_INSTRUCTION;
        return;
    }
}

void op_0xFXNN(Chip8* chip8, uint16_t opcode)
{
    uint8_t x = (opcode & 0x0F00) >> 8;
    switch (opcode & 0x00FF) {
    case 0x07: // Instr 0xFX07: Store the current value of the delay timer in register VX
        chip8->v[x] = chip8->timer_delay;
        break;
    case 0x0A: // Instr 0xFX0A: Wait for a keypress and store the result in register VX
        if (chip8->keys == 0) {
            chip8->pc -= 2;
            return;
        }

        for (int i = 0; i < 16; i++) {
            if (chip8->keys & (1 << i)) {
                chip8->v[x] = i;
                break;
            }
        }
        break;
    case 0x15: // Instr 0xFX15: Set the delay timer to the value of register VX
        chip8->timer_delay = chip8->v[x];
        break;
    case 0x18: // Instr 0xFX18: Set the sound timer to the value of register VX
        chip8->timer_sound = chip8->v[x];
        break;
    case 0x1E: // Instr 0xFX1E: Add the value stored in register VX to register I
        chip8->i += chip8->v[x];
        break;
    case 0x29: // Instr 0xFX29: Set I to the memory address of the sprite data corresponding to the hexadecimal digit stored in register VX
        chip8->i = chip8->memory[chip8->v[x] * 5];
        break;
    case 0x33: // Instr 0xFX33: Store the binary-coded decimal equivalent of the value stored in register VX at addresses I, I+1, and I+2
        chip8->memory[chip8->i] = chip8->v[x] / 100;
        chip8->memory[chip8->i + 1] = (chip8->v[x] / 10) % 10;
        chip8->memory[chip8->i + 2] = chip8->v[x] % 10;
        break;
    case 0x55: // Instr 0xFX55: Store the values of registers V0 to VX inclusive in memory starting at address I
               // I is set to I + X + 1 after operation
        for (int i = 0; i <= x; i++) {
            chip8->memory[chip8->i + i] = chip8->v[i];
        }
        chip8->i += x + 1;
        break;

    case 0x65: // Instr 0xFX65: Fill registers V0 to VX inclusive with the values stored in memory starting at address I
               // I is set to I + X + 1 after operation
        for (int i = 0; i <= x; i++) {
            chip8->v[i] = chip8->memory[chip8->i + i];
        }
        chip8->i += x + 1;
        break;
    default:
        fprintf(stderr, "Unknown opcode: 0x%X\n", opcode);
        chip8->halt_code = HLT_UNKNOWN_INSTRUCTION;
        return;
    }
}

/* Basic functions */
Chip8* chip8_new()
{
    Chip8* chip8 = malloc(sizeof(Chip8));
    if (chip8 == NULL) {
        return NULL;
    }

    clear_chip8(chip8);

    return chip8;
}

void chip8_free(Chip8** chip8)
{
    free(*chip8);
    *chip8 = NULL;
}

/* Chip8 functions */
int chip8_load(Chip8* chip8, const char* rom)
{
    FILE* f = fopen(rom, "rb");
    if (f == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", rom);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    if (size > SIZE_MEMORY - ADDRESS_CODE_BEG) {
        fprintf(stderr, "ROM too big\n");
        fclose(f);
        return 2;
    }

    clear_chip8(chip8);

    fread(chip8->memory + ADDRESS_CODE_BEG, size, 1, f);
    fclose(f);

    return 0;
}

uint8_t chip8_get_pixel(Chip8* chip8, int x, int y)
{
    uint16_t address = (x % DISPLAY_WIDTH) / 8 + (y % DISPLAY_HEIGHT) * 8;
    return (chip8->display[address] >> (7 - (x % 8))) & 1;
}

void chip8_next_instruction(Chip8* chip8)
{
    uint16_t opcode = chip8->memory[chip8->pc] << 8 | chip8->memory[chip8->pc + 1];
    chip8->pc += 2;

    uint8_t x = (opcode & 0x0F00) >> 8;
    uint8_t y = (opcode & 0x00F0) >> 4;
    uint8_t nn = opcode & 0x00FF;
    uint16_t nnn = opcode & 0x0FFF;
    switch (opcode & 0xF000) {
    case 0x0000: // Instr 0x0NNN: Execute operation based on NNN
        op_0x0NNN(chip8, opcode);
        break;
    case 0x1000: // Instr 0x1NNN: Jump to address NNN
        chip8->pc = nnn;
        break;
    case 0x2000: // Instr 0x2NNN: Call subroutine at address NNN
        op_0x2000(chip8, opcode);
        break;
    case 0x3000: // Instr 0x3XNN: Skip next instruction if register VX == NN
        if (chip8->v[x] == nn) {
            chip8->pc += 2;
        }
        break;
    case 0x4000: // Instr 0x4XNN: Skip next instruction if register VX != NN
        if (chip8->v[x] != nn) {
            chip8->pc += 2;
        }
        break;
    case 0x5000: // Instr 0x5XY0: Skip next instruction if register VX == VY
        if (chip8->v[x] == chip8->v[y]) {
            chip8->pc += 2;
        }
        break;
    case 0x6000: // Instr 0x6XNN: Store number NN in register VX
        chip8->v[x] = nn;
        break;
    case 0x7000: // Instr 0x7XNN: Add number NN to register VX
        chip8->v[x] += nn;
        break;
    case 0x8000: // Instr 0x8XYN: Execute operation based on N
        op_0x8XYN(chip8, opcode);
        break;
    case 0x9000: // Instr 0x9XY0: Skip next instruction if register VX != VY
        if (chip8->v[x] != chip8->v[y]) {
            chip8->pc += 2;
        }
        break;
    case 0xA000: // Instr 0xANNN: Store memory address NNN in register I
        chip8->i = nnn;
        break;
    case 0xB000: // Instr 0xBNNN: Jump to address NNN + V0
        chip8->pc = nnn + chip8->v[0];
        break;
    case 0xC000: // Instr 0xCXNN: Set VX to a random number AND NN
        chip8->v[x] = rand() & nn;
        break;
    case 0xD000: // Instr 0xDXYN: Draw a sprite at position VX, VY with N bytes of sprite data starting at the address stored in I
                 // Set VF to 0x01 if any set pixels are changed to unset, else 0x00
        op_0xDXYN(chip8, opcode);
        break;
    case 0xE000: // Instr 0xEXNN: Execute operation based on NN
        op_0xEXNN(chip8, opcode);
        break;
    case 0xF000: // Instr 0xFXNN: Execute operation based on NN
        op_0xFXNN(chip8, opcode);
    }
}

uint16_t chip8_current_instruction(Chip8* chip8)
{
    return chip8->memory[chip8->pc] << 8 | chip8->memory[chip8->pc + 1];
}

void chip8_vblank(Chip8* chip8)
{
    chip8->vblank = 1;

    if (chip8->timer_delay > 0) {
        chip8->timer_delay--;
    }

    if (chip8->timer_sound > 0) {
        chip8->timer_sound--;
    }
}

void chip8_key_down(Chip8* chip8, uint8_t key)
{
    chip8->keys |= 1 << (key & 0xF);
}

void chip8_key_up(Chip8* chip8, uint8_t key)
{
    chip8->keys &= ~(1 << (key & 0xF));
}
