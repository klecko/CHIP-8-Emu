#include <cstdint>
#include <bitset>
#include <SDL2/SDL.h>

struct SDL_Data {
	SDL_Window*       window;
	SDL_Renderer*     renderer;
	SDL_Texture*      texture;
	SDL_AudioSpec     spec;
	uint32_t          audio_len;
	uint8_t*          audio_buf;
	SDL_AudioDeviceID audio_dev;
};

class Emulator {
	public:
		static const int FRAMEBUF_W = 64;
		static const int FRAMEBUF_H = 32;
		static const SDL_Keycode KEYMAP[0x10];

	private:
		// Memory and stack. Sizes can be changed
		uint8_t  memory[4096];
		uint16_t stack[16];

		// Registers
		uint8_t  regs[16];
		uint16_t I;
		uint8_t  sp; // Index of stack
		uint16_t pc;

		// update_timers
		uint8_t  delay_timer;
		uint8_t  sound_timer;

		// Keys state, bit set means pressed
		std::bitset<0x10> keys;

		// Pixels state, bit set means displayed
		std::bitset<FRAMEBUF_W*FRAMEBUF_H> framebuf;

		// Is the emulator running? Set when run() is called, cleared when
		// emulator window is closed.
		bool running;

		// SDL stuff
		SDL_Data sdl;

		// Initialize SDL stuff
		void init_sdl();

		// Free SDL stuff
		void destroy_sdl();

		// Load a CHIP-8 ROM into memory
		void load(const char* filename);

		// Display the sprite located at `addr` of `size` bytes at `x`, `y` 
		// position. Returns whether there was a collision or not
		bool display_sprite(uint16_t addr, uint8_t size, uint8_t x, uint8_t y);

		// Update timers and play sound if needed
		void update_timers();

		// Draw `framebuf` into the screen and update it
		void update_screen();

		// Update the state of `keys`
		void update_keys();

		// Wait for a key press
		uint8_t wait_for_key_press();

		// Run one instruction
		void run_instruction();

	public:
		// Initialize the emulator state and load the CHIP-8 ROM into memory
		Emulator(const char* filename);

		// Free SDL stuff
		~Emulator();

		// Run the emulator waiting `sleep_time` microseconds between cycles
		void run(uint sleep_time);
};