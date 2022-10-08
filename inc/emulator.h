#ifndef EMULATOR_H
#define EMULATOR_H

#include <string>
#include <vector>

using namespace std;

/* memory & registers */
#define MEMORY_SIZE 1 << 16 // 2^16
#define MMAP_REGISTERS_START_ADDRESS 0xFF00
#define NO_REGISTERS 9 // r[0-7] & psw

enum R_INDEX {
    r0,
    r1,
    r2,
    r3,
    r4,
    r5,
    r6,
    r7,
    sp = r6,
    pc = r7,
    psw
};

enum FLAG_MASK {
    z = 1 << 0,
    o = 1 << 1,
    c = 1 << 2,
    n = 1 << 3
};

/* IVT table and its entries */
#define IVT_ENTRY_PROGRAM_START 0
#define IVT_ENTRY_INVALID_INSTRUCTION 1
#define IVT_ENTRY_TIMER 2
#define IVT_ENTRY_TERMINAL 3

/* assembler commands */
enum MNEMONIC {
    halt = 0x00,
    _int = 0x10,
    iret = 0x20,
    call = 0x30,
    ret = 0x40,
    jmp = 0x50,
    jeq,
    jne,
    jgt,
    xchg = 0x60,
    add = 0x70,
    sub,
    mul,
    _div,
    cmp,
    _not = 0x80,
    _and,
    _or,
    _xor,
    test,
    shl = 0x90,
    shr,
    ldr_pop = 0xA0, // ldr and pop have the same first byte
    str_push = 0xB0 // str and push have the same first byte
};

enum UPDATE_TYPE {
    no_update,
    pre_decrement,
    pre_increment,
    post_decrement,
    post_increment
};

enum ADDRESSING_MODE {
    immed,
    regdir,
    regind,
    regind_disp,
    memdir,
    regdir_disp
};

/* additional constants */
#define BYTE 1
#define WORD 2
#define LITTLE_ENDIAN_ORDER true
#define BIG_ENDIAN_ORDER false

class Emulator {
private:
    string inputFilePath;
    vector<string> emulatingErrors;

    /* data structure about the program segment */
    struct ProgramSegmentData {
        vector<char> segmentData; // segment data
        unsigned baseAddress; // proposed location of the program segment in VMEM
    };

    /* elements of the emulated computer system */
    vector<char> memory;     // memory (addressable unit == 1B)
    vector<short> registers; // 8 GPR and psw registers

    /* data about the current command */
    struct CommandData {
        /* first byte - [operation code (4b) | modifier (4b)] */
        short mnemonic; // command mnemonic

        /* second byte - [rDst (4b) | rSrc (4b)] */
        char rDst, rSrc; // destination and source registers

        /* third byte - [update (4b) | addressing mode (4b)] */
        char updateType; // rSrc register update method
        char addressingMode;

        /* fourth and fifth byte (a command payload) */
        short payload; // exists if 'length' == 5
    };
    CommandData cd;

    /* utility methods */
    short readFromMemory(int, unsigned, bool = LITTLE_ENDIAN_ORDER); // up to 2B can be read at one time
    void writeToMemory(int, unsigned, short);

    void updateSource(); // updating rSrc before/after the forming of the address of the operand

    short getOperand(); // operand fetch
    bool setOperand();  // operand set

    void pushOnStack(short); // puts a value on the stack (and updating sp)
    short popFromStack();    // gets a value from the stack (and updates sp)

    bool evaluateJumpCondition(); // returns the result of checking the jump condition
    void updatePswFlags(short);   // sets psw flags

    /* methods called by emulate() */
    bool fillMemoryFromInputFile(); // loads segments into memory

    bool commandFetchAndDecode(); // fetching and decoding a command
    bool commandExecute(bool &);  // command execution

public:
    Emulator(string); // constructor

    bool emulate(); // emulation of program execution on the described system
    
    /* printing methods */
    bool memoryDump();
    void printErrorMessages(); // error printout
};

#endif
