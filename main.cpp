#include <stdio.h>
#include <SDL2/SDL.h>
#include "emulator.h"

using namespace std;

int main(int argc, char** argv){
	if (argc != 2){
		fprintf(stderr, "Usage: %s romfile\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	printf("Loading %s\n", argv[1]);

	Emulator emu(argv[1]);
	emu.run(3000);
	printf("DONE\n");
}