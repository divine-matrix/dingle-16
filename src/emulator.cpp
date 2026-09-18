#include <iostream>
#include <cstdint>
#include <sstream>
#include <limits>
#include <iterator>
#include <getopt.h>

using namespace std;

// === DEBUG FLAGS ===
class DebugFlags {
    public:
        DebugFlags() {}

        bool debug = false;

        bool nonZeroRegisters = false;
        bool debugRegisters = false;
};

// === INSTRUCTION CONSTANTS ===
const uint AND = 0xF;
const uint OR  = 0x1;
const uint NOT = 0x2;
const uint XOR = 0x3;
const uint ADD = 0x4;
const uint SUB = 0x5;
const uint SFL = 0x6;
const uint SFR = 0x7;

const uint LDR = 0x8; // Load register from RAM: Reg[Rx] = RAM[Reg[Ry]]
const uint LDI = 0x9; // Load immediate/address from RAM: Reg[Rx] = RAM[PC++]
const uint STR = 0xA; // Store register to RAM: RAM[Reg[Rx]] = Reg[Ry]
const uint STI = 0xB; // Store immediate to RAM: RAM[PC++] = Reg[Rx]

const uint JMP = 0xC; // Unconditional Jump: PC = RAM[PC++] (literal 16-bit address)
const uint JMZ = 0xD; // Jump if Zero flag is set: PC = RAM[PC++] if flags[0]
const uint JMN = 0xE; // Jump if Negative flag is set: PC = RAM[PC++] if flags[1]

const uint NOP = 0x0;

bool haltFlag = false;

// Checks overflow for signed 16-bit addition
static bool willAddOverflow(int16_t a, int16_t b) {
    int32_t sum = static_cast<int32_t>(a) + static_cast<int32_t>(b);
    return (sum > std::numeric_limits<int16_t>::max() || sum < std::numeric_limits<int16_t>::min());
}

// Checks overflow for signed 16-bit subtraction
static bool willSubOverflow(int16_t a, int16_t b) {
    int32_t diff = static_cast<int32_t>(a) - static_cast<int32_t>(b);
    return (diff > std::numeric_limits<int16_t>::max() || diff < std::numeric_limits<int16_t>::min());
}

struct reg_t {
    int16_t value;

    reg_t(int16_t v = 0) : value(v) {}
    operator int16_t&() { return value; }
    operator int16_t() const { return value; }
};

struct ram_t {
    int16_t memory[0x8000];
    void clear() {}
} ram;

struct cpu_t {
    reg_t registerFile[16];
    reg_t ir;
    reg_t pc;
    reg_t mar;
    reg_t mdr;
    bool flags[3]; // 0: zero flag, 1: negative flag, 2: overflow flag
} cpu;

static void fetch() {
    cpu.mar = cpu.pc;
    cpu.pc = cpu.pc + 1;

    cpu.mdr = ram.memory[static_cast<uint16_t>(cpu.mar) & 0x7FFF];
    cpu.ir = cpu.mdr;
}

static int16_t fetchNextWord() {
    cpu.mar = cpu.pc;
    cpu.pc = cpu.pc + 1;
    cpu.mdr = ram.memory[static_cast<uint16_t>(cpu.mar) & 0x7FFF];
    return cpu.mdr;
}

