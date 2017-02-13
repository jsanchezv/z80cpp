# z80cpp
Z80 core in C++

That's a port from Java to C++ of my Z80Core.

To build:

mkdir build

cd build

cmake ..

make

Then, you have an use case at dir 'example'.

The core have the same characteristics of Z80Core:

* Complete instruction set emulation
* Emulates the undocumented bits 3 & 5 from flags register
* Emulates the MEMPTR register (known as WZ at Zilog official doc.)
* Strict execution order for every instruction
* Precise timing for instructions, totally decoupled from the core

jspeccy at gmail dot com
