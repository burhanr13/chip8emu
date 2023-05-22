#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "chip8.h"

#define FPS 60
#define IPS 700

FILE* logfile;

void audio_clbk(void* data, Uint8* stream, int len) {
    struct chip8* chip8 = data;
    if (chip8->sound) {
        for (int i = 0; i < len; i++) {
            int x = i % 100;
            if (x > 50) x = 100 - x;
            stream[i] = x;
        }
    } else {
        memset(stream, 0, len);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("give the program filename\n");
        return -1;
    }

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);

    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    SDL_CreateWindowAndRenderer(DISPLAY_COLS * PIXEL_SIZE,
                                DISPLAY_ROWS * PIXEL_SIZE, 0, &window,
                                &renderer);

    srand(time(NULL));

    struct chip8* chip8 = init_chip8(argv[1]);
    if (!chip8) {
        printf("error opening file\n");
        return -1;
    }

    logfile = fopen("log.txt", "w");

    clock_t last_frame_time = clock();
    clock_t last_instr_time = clock();

    SDL_AudioSpec audio = {.freq = 42000,
                           .format = AUDIO_U8,
                           .channels = 1,
                           .samples = 42000 / FPS,
                           .callback = audio_clbk,
                           .userdata = chip8};
    SDL_AudioDeviceID audio_id = SDL_OpenAudioDevice(NULL, 0, &audio, NULL, 0);
    SDL_PauseAudioDevice(audio_id, 0);

    SDL_Scancode keys[] = {
        SDL_SCANCODE_X, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3,
        SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_A,
        SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_Z, SDL_SCANCODE_C,
        SDL_SCANCODE_4, SDL_SCANCODE_R, SDL_SCANCODE_F, SDL_SCANCODE_V};

    int running = 1;
    while (running) {
        clock_t cur_time = clock();
        if (cur_time - last_instr_time >= (CLOCKS_PER_SEC / IPS)) {
            run_instruction(chip8);
            last_instr_time = cur_time;
        }
        cur_time = clock();
        if (cur_time - last_frame_time >= (CLOCKS_PER_SEC / FPS)) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) running = 0;
                if (e.type == SDL_KEYDOWN) {
                    for (int i = 0; i < 16; i++) {
                        if (e.key.keysym.scancode == keys[i]) {
                            chip8->keypad |= (1 << i);
                            break;
                        }
                    }
                }
                if (e.type == SDL_KEYUP) {
                    for (int i = 0; i < 16; i++) {
                        if (e.key.keysym.scancode == keys[i]) {
                            chip8->keypad &= ~(1 << i);
                            break;
                        }
                    }
                }
            }
            if (chip8->delay) chip8->delay--;
            if (chip8->sound) chip8->sound--;
            render_display(chip8, renderer);
            last_frame_time = cur_time;
        }
    }

    fclose(logfile);

    SDL_PauseAudioDevice(audio_id, 1);
    SDL_CloseAudioDevice(audio_id);

    free(chip8);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();

    return 0;
}