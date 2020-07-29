#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include <chrono>
#include <thread>

#include "emulator.h"

const uint8_t font[0x10*5] = {
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

const SDL_Keycode Emulator::KEYMAP[0x10] = {
	SDLK_x,  // 0
	SDLK_1,  // 1
	SDLK_2,  // 2
	SDLK_3,  // 3
	SDLK_q,  // 4
	SDLK_w,  // 5
	SDLK_e,  // 6
	SDLK_a,  // 7
	SDLK_s,  // 8
	SDLK_d,  // 9
	SDLK_z,  // A
	SDLK_c,  // B
	SDLK_4,  // C
	SDLK_r,  // D
	SDLK_f,  // E
	SDLK_v,  // F
};

void error(const char* msg){
	perror(msg);
	exit(EXIT_FAILURE);
}

void error_sdl(const char* msg){
	printf("%s: %s\n", msg, SDL_GetError());
	exit(EXIT_FAILURE);
}

Emulator::Emulator(const char* filename){
	// Init everything
	memset(memory, 0, sizeof(memory));
	memcpy(memory, font, sizeof(font));
	memset(regs, 0, sizeof(regs));
	memset(stack, 0, sizeof(stack));
	I           = 0;
	sp          = 0;
	pc          = 0x200;
	delay_timer = 0;
	sound_timer = 0;
	should_draw = false;
	running     = false;
	keys.reset();
	framebuf.reset();
	init_sdl(basename(filename));
	srand(time(NULL));

	// Load ROM into memory
	load(filename);
}

Emulator::~Emulator(){
	destroy_sdl();
}

void Emulator::init_sdl(const char* game_name){
	// Create window name
	char window_name[32];
	snprintf(window_name, sizeof(window_name), "CHIP-8 Emu: %s", game_name);

	// Init SDL
	memset(&sdl, 0, sizeof(sdl));
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
		error_sdl("SDL_Init");

	// Create window
	sdl.window = SDL_CreateWindow(window_name, SDL_WINDOWPOS_UNDEFINED, 
	                              SDL_WINDOWPOS_UNDEFINED, FRAMEBUF_W*20, 
	                              FRAMEBUF_H*20, SDL_WINDOW_SHOWN);
	if (sdl.window == NULL)
		error_sdl("SDL_CreateWindow");

	// Create renderer
	sdl.renderer = SDL_CreateRenderer(sdl.window, -1, 0);
	SDL_RenderSetLogicalSize(sdl.renderer, FRAMEBUF_W*20, FRAMEBUF_H*20);

	// Create texture that stores frame buffer
	sdl.texture = SDL_CreateTexture(sdl.renderer, SDL_PIXELFORMAT_ARGB8888,
	                                SDL_TEXTUREACCESS_STREAMING, FRAMEBUF_W,
	                                FRAMEBUF_H);

	// Load audio
	SDL_LoadWAV("beep.wav", &sdl.spec, &sdl.audio_buf, &sdl.audio_len);
	if (!sdl.audio_buf || !sdl.audio_len)
		error_sdl("SDL_LoadWAV");

	sdl.audio_dev = SDL_OpenAudioDevice(NULL, 0, &sdl.spec, NULL, 0);
	if (!sdl.audio_dev)
		error_sdl("SDL_OpenAudioDevice");

	SDL_PauseAudioDevice(sdl.audio_dev, 0);
}

void Emulator::destroy_sdl(){
	SDL_DestroyTexture(sdl.texture);
	SDL_DestroyRenderer(sdl.renderer);
	SDL_CloseAudioDevice(sdl.audio_dev);
	SDL_FreeWAV(sdl.audio_buf);
	SDL_DestroyWindow(sdl.window);
	SDL_Quit();
}

void Emulator::load(const char* filename){
	// Open file
	int fd = open(filename, O_RDONLY);
	if (fd == -1)
		error("open");

	// Get file size
	int size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	if (size == -1)
		error("lseek getting file size");
	
	assert(sizeof(memory)-0x200 >= size);

	// Load file content into memory
	if (read(fd, &memory[0x200], size) != size)
		error("read");

	close(fd);
}

bool Emulator::display_sprite(uint16_t addr, uint8_t size, uint8_t x, uint8_t y){
	assert(size <= 15);  // max sprite size is 8x15
	assert(addr <= sizeof(memory)-size);

	bool pixel_erased = false;
	uint8_t c, draw_x, draw_y;
	for (int i = 0; i < size; i++){
		c = memory[addr+i];
		for (int j = 0; j < 8; j++){
			if (c & (1 << (7-j))){
				// Set `pixel_erased` to true if the bit is set, and flip it
				draw_x = (x+j) % FRAMEBUF_W;
				draw_y = (y+i) % FRAMEBUF_H;
				pixel_erased |= framebuf[draw_x + draw_y*FRAMEBUF_W];
				framebuf[draw_x + draw_y*FRAMEBUF_W].flip();
			}
		}
	}
	return pixel_erased;
}

void Emulator::update_timers(){
	if (delay_timer > 0) delay_timer--;
	if (sound_timer > 0 && --sound_timer == 0) // Play sound
	    if (SDL_QueueAudio(sdl.audio_dev, sdl.audio_buf, sdl.audio_len) == -1)
			error_sdl("SDL_QueueAudio");
}

void Emulator::update_screen(){
	// Update screen only when needed
	if (!should_draw)
		return;

	// Get the pixels from the framebuf
	uint32_t pixels[FRAMEBUF_H*FRAMEBUF_W];
	for (int i = 0; i < FRAMEBUF_H*FRAMEBUF_W; i++)
		pixels[i] = (framebuf[i] ? 0xFFFFFFFF : 0xFF000000);
	
	// Draw pixels
	SDL_UpdateTexture(sdl.texture, NULL, pixels, FRAMEBUF_W*sizeof(uint32_t));
	//SDL_RenderClear(sdl.renderer);
	SDL_RenderCopy(sdl.renderer, sdl.texture, NULL, NULL);
	SDL_RenderPresent(sdl.renderer);
	
	// Update flag
	should_draw = false;
}

void Emulator::update_keys(){
	SDL_Event e;
	while (SDL_PollEvent(&e) != 0){
		if (e.type == SDL_QUIT)
			running = false; // exit

		// Keep which keys are pressed and which aren't
		else if (e.type == SDL_KEYDOWN){
			for (int i = 0; i < 16; i++)
				if (e.key.keysym.sym == KEYMAP[i])
					keys[i] = 1;

		} else if (e.type == SDL_KEYUP){
			for (int i = 0; i < 16; i++)
				if (e.key.keysym.sym == KEYMAP[i])
					keys[i] = 0;
		}
	}
}

uint8_t Emulator::wait_for_key_press(){
	while (keys.none() && running)
		update_keys();
	for (int i = 0; i < 16; i++)
		if (keys[i])
			return i;
	
	// We get here if `running` was cleared. Doesn't matter, we're exiting.
	return -1;
}

void Emulator::run_instruction(){
	assert(pc >= 0 && pc < sizeof(memory)-1);
	assert(sp >= 0 && sp < sizeof(stack)/sizeof(stack[0]));

	// Get instruction and opcode
	uint16_t inst   = (memory[pc] << 8) | (memory[pc+1]);
	uint8_t  opcode = (inst & 0xF000) >> 12;

	// Auxiliary values
	uint16_t nnn = inst & 0x0FFF;
	uint8_t  n   = inst & 0x000F;
	uint8_t  kk  = inst & 0x00FF;
	uint8_t  x   = (inst & 0x0F00) >> 8;
	uint8_t  y   = (inst & 0x00F0) >> 4;

	switch (opcode){
		case 0x0:
			switch (kk){
				case 0xE0:
					// 00E0 - CLS
					// Clear the display.
					framebuf.reset();
					break;

				case 0xEE:
					// 00EE - RET
					// Return from a subroutine.
					pc = stack[sp--];
					break;

				default:
					fprintf(stderr, "Unknown inst 0 at 0x%X: 0x%X\n", pc, inst);
					exit(EXIT_FAILURE);
			}
			pc += 2;
			break;

		case 0x1:
			// 1nnn - JP addr
			// Jump to location nnn.
			pc = nnn;
			break;

		case 0x2:
			// 2nnn - CALL addr
			// Call subroutine at nnn.
			stack[++sp] = pc;
			pc = nnn;
			break;

		case 0x3:
			// 3xkk - SE Vx, byte
			// Skip next instruction if Vx = kk.
			pc += (regs[x] == kk ? 4 : 2);
			break;

		case 0x4:
			// 4xkk - SNE Vx, byte
			// Skip next instruction if Vx != kk.
			pc += (regs[x] != kk ? 4 : 2);
			break;
			
		case 0x5:
			// 5xy0 - SE Vx, Vy
			// Skip next instruction if Vx = Vy.
			pc += (regs[x] == regs[y] ? 4 : 2);
			break;

		case 0x6: 
			// 6xkk - LD Vx, byte
			// Set Vx = kk.
			regs[x] = kk;
			pc += 2;
			break;

		case 0x7:
			// 7xkk - ADD Vx, byte
			// Set Vx = Vx + kk.
			regs[x] += kk;
			pc += 2;
			break;

		case 0x8:
			switch (n){
				case 0x0:
					// 8xy0 - LD Vx, Vy
					// Set Vx = Vy.
					regs[x] = regs[y];
					break;
				
				case 0x1:
					// 8xy1 - OR Vx, Vy
					// Set Vx = Vx OR Vy.
					regs[x] |= regs[y];
					break;

				case 0x2:
					// 8xy2 - AND Vx, Vy
					// Set Vx = Vx AND Vy.
					regs[x] &= regs[y];
					break;

				case 0x3:
					// 8xy3 - XOR Vx, Vy
					// Set Vx = Vx XOR Vy.
					regs[x] ^= regs[y];
					break;

				case 0x4:
					// 8xy4 - ADD Vx, Vy
					// Set Vx = Vx ADD Vy.
					regs[0xF] = ((uint16_t)regs[x] + regs[y] > 255);
					regs[x] += regs[y];
					break;

				case 0x5:
					// 8xy5 - SUB Vx, Vy
					// Set Vx = Vx - Vy, set VF = NOT borrow.
					regs[0xF] = (regs[x] >= regs[y]);
					regs[x] -= regs[y];
					break;

				case 0x6:
					// 8xy6 - SHR Vx {, Vy}
					// Set Vx = Vx SHR 1.
					regs[0xF] = regs[x] & 1;
					regs[x] >>= 1;
					break;

				case 0x7:
					// 8xy7 - SUBN Vx, Vy
					// Set Vx = Vy - Vx, set VF = NOT borrow.
					regs[0xF] = (regs[y] >= regs[x]);
					regs[x] = regs[y] - regs[x];
					break;

				case 0xE:
					// 8xyE - SHL Vx {, Vy}
					// Set Vx = Vx SHL 1.
					regs[0xF] = regs[x] >> 7;
					regs[x] <<= 1;
					break;

				default:
					fprintf(stderr, "Unknown inst 8 at 0x%X: 0x%X\n", pc, inst);
					exit(EXIT_FAILURE);
			}
			
			pc += 2;
			break;

		case 0x9:
			// 9xy0 - SNE Vx, Vy
			// Skip next instruction if Vx != Vy.
			pc += (regs[x] != regs[y] ? 4 : 2);
			break;

		case 0xA:
			// Annn - LD I, addr
			// Set I = nnn.
			I = nnn;
			pc += 2;
			break;

		case 0xB:
			// Bnnn - JP V0, addr
			// Jump to location nnn + V0.
			pc = regs[0] + nnn;
			break;

		case 0xC:
			// Cxkk - RND Vx, byte
			// Set Vx = random byte AND kk.
			regs[x] = rand() & kk;
			pc += 2;
			break;

		case 0xD:
			// Dxyn - DRW Vx, Vy, nibble
			// Display n-byte sprite starting at memory location I at (Vx, Vy),
			// set VF = collision.
			should_draw = true;
			regs[0xF] = display_sprite(I, n, regs[x], regs[y]);
			pc += 2;
			break;

		case 0xE:
			switch (kk){
				case 0x9E:
					// Ex9E - SKP Vx
					// Skip next instruction if key with the value of Vx is
					// pressed.
					pc += (keys[regs[x]] ? 4 : 2);
					break;

				case 0xA1:
					// ExA1 - SKNP Vx
					// Skip next instruction if key with the value of Vx is not
					// pressed.
					pc += (!keys[regs[x]] ? 4 : 2);
					break;

				default:
					fprintf(stderr, "Unknown inst E at 0x%X: 0x%X\n", pc, inst);
					exit(EXIT_FAILURE);
			}
			break;

		case 0xF:
			switch (kk){
				case 0x07:
					// Fx07 - LD Vx, DT
					// Set Vx = delay timer value.
					regs[x] = delay_timer;
					break;
				
				case 0x0A:
					// Fx0A - LD Vx, K
					// Wait for a key press, store the value of the key in Vx.
					regs[x] = wait_for_key_press();
					break;

				case 0x15:
					// Fx15 - LD DT, Vx
					// Set delay timer = Vx.
					delay_timer = regs[x];
					break;

				case 0x18:
					// Fx18 - LD ST, Vx
					// Set sound timer = Vx.
					sound_timer = regs[x];
					break;

				case 0x1E:
					// Fx1E - ADD I, Vx
					// Set I = I + Vx. 
					regs[0xF] = ((uint16_t)I + regs[x] > 255);
					I += regs[x];
					break;

				case 0x29:
					// Fx29 - LD F, Vx
					// Set I = location of sprite for digit Vx.
					assert(regs[x] <= 0xF); // last digit is F
					I = regs[x]*5;
					break;

				case 0x33:
					// Fx33 - LD B, Vx
					// Store BCD representation of Vx in memory locations 
					// I, I+1, and I+2.
					assert(I <= sizeof(memory)-3);
					memory[I]   = regs[x] / 100;
					memory[I+1] = (regs[x] / 10) % 10;
					memory[I+2] = (regs[x] % 10);
					break;

				case 0x55:
					// Fx55 - LD [I], Vx
					// Store registers V0 through Vx in memory starting at
					// location I.
					assert(I <= sizeof(memory)-x);
					assert(x <= 15); // last register is V15 (regs[15])
					memcpy(&memory[I], regs, x+1);
					break;

				case 0x65:
					// Fx65 - LD Vx, [I]
					// Read registers V0 through Vx from memory starting at
					// location I.
					assert(I <= sizeof(memory)-x);
					assert(x <= 15); // last register is V15 (regs[15])
					memcpy(regs, &memory[I], x+1);
					break;

				default:
					fprintf(stderr, "Unknown inst F at 0x%X: 0x%X\n", pc, inst);
					exit(EXIT_FAILURE);

			}
			pc += 2;
			break;

		default:
			fprintf(stderr, "Unknown opcode at 0x%X: 0x%X\n", pc, opcode);
			exit(EXIT_FAILURE);
	}
}

void Emulator::run(uint sleep_time){
	running = true;

	// Main loop. Each cycle we update keys state, run a single instruction,
	// update timers and update the screen.
	while (running){
		update_keys();
		run_instruction();
		update_timers();
		update_screen();
		std::this_thread::sleep_for(std::chrono::microseconds(sleep_time));
	}
}