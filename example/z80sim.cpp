#include "z80sim.h"

using namespace std;

Z80sim::Z80sim(void) : cpu(this)
{

}

Z80sim::~Z80sim() {}

uint8_t Z80sim::fetchOpcode(uint16_t address) {
    // 3 clocks to fetch opcode from RAM and 1 execution clock
    tstates += 4;

#ifdef WITH_BREAKPOINT_SUPPORT
    return z80Ram[address];
#else
    uint8_t opcode = z80Ram[address];
    return (address != 0x0005 ? opcode : breakpoint(address, opcode));
#endif

}

uint8_t Z80sim::peek8(uint16_t address) {
    // 3 clocks for read byte from RAM
    tstates += 3;
    return z80Ram[address];
}

void Z80sim::poke8(uint16_t address, uint8_t value) {
    // 3 clocks for write byte to RAM
    tstates += 3;
    z80Ram[address] = value;
}

uint16_t Z80sim::peek16(uint16_t address) {
    // Order matters, first read lsb, then read msb, don't "optimize"
    uint8_t lsb = peek8(address);
    uint8_t msb = peek8(address + 1);
    return (msb << 8) | lsb;
}

void Z80sim::poke16(uint16_t address, RegisterPair word) {
    // Order matters, first write lsb, then write msb, don't "optimize"
    poke8(address, word.byte8.lo);
    poke8(address + 1, word.byte8.hi);
}

uint8_t Z80sim::inPort(uint16_t port) {
    // 4 clocks for read byte from bus
    tstates += 3;
    return z80Ports[port];
}

void Z80sim::outPort(uint16_t port, uint8_t value) {
    // 4 clocks for write byte to bus
    tstates += 4;
    z80Ports[port] = value;
}

void Z80sim::addressOnBus(uint16_t address, int32_t tstates) {
    // Additional clocks to be added on some instructions
    this->tstates += tstates;
}

void Z80sim::interruptHandlingTime(int32_t tstates) {
    this->tstates += tstates;
}

bool Z80sim::isActiveINT(void) {
	// Put here the needed logic to trigger an INT
    return false;
}

#ifdef WITH_EXEC_DONE
void Z80sim::execDone(void) {}
#endif

uint8_t Z80sim::breakpoint(uint16_t address, uint8_t opcode) {
    // Emulate CP/M Syscall at address 5

#ifdef WITH_BREAKPOINT_SUPPORT
    if (address != 0x0005)
         return opcode;
#endif

    switch (cpu.getRegC()) {
        case 0: // BDOS 0 System Reset
        {
            cout << "Z80 reset after " << tstates << " t-states" << endl;
            finish = true;
            break;
        }
        case 2: // BDOS 2 console char output
        {
            cout << (char) cpu.getRegE();
            break;
        }
        case 9: // BDOS 9 console string output (string terminated by "$")
        {
            uint16_t strAddr = cpu.getRegDE();
            while (z80Ram[strAddr] != '$') {
                cout << (char) z80Ram[strAddr++];
            }
            cout.flush();
            break;
        }
        default:
        {
            cout << "BDOS Call " << cpu.getRegC() << endl;
            finish = true;
            cout << finish << endl;
        }
    }
    // opcode would be modified before the decodeOpcode method
    return opcode;
}

void Z80sim::runTest(std::ifstream* f) {
    streampos size;
    if (!f->is_open()) {
        cout << "file NOT OPEN" << endl;
        return;
    } else cout << "file open" << endl;

    size = f->tellg();
    cout << "Test size: " << size << endl;
    f->seekg(0, ios::beg);
    f->read((char *) &z80Ram[0x100], size);
    f->close();

#ifdef WITH_BREAKPOINT_SUPPORT
    cpu.setBreakpoint(true);
#endif

    cpu.reset();
    finish = false;

    z80Ram[0] = (uint8_t) 0xC3;
    z80Ram[1] = 0x00;
    z80Ram[2] = 0x01; // JP 0x100 CP/M TPA
    z80Ram[5] = (uint8_t) 0xC9; // Return from BDOS call

    while (!finish) {
        cpu.execute();
    }
}

int main(void) {

    Z80sim sim = Z80sim();

    ifstream f1("zexall.bin", ios::in | ios::binary | ios::ate);
    sim.runTest(&f1);
    f1.close();

}
