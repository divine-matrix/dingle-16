#ifndef EMULATOR_H
#define EMULATOR_H

#include <cstdint>

class DebugFlags {
    public:
        DebugFlags() {};
        bool debug;
        bool debugRegisters;
        bool debugNonZeroRegisters;
};

void runProgram(const DebugFlags& flags);
void setRAM(uint16_t addr, int16_t value);

#endif