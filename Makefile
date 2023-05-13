chip8emu: chip8.c main.c chip8.h
	gcc -o $@ chip8.c main.c -lSDL2