#include "chip8.h"

#include <SDL2/SDL.h>

#include <stdio.h>
#include <string.h>

extern FILE* logfile;

struct chip8* init_chip8(char* prog_file) {
    struct chip8* chip8 = calloc(1, sizeof(struct chip8));
    chip8->pc = PROG_OFFSET;
    FILE* prog = fopen(prog_file, "rb");
    if (!prog) {
        free(chip8);
        return NULL;
    }
    fseek(prog, 0, SEEK_END);
    long size = ftell(prog);
    if (size > RAM_SIZE - PROG_OFFSET) {
        free(chip8);
        fclose(prog);
        return NULL;
    }
    fseek(prog, 0, SEEK_SET);
    if (fread(chip8->ram + PROG_OFFSET, 1, size, prog) < 1) {
        fclose(prog);
        free(chip8);
        return NULL;
    }
    fclose(prog);
    static Uint8 font[] = {
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
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    };
    memcpy(chip8->ram + FONT_OFFSET, font, sizeof(font));
    return chip8;
}

Uint8 reverse_byte(Uint8 b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

void run_instruction(struct chip8* chip8) {
    Uint16 instruction =
        (chip8->ram[chip8->pc] << 8) | chip8->ram[chip8->pc + 1];
    fprintf(logfile, "pc: %04x instr %04x: ", chip8->pc, instruction);
    chip8->pc += 2;

    Uint8 opcode = instruction >> 12;
    Uint16 addr = instruction & 0x0fff;
    Uint8 num = instruction & 0x00ff;
    Uint8 rx = (instruction & 0x0f00) >> 8;
    Uint8 ry = (instruction & 0x00f0) >> 4;
    Uint8 h = instruction & 0x000f;
    switch (opcode) {
        case 0x0:
            if (instruction == 0x00e0) { // clear screen
                memset(chip8->display, 0, DISPLAY_ROWS * DISPLAY_COLS / 8);
                fprintf(logfile, "clear screen\n");
            }
            if (instruction == 0x00ee) { // return
                chip8->sp = (chip8->sp - 1) & (STACK_SIZE - 1);
                chip8->pc = chip8->stack[chip8->sp];
                fprintf(logfile, "return to %x, sp = %x\n", chip8->pc,
                        chip8->sp);
            }
            break;
        case 0x1: // jump
            chip8->pc = addr;
            fprintf(logfile, "jump to %x\n", addr);
            break;
        case 0x2: // call
            chip8->stack[chip8->sp] = chip8->pc;
            chip8->sp = (chip8->sp + 1) & (STACK_SIZE - 1);
            chip8->pc = addr;
            fprintf(logfile, "call subroutine at %x, sp = %x\n", addr,
                    chip8->sp);
            break;
        case 0x3: // skip if equal
            if (chip8->reg[rx] == num) chip8->pc += 2;
            fprintf(logfile, "skip if equal: %x %x\n", chip8->reg[rx], num);
            break;
        case 0x4: // skip if not equal
            if (chip8->reg[rx] != num) chip8->pc += 2;
            fprintf(logfile, "skip if not equal: %x %x\n", chip8->reg[rx], num);
            break;
        case 0x5: // skip if equal (register)
            if (chip8->reg[rx] == chip8->reg[ry]) chip8->pc += 2;
            fprintf(logfile, "skip if equal (registers): %x %x\n",
                    chip8->reg[rx], chip8->reg[ry]);
            break;
        case 0x6: // set reg
            chip8->reg[rx] = num;
            fprintf(logfile, "set reg %x to %x\n", rx, num);
            break;
        case 0x7: // add to reg
            chip8->reg[rx] += num;
            fprintf(logfile, "add %x to reg %x\n", num, rx);
            break;
        case 0x8: // alu
            switch (h) {
                case 0x0:
                    chip8->reg[rx] = chip8->reg[ry];
                    fprintf(logfile, "set reg %x to reg %x\n", rx, ry);
                    break;
                case 0x1:
                    chip8->reg[rx] |= chip8->reg[ry];
                    fprintf(logfile, "or reg %x to reg %x\n", ry, rx);
                    break;
                case 0x2:
                    chip8->reg[rx] &= chip8->reg[ry];
                    fprintf(logfile, "and reg %x to reg %x\n", ry, rx);
                    break;
                case 0x3:
                    chip8->reg[rx] ^= chip8->reg[ry];
                    fprintf(logfile, "xor reg %x to reg %x\n", ry, rx);
                    break;
                case 0x4: {
                    Uint8 bef = chip8->reg[rx];
                    chip8->reg[rx] += chip8->reg[ry];
                    chip8->reg[0xf] = (chip8->reg[rx] < bef) ? 1 : 0;
                    fprintf(logfile, "add reg %x to reg %x\n", ry, rx);
                    break;
                }
                case 0x5: {
                    Uint8 bef = chip8->reg[rx];
                    chip8->reg[rx] -= chip8->reg[ry];
                    chip8->reg[0xf] = (chip8->reg[rx] < bef) ? 1 : 0;
                    fprintf(logfile, "sub reg %x from reg %x\n", ry, rx);
                    break;
                }
                case 0x6:
                    chip8->reg[0xf] = chip8->reg[rx] & 1;
                    chip8->reg[rx] >>= 1;
                    fprintf(logfile, "right shift reg %x\n", rx);
                    break;
                case 0x7:
                    chip8->reg[rx] = chip8->reg[ry] - chip8->reg[rx];
                    chip8->reg[0xf] = (chip8->reg[rx] < chip8->reg[ry]) ? 1 : 0;
                    fprintf(logfile, "reverse sub reg %x from reg %x\n", ry,
                            rx);
                    break;
                case 0xe:
                    chip8->reg[0xf] = (chip8->reg[rx] & (1 << 7)) ? 1 : 0;
                    chip8->reg[rx] <<= 1;
                    fprintf(logfile, "left shift reg %x\n", rx);
                    break;
            }
            break;
        case 0x9: // skip if not equal (register)
            if (chip8->reg[rx] != chip8->reg[ry]) chip8->pc += 2;
            fprintf(logfile, "skip if not equal (registers): %x %x\n",
                    chip8->reg[rx], chip8->reg[ry]);
            break;
        case 0xa: // set index
            chip8->ind = addr;
            fprintf(logfile, "set ind to %x\n", addr);
            break;
        case 0xb: // jump with offset
            chip8->pc = (addr + chip8->reg[0x0]) & (RAM_SIZE - 1);
            fprintf(logfile, "jump to %x with offset %x\n", addr,
                    chip8->reg[0x0]);
            break;
        case 0xc: // random
            chip8->reg[rx] = rand() & num;
            fprintf(logfile, "generate random (%x) in reg %x\n", chip8->reg[rx], rx);
            break;
        case 0xd: // display
            chip8->reg[0xf] = 0;
            Uint8 x = chip8->reg[rx] & (DISPLAY_COLS - 1);
            Uint8 y = chip8->reg[ry] & (DISPLAY_ROWS - 1);
            for (int i = 0;
                 i < h && chip8->ind + i < RAM_SIZE && y + i < DISPLAY_ROWS;
                 i++) {
                Uint64 sprite =
                    (Uint64) reverse_byte(chip8->ram[chip8->ind + i]) << x;
                if (chip8->display[y + i] & sprite) chip8->reg[0xf] = 1;
                chip8->display[y + i] ^= sprite;
            }
            fprintf(logfile,
                    "drew sprite at address %x and size %x to x: %x y: %x\n",
                    chip8->ind, h, x, y);
            break;
        case 0xe:
            if (num == 0x9e) { // skip if key pressed
                if (chip8->keypad & (1 << chip8->reg[rx])) {
                    chip8->pc += 2;
                }
                fprintf(logfile, "skip if key %x pressed\n", chip8->reg[rx]);
            }
            if (num == 0xa1) { // skip if key not pressed
                if (!(chip8->keypad & (1 << chip8->reg[rx]))) {
                    chip8->pc += 2;
                }
                fprintf(logfile, "skip if key %x not pressed\n",
                        chip8->reg[rx]);
            }
            break;
        case 0xf:
            switch (num) {
                case 0x07: // get delay timer
                    chip8->reg[rx] = chip8->delay;
                    fprintf(logfile, "store delay timer (%x) in reg %x\n",
                            chip8->reg[rx], rx);
                    break;
                case 0x15: // set delay timer
                    chip8->delay = chip8->reg[rx];
                    fprintf(logfile, "set delay timer to %x\n", chip8->reg[rx]);
                    break;
                case 0x18: // set sound timer
                    chip8->sound = chip8->reg[rx];
                    fprintf(logfile, "set sound timer to %x\n", chip8->reg[rx]);
                    break;
                case 0x1e: { // add to index
                    Uint16 bef = chip8->ind;
                    chip8->ind = (chip8->ind + chip8->reg[rx]) & (RAM_SIZE - 1);
                    chip8->reg[0xf] = (chip8->ind < bef) ? 1 : 0;
                    fprintf(logfile, "add reg %x to index\n", rx);
                    break;
                }
                case 0x0a: // wait for keypress
                    if (chip8->keypad) {
                        for (int i = 0; i < 16; i++) {
                            if (chip8->keypad & (1 << i)) {
                                chip8->reg[rx] = i;
                            }
                        }
                    } else {
                        chip8->pc -= 2;
                    }
                    fprintf(logfile, "wait for keypress and store in reg %x\n",
                            rx);
                    break;
                case 0x29: // font character
                    chip8->ind = FONT_OFFSET + (chip8->reg[rx] & 0xf);
                    fprintf(logfile, "set index to font character %x\n",
                            chip8->reg[rx] & 0xf);
                    break;
                case 0x33: // bcd conversion
                    chip8->ram[chip8->ind] = chip8->reg[rx] / 100;
                    chip8->ram[(chip8->ind + 1) & (RAM_SIZE - 1)] =
                        chip8->reg[rx] / 10 % 10;
                    chip8->ram[(chip8->ind + 2) & (RAM_SIZE - 1)] =
                        chip8->reg[rx] % 10;
                    fprintf(logfile, "stored bcd of %x at ind\n",
                            chip8->reg[rx]);
                    break;
                case 0x55: // store to memory
                    for (int i = 0; chip8->ind + i < RAM_SIZE && i <= rx; i++) {
                        chip8->ram[chip8->ind + i] = chip8->reg[i];
                    }
                    fprintf(logfile, "stored regs 0 to %x at ind\n", rx);
                    break;
                case 0x65: // load from memory
                    for (int i = 0; chip8->ind + i < RAM_SIZE && i <= rx; i++) {
                        chip8->reg[i] = chip8->ram[chip8->ind + i];
                    }
                    fprintf(logfile, "loaded regs 0 to %x from ind\n", rx);
                    break;
            }
            break;
    }
}

void render_display(struct chip8* chip8, SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 20, 40, 100, 0);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 255, 150, 100, 0);
    for (int y = 0; y < DISPLAY_ROWS; y++) {
        for (int x = 0; x < DISPLAY_COLS; x++) {
            if (chip8->display[y] & (1UL << x)) {
                SDL_RenderFillRect(renderer,
                                   &(SDL_Rect){x * PIXEL_SIZE, y * PIXEL_SIZE,
                                               PIXEL_SIZE, PIXEL_SIZE});
            }
        }
    }
    SDL_RenderPresent(renderer);
}