/* "That means snark shit, Gordon." */
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <array>
#include <fstream>
#include <ios>
#include <iostream>

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <unistd.h>

#include "letters.xbm"
#include "rs_primitives.h"

using namespace std;
using namespace sf;

int main(int argc, char *argv[]) {
	if (argc < 2)
		return 3;

	if (access(argv[1], F_OK) != 0) {
		printf("fail access %i\n", access(argv[1], F_OK));
		return 4;
	}

	/* read rom to memory, stolen from cc8dc */
	u8 memory[0xFFF] = {0};
	FILE* rom = fopen(argv[1], "rb");
	u16 fileData[0xDFF] = {0}; /* dff is max rom size */
	fseek(rom, 0L, SEEK_END);
	int romsize = ftell(rom);
	rewind(rom);
	for (int i = 0; i < romsize; i+=2) {
		u8 readto[2];
		fread(readto, 1, 2, rom);
		fileData[i+1]   = readto[1];
		fileData[i] = readto[0];
	}
	fclose(rom);

	/* load rom */
	for (int i = 0; i < 0xDFF; i++) {
		memory[i+0x200] = fileData[i];
	}

	/* load text */
	for (int i = 0; i < 80; i++) {
		memory[i] = letters_bits[i];
	}

	/* window */
	RenderWindow window(VideoMode(640, 320), "cppc8emu");
	Event event;

	/* av */
	array<bool, 2048> screen = {false};
	RectangleShape square(Vector2f(10, 10));
	square.setFillColor(Color(255, 255, 255));
	square.setOutlineThickness(0);

	/* exec */
	u16 pc = 0x200;
	u8 registers[16];
	u16 address;
	u8 sp;
	u16 stack[0xF];
	u8 dt;
	u8 st;
	bool waitForKey = false;

	const array<Keyboard::Key, 16> keymap = {
			Keyboard::X,
			Keyboard::Num1,
			Keyboard::Num2,
			Keyboard::Num3,
			Keyboard::Q,
			Keyboard::W,
			Keyboard::E,
			Keyboard::A,
			Keyboard::S,
			Keyboard::D,
			Keyboard::Z,
			Keyboard::C,
			Keyboard::Num4,
			Keyboard::R,
			Keyboard::F,
			Keyboard::V
		};

	window.setFramerateLimit(60);
	while (window.isOpen()) {
		while (window.pollEvent(event)) {
			switch (event.type) {
			case Event::Closed:
				window.close();
				break;
			case Event::KeyPressed:
				if (waitForKey){
					for (int i = 0; i < 16; i++) {
						if (event.key.code == keymap[i]){
							registers[memory[pc]&0xF] = i;
							break;
						}
					}
				}

			default:
				break;
			}
		}

		if (!waitForKey) {
			switch (memory[pc] & 0xF0) {
			case 0x00:
				if (memory[pc + 1] == 0xE0) {
					screen.fill(false);
				} else if (memory[pc + 1] == 0xEE) {
					pc = stack[sp -= 2] - 1;
				} else {
					cout << "illegal sys call" << endl;
				}
				break;

			case 0x10:
				pc = ((memory[pc] << 8) | memory[pc + 1])&0x0FFF;
				break;

			case 0x20:
				stack[++sp] = pc;
				pc = ((memory[pc] << 8) | memory[pc + 1])&0x0FFF;
				break;

			case 0x30:
				if (registers[memory[pc]&0xF]==memory[pc+1])
					pc+=2;
				break;

			case 0x40:
				if (registers[memory[pc]&0xF]!=memory[pc+1])
					pc+=2;
				break;

			case 0x50:
				if (registers[memory[pc]&0xF]==(registers[memory[pc+1]&0xF0]>>8))
					pc+=2;
				break;

			case 0x60:
				registers[memory[pc]&0xF] = memory[pc+1];
				break;

			case 0x70:
				registers[memory[pc]&0xF] += memory[pc+1];
				break;

			case 0x08:
				switch (memory[pc+1]&0xF) {
					case 0x0:
						registers[memory[pc]&0xF] = registers[memory[pc+1]&0xF0];
						break;

					case 0x1:
						registers[memory[pc]&0xF] |= registers[memory[pc+1]&0xF0];
						break;

					case 0x2:
						registers[memory[pc]&0xF] &= registers[memory[pc+1]&0xF0];
						break;

					case 0x3:
						registers[memory[pc]&0xF] ^= registers[memory[pc+1]&0xF0];
						break;

					case 0x4:
						registers[memory[pc]&0xF] += registers[memory[pc+1]&0xF0];
						if (registers[memory[pc]&0xF] > 255)
							registers[0xF] = 1;
						break;

					case 0x5:
						registers[memory[pc]&0xF] -= registers[memory[pc+1]&0xF0];
						if (registers[memory[pc]&0xF] > registers[memory[pc+1]&0xF0])
							registers[0xF] = 1;
						break;

					case 0x6:
						registers[memory[pc]&0xF] >>= 1;
						registers[0xF] = registers[memory[pc]&0xF]&1;
						break;

					case 0x7:
						registers[memory[pc]&0xF] = registers[memory[pc+1]&0xF0] - registers[memory[pc]&0xF];
						if (registers[memory[pc]&0xF] < registers[memory[pc+1]&0xF0])
							registers[0xF] = 1;
						break;

					case 0xE:
						registers[memory[pc]&0xF] <<= 1;
						if ((registers[memory[pc]&0xF]&0x80) == 0x80)
							registers[0xF] = 1;
						break;

					default:
						break;
				}
				break;

			case 0x90:
				if (registers[memory[pc]&0xF]!=(registers[memory[pc+1]&0xF0]>>8))
					pc+=2;
				break;

			case 0xA0:
				address = (registers[memory[pc]&0xF] << 8) | registers[memory[pc+1]];
				break;

			case 0xB0:
				pc = (registers[0]+(registers[memory[pc]&0xF] << 8) | registers[memory[pc+1]])-2;
				break;

			case 0xC0:
				registers[memory[pc]&0xF] = min(rand(),255)&registers[memory[pc+1]];
				break;

			case 0xD0:
				for (int i = 1; i < registers[memory[pc+1]&0xF]; i++) {
					int x = registers[memory[pc]&0xF];
					int y = registers[memory[pc+1]&0xF0];
					int pos = x+(y*64);
					screen[pos+0] ^= (memory[address]&0b10000000) > 0;
					screen[pos+1] ^= (memory[address]&0b01000000) > 0;
					screen[pos+2] ^= (memory[address]&0b00100000) > 0;
					screen[pos+3] ^= (memory[address]&0b00010000) > 0;
					screen[pos+4] ^= (memory[address]&0b00001000) > 0;
					screen[pos+5] ^= (memory[address]&0b00000100) > 0;
					screen[pos+6] ^= (memory[address]&0b00000010) > 0;
					screen[pos+7] ^= (memory[address]&0b00000001) > 0;
				}
				break;

			case 0xE0:
				if (memory[pc+1]==0x9E) {
					if(Keyboard::isKeyPressed(keymap[memory[pc]&0xF]))
						pc+=2;
				} else if (memory[pc+1]==0xA1) {
					if(!Keyboard::isKeyPressed(keymap[memory[pc]&0xF]))
						pc+=2;
				}
				break;

			case 0xF0:
				switch (memory[pc+1]) {
					case 0x07:
						registers[memory[pc]&0xF] = dt;
						break;

					case 0x0A:
						waitForKey = true;
						goto skip;
					
					case 0x15:
						dt = registers[memory[pc]&0xF];
						break;

					case 0x18:
						st = registers[memory[pc]&0xF];
						break;

					case 0x1E:
						address += registers[memory[pc]&0xF];
						break;

					case 0x29:
						address = registers[memory[pc]&0xF]*5;
						break;
					
					case 0x33:
						//TODO:vx to bcd at i
						break;

					case 0x55:
						for (int i = 0; i < (memory[pc]&0xF); i++) {
							memory[address+i] = registers[i];
						}

					case 0x65:
						for (int i = 0; i < (memory[pc]&0xF); i++) {
							registers[i] = memory[address+i];
						}

				default:
					break;
				}
				break;

			default:
				break;
			}
			pc += 2;
			dt -= dt > 0 ? 1 : 0;
			st -= st > 0 ? 1 : 0;
		}
		skip:
		window.clear(Color(0, 0, 0));
		for (int y = 0; y < 32; y++) {
			for (int x = 0; x < 64; x++) {
				if (screen[y * 64 + x]) {
					square.setPosition(x * 10, y * 10);
					window.draw(square);
				}
			}
		}
		window.display();
	}

	return 0;
}