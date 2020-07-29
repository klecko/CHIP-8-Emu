#include <stdio.h>
#include <cstdlib>
#include <cstdint>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

void error(const char* msg){
	perror(msg);
	exit(EXIT_FAILURE);
}

int main(int argc, char** argv){
	if (argc != 2){
		fprintf(stderr, "Usage: %s romfile\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	printf("Loading %s\n", argv[1]);

	// Open file
	int fd = open(argv[1], O_RDONLY);
	if (fd == -1)
		error("open");

	// Get file size
	int size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	if (size == -1)
		error("lseek getting file size");
	printf("File size %d\n", size);

	// Load file content
	uint8_t* memory = new uint8_t[size];
	if (read(fd, memory, size) != size)
		error("read");
	close(fd);
	
	// For each call, record the called address
	bool calls[size/2];
	memset(calls, 0, sizeof(calls));
	for (int pc = 0; pc < size-1; pc += 2){
		uint16_t inst   = (memory[pc] << 8) | (memory[pc+1]);
		uint8_t  opcode = (inst & 0xF000) >> 12;
		uint16_t nnn    = inst & 0x0FFF;
		if (opcode == 0x2)
			calls[(nnn-0x200)/2] = true;
	}

	// Disassemble
	printf("\nSTART:\n");
	char disass[32];
	for (int pc = 0; pc < size-1; pc += 2){
		// Fetch instruction and get opcode
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
						snprintf(disass, sizeof(disass), "cls");
						break;

					case 0xEE:
						// 00EE - RET
						// Return from a subroutine.
						snprintf(disass, sizeof(disass), "ret");
						break;

					default:
						snprintf(disass, sizeof(disass), "Unknown inst 0: 0x%X", inst);
						break;
				}
				break;

			case 0x1:
				// 1nnn - JP   addr
				// Jump to location nnn.
				snprintf(disass, sizeof(disass), "jp    0x%X", nnn);
				break;

			case 0x2:
				// 2nnn - CALL addr
				// Call subroutine at nnn.
				snprintf(disass, sizeof(disass), "call  0x%X", nnn);
				break;

			case 0x3:
				// 3xkk - SE Vx, byte
				// Skip next instruction if Vx = kk.
				snprintf(disass, sizeof(disass), "se    V%d, 0x%X", x, kk);
				break;

			case 0x4:
				// 4xkk - SNE Vx, byte
				// Skip next instruction if Vx != kk.
				snprintf(disass, sizeof(disass), "sne   V%d, 0x%X", x, kk);
				break;
				
			case 0x5:
				// 5xy0 - SE Vx, Vy
				// Skip next instruction if Vx = Vy.
				snprintf(disass, sizeof(disass), "se    V%d, V%d", x, y);
				break;

			case 0x6: 
				// 6xkk - ld    Vx, byte
				// Set Vx = kk.
				snprintf(disass, sizeof(disass), "ld    V%d, 0x%X", x, kk);
				break;

			case 0x7:
				// 7xkk - ADD Vx, byte
				// Set Vx = Vx + kk.
				snprintf(disass, sizeof(disass), "add   V%d, 0x%X", x, kk);
				break;

			case 0x8:
				switch (n){
					case 0x0:
						// 8xy0 - ld    Vx, Vy
						// Set Vx = Vy.
						snprintf(disass, sizeof(disass), "ld    V%d, V%d", x, y);
						break;
					
					case 0x1:
						// 8xy1 - OR Vx, Vy
						// Set Vx = Vx OR Vy.
						snprintf(disass, sizeof(disass), "or    V%d, V%d", x, y);
						break;

					case 0x2:
						// 8xy2 - AND Vx, Vy
						// Set Vx = Vx AND Vy.
						snprintf(disass, sizeof(disass), "and   V%d, V%d", x, y);
						break;

					case 0x3:
						// 8xy3 - XOR Vx, Vy
						// Set Vx = Vx XOR Vy.
						snprintf(disass, sizeof(disass), "xor   V%d, V%d", x, y);
						break;

					case 0x4:
						// 8xy4 - ADD Vx, Vy
						// Set Vx = Vx ADD Vy.
						snprintf(disass, sizeof(disass), "add   V%d, V%d", x, y);
						break;

					case 0x5:
						// 8xy5 - SUB Vx, Vy
						// Set Vx = Vx - Vy, set VF = NOT borrow.
						snprintf(disass, sizeof(disass), "sub   V%d, V%d", x, y);
						break;

					case 0x6:
						// 8xy6 - SHR Vx {, Vy}
						// Set Vx = Vx SHR 1.
						snprintf(disass, sizeof(disass), "shr   V%d {, V%d}", x, y);
						break;

					case 0x7:
						// 8xy7 - SUBN Vx, Vy
						// Set Vx = Vy - Vx, set VF = NOT borrow.
						snprintf(disass, sizeof(disass), "subn  V%d, V%d", x, y);
						break;

					case 0xE:
						// 8xyE - SHL Vx {, Vy}
						// Set Vx = Vx SHL 1.
						snprintf(disass, sizeof(disass), "shl   V%d {, V%d}", x, y);
						break;

					default:
						snprintf(disass, sizeof(disass), "Unknown inst 8: 0x%X", inst);
						break;
				}
				break;

			case 0x9:
				// 9xy0 - SNE Vx, Vy
				// Skip next instruction if Vx != Vy.
				snprintf(disass, sizeof(disass), "sne   V%d, V%d", x, y);
				break;

			case 0xA:
				// Annn - ld    I, addr
				// Set I = nnn.
				snprintf(disass, sizeof(disass), "ld    I, 0x%X", nnn);
				break;

			case 0xB:
				// Bnnn - JP   V0, addr
				// Jump to location nnn + V0.
				snprintf(disass, sizeof(disass), "jp    V0, 0x%X", nnn);
				break;

			case 0xC:
				// Cxkk - RND Vx, byte
				// Set Vx = random byte AND kk.
				snprintf(disass, sizeof(disass), "rnd   V%d, 0x%X", x, kk);
				break;

			case 0xD:
				// Dxyn - DRW Vx, Vy, nibble
				// Display n-byte sprite starting at memory location I at (Vx, Vy),
				// set VF = collision.
				snprintf(disass, sizeof(disass), "drw   V%d, V%d, 0x%X", x, y, n);
				break;

			case 0xE:
				switch (kk){
					case 0x9E:
						// Ex9E - SKP Vx
						// Skip next instruction if key with the value of Vx is
						// pressed.
						snprintf(disass, sizeof(disass), "skp   V%d", x);
						break;

					case 0xA1:
						// ExA1 - SKNP Vx
						// Skip next instruction if key with the value of Vx is not
						// pressed.
						snprintf(disass, sizeof(disass), "sknp  V%d", x);
						break;

					default:
						snprintf(disass, sizeof(disass), "Unknown inst E: 0x%X", inst);
						break;
				}
				break;

			case 0xF:
				switch (kk){
					case 0x07:
						// Fx07 - ld    Vx, DT
						// Set Vx = delay timer value.
						snprintf(disass, sizeof(disass), "ld    V%d, DT", x);
						break;
					
					case 0x0A:
						// Fx0A - ld    Vx, K
						// Wait for a key press, store the value of the key in Vx.
						snprintf(disass, sizeof(disass), "ld    V%d, K", x);
						break;

					case 0x15:
						// Fx15 - ld    DT, Vx
						// Set delay timer = Vx.
						snprintf(disass, sizeof(disass), "ld    DT, V%d", x);
						break;

					case 0x18:
						// Fx18 - ld    ST, Vx
						// Set sound timer = Vx.
						snprintf(disass, sizeof(disass), "ld    ST, V%d", x);
						break;

					case 0x1E:
						// Fx1E - ADD I, Vx
						// Set I = I + Vx. 
						snprintf(disass, sizeof(disass), "add   I, V%d", x);
						break;

					case 0x29:
						// Fx29 - ld    F, Vx
						// Set I = location of sprite for digit Vx.
						snprintf(disass, sizeof(disass), "ld    F, V%d", x);
						break;

					case 0x33:
						// Fx33 - ld    B, Vx
						// Store BCD representation of Vx in memory locations 
						// I, I+1, and I+2.
						snprintf(disass, sizeof(disass), "ld    B, V%d", x);
						break;

					case 0x55:
						// Fx55 - ld    [I], Vx
						// Store registers V0 through Vx in memory starting at
						// location I.
						snprintf(disass, sizeof(disass), "ld    [I], V%d", x);
						break;

					case 0x65:
						// Fx65 - ld    Vx, [I]
						// Read registers V0 through Vx from memory starting at
						// location I.
						snprintf(disass, sizeof(disass), "ld    V%d, [I]", x);
						break;

					default:
						snprintf(disass, sizeof(disass), "Unknown inst F: 0x%X", inst);
						break;
				}
				break;

			default:
				snprintf(disass, sizeof(disass), "Unknown opcode: 0x%X", opcode);
				break;
		}

		if (calls[pc/2])
			printf("\nFUNCTION 0x%X:\n", pc+0x200);
		printf("    %04X:     %04X       %s\n", pc+0x200, inst, disass);
	}

	delete[] memory;
}