static void execute(const DebugFlags& debugFlags) {
    uint16_t raw_ir = static_cast<uint16_t>(cpu.ir);
    uint8_t opcode      = (raw_ir >> 12) & 0xF;
    uint8_t nibble_high = (raw_ir >> 8) & 0xF; // Rx / Target
    uint8_t nibble_mid  = (raw_ir >> 4) & 0xF; // Ry / Source 1
    uint8_t nibble_low  =  raw_ir & 0xF;       // Rz / Source 2

    bool updatesFlags = true;

    if (debugFlags.debug) {
        std::stringstream stream;
        stream << std::hex << static_cast<uint16_t>(cpu.mdr);
        std::cout << "PC: " << cpu.pc << " | Instruction: 0x" << stream.str() << std::endl;
    }

    switch (opcode) {
        case AND:
            cpu.registerFile[nibble_high] = cpu.registerFile[nibble_mid] & cpu.registerFile[nibble_low];
            updatesFlags = true;
            break;
        case OR:
            cpu.registerFile[nibble_high] = cpu.registerFile[nibble_mid] | cpu.registerFile[nibble_low];
            updatesFlags = true;
            break;
        case NOT:
            cpu.registerFile[nibble_high] = ~cpu.registerFile[nibble_mid];
            updatesFlags = true;
            break;
        case XOR:
            cpu.registerFile[nibble_high] = cpu.registerFile[nibble_mid] ^ cpu.registerFile[nibble_low];
            updatesFlags = true;
            break;
        case ADD: {
            int16_t op1 = cpu.registerFile[nibble_mid];
            int16_t op2 = cpu.registerFile[nibble_low];

            cpu.flags[2] = willAddOverflow(op1, op2);
            cpu.registerFile[nibble_high] = static_cast<int16_t>(static_cast<uint16_t>(op1) + static_cast<uint16_t>(op2));
            updatesFlags = true;
            break;
        }
        case SUB: {
            int16_t op1 = cpu.registerFile[nibble_mid];
            int16_t op2 = cpu.registerFile[nibble_low];

            cpu.flags[2] = willSubOverflow(op1, op2);
            cpu.registerFile[nibble_high] = static_cast<int16_t>(static_cast<uint16_t>(op1) - static_cast<uint16_t>(op2));
            updatesFlags = true;
            break;
        }
        case SFL: {
            uint16_t val = static_cast<uint16_t>(cpu.registerFile[nibble_mid]);
            uint16_t shift = static_cast<uint16_t>(cpu.registerFile[nibble_low]) & 0xF;
            cpu.registerFile[nibble_high] = static_cast<int16_t>(val << shift);
            updatesFlags = true;
            break;
        }
        case SFR: {
            uint16_t val = static_cast<uint16_t>(cpu.registerFile[nibble_mid]);
            uint16_t shift = static_cast<uint16_t>(cpu.registerFile[nibble_low]) & 0xF;
            cpu.registerFile[nibble_high] = static_cast<int16_t>(val >> shift);
            updatesFlags = true;
            break;
        }
        case LDR: {
            uint16_t addr = static_cast<uint16_t>(cpu.registerFile[nibble_mid]);
            cpu.registerFile[nibble_high] = ram.memory[addr & 0x7FFF];
            updatesFlags = true;
            break;
        }
        case LDI: {
            // 2-word instruction: Next word contains the target memory address or direct constant
            int16_t immediateValue = fetchNextWord();
            cpu.registerFile[nibble_high] = ram.memory[immediateValue];
            updatesFlags = true;
            break;
        }
        case STR: {
            uint16_t destAddr = static_cast<uint16_t>(cpu.registerFile[nibble_high]);
            ram.memory[destAddr & 0x7FFF] = cpu.registerFile[nibble_mid];
            break;
        }
        case STI: {
            // 2-word instruction: Next word contains the target RAM address
            uint16_t targetAddr = static_cast<uint16_t>(fetchNextWord());
            ram.memory[targetAddr & 0x7FFF] = cpu.registerFile[nibble_high];
            break;
        }
        case JMP: {
            // 2-word instruction: next word is the literal absolute jump target.
            int16_t targetAddr = fetchNextWord();
            cpu.pc = targetAddr;
            updatesFlags = false;
            break;
        }
        case JMZ: {
            // Always consume the second word, whether or not the jump is taken —
            // otherwise a not-taken branch leaves PC pointing at the address word
            // itself, and the next fetch decodes raw data as an instruction.
            int16_t targetAddr = fetchNextWord();
            if (cpu.flags[0]) { // Zero flag set
                cpu.pc = targetAddr;
            }
            updatesFlags = false;
            break;
        }
        case JMN: {
            int16_t targetAddr = fetchNextWord();
            if (cpu.flags[1]) { // Negative flag set
                cpu.pc = targetAddr;
            }
            updatesFlags = false;
            break;
        }
        case NOP:
            haltFlag = true;
            break;
    }

    // Set Zero and Negative flags after arithmetic/logic/load operations
    if (updatesFlags) {
        int16_t res = cpu.registerFile[nibble_high];
        cpu.flags[0] = (res == 0);
        cpu.flags[1] = (res < 0);
    }
}

static void cycle(const DebugFlags& debugFlags) {
    fetch();
    execute(debugFlags);

    if (debugFlags.debug && debugFlags.debugRegisters) {
        cout << "Register File values" << endl;
        for (size_t i = 0; i < size(cpu.registerFile); i++) {
            if (!debugFlags.nonZeroRegisters) {
                cout << i << ": " << cpu.registerFile[i] << endl;
            } else if (cpu.registerFile[i] != 0) {
                cout << "Reg" << i << ": " << cpu.registerFile[i] << endl;
            }
        }
    }

    if (debugFlags.debug) {
        cout << "MAR: " << cpu.mar << endl;
        cout << "MDR: " << cpu.mdr << endl;
    }
    else
    {    }
}

void setRAM(uint16_t addr, int16_t value) {
    ram.memory[addr] = value;
}

void runProgram(const DebugFlags& debugFlags) {
    while (!haltFlag) {
        cycle(debugFlags);
    }
}

int rmain(int argc, char* argv[]) {
    DebugFlags debugFlags;

    enum {
        OPT_DEBUG = 1000,
        OPT_DEBUG_REGISTERS = 1001,
        OPT_DEBUG_NON_ZERO_REGISTERS = 1002
    };

    static struct option long_options[] = {
        {"debug",    no_argument,              0, OPT_DEBUG},
        {"debugRegisters", no_argument,        0, OPT_DEBUG_REGISTERS},
        {"debugNonZeroRegisters", no_argument, 0, OPT_DEBUG_NON_ZERO_REGISTERS},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "", long_options, nullptr)) != -1) {
        switch (opt) {
            case OPT_DEBUG:
                debugFlags.debug = true;
                break;
            case OPT_DEBUG_REGISTERS:
                debugFlags.debugRegisters = true;
                break;
            case OPT_DEBUG_NON_ZERO_REGISTERS:
                debugFlags.nonZeroRegisters = true;
                break;
        }
    }

    // Program
    ram.memory[0x20] = -2;
    ram.memory[0x21] = 1;
    ram.memory[0] = 0x9000;
    ram.memory[1] = 0x0020;
    ram.memory[2] = 0x9100;
    ram.memory[3] = 0x0021;
    // Loop
    ram.memory[4] = 0x4001;
    ram.memory[5] = 0xE000;
    ram.memory[6] = 0x0004;
    
    runProgram(debugFlags);
    return 0;
}