#ifndef CHIP8_H
#define CHIP8_H

#include <SDL2/SDL.h>

#define RAM_SIZE 4096
#define DISPLAY_ROWS 32
#define DISPLAY_COLS 64
#define STACK_SIZE 32

#define FONT_OFFSET 0x050
#define PROG_OFFSET 0x200

#define PIXEL_SIZE 10

struct chip8 {
    Uint8 ram[RAM_SIZE];
    Uint64 display[DISPLAY_ROWS];
    Uint16 pc;
    Uint16 ind;
    Uint16 stack[STACK_SIZE];
    Uint8 sp;
    Uint8 delay;
    Uint8 sound;
    Uint8 reg[16];
    Uint16 keypad;
};

struct chip8* init_chip8(char*);

void run_instruction(struct chip8*);

void render_display(struct chip8*, SDL_Renderer*);

#endif