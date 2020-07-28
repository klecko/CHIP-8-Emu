cmake_minimum_required(VERSION 3.10)

project(CHIP-8-Emu)

add_executable(chip-8-emu main.cpp emulator.cpp)
target_link_libraries(chip-8-emu SDL2)

add_executable(disass disass.cpp)
