#include <iostream>
#include <fstream>
#include <iomanip>
#include <utility> // we use only std::swap() from here
#include <bitset>  // for psw register printout

#include "../inc/emulator.h"

/* main program */
int main(int argc, const char *argv[]) {
    // expected format: './emulator <input_file>'
    if (argc < 2) {
        cout << "Input file is not specified." << endl;
        return -1;
    }

    /* emulator object creation and emulation */
    string inputFilePath = argv[1];
    Emulator emulator(inputFilePath);

    if (!emulator.emulate()) {
        emulator.printErrorMessages();
        return -1;
    }

    emulator.memoryDump(); //memory state after the program execution
    return 0;
}

/* constructor */
Emulator::Emulator(string inputPath) : inputFilePath(inputPath), memory(MEMORY_SIZE), registers(NO_REGISTERS) {}

/* emulate() and methods called by it */
bool Emulator::emulate() {
    /* extracting data from the input file */
    if (!fillMemoryFromInputFile()) return false;

    /* registers initialization */
    registers[R_INDEX::pc] = readFromMemory(IVT_ENTRY_PROGRAM_START, WORD); // pc <= IVT[0] - program starting point address
    registers[R_INDEX::sp] = MMAP_REGISTERS_START_ADDRESS;                  // sp points to the last occupied location (initially 0xFF00), and increases downwards
    registers[R_INDEX::psw] = 0x6000;                                       // initial value: [0i 1tl 1tr ... 0n 0c 0o 0z]

    bool running = true; // program execution status
    while (running) {
        cd = {}; // every iteration resets values of the 'command data' structure

        /* stages of the execution of an assembler command */
        if (!commandFetchAndDecode() || !commandExecute(running)) return false;

        /*
        cout << hex;
        cout << "AFTER\n";
        if (cd.mnemonic == MNEMONIC::halt)
            cout << "halt instruction" << endl;
        else cout << "instruction " << cnt << endl;
        cout << "R_DST = " << cd.rDst << " | R_SRC = " << cd.rDst << endl;
        cout << "\nregister states:" << hex << endl;
        for (int i = 0; i < registers.size(); i++)
            cout << "r" << i << " = " << registers[i] << endl;
        cnt++;
        cout << dec;
        */
    }

    /* printout of the final status according to the project (after HALT) */
    if (cd.mnemonic == MNEMONIC::halt) {
        cout << "Emulated processor executed halt instruction" << endl;
    }
    cout << "Emulated processor state: psw=0b";
    bitset<16> x(registers[R_INDEX::psw]);
    cout << x << endl;
    cout << hex;
    for (unsigned i = 0; i < 8; i++) {
        cout << "r" << i << "=0x" << setfill('0') << setw(4) << registers[i];
        if (i == 3) cout << endl;
        else cout << "\t";
    }
    cout << dec << endl;

    return true;
}

bool Emulator::fillMemoryFromInputFile() {
    ifstream file; // input binary file
    unsigned tmp, nOfIterations;

    /* reading a binary input file */
    // cout << "*** Reading input file ***" << endl;

    /* file opening */
    file.open(inputFilePath, ios::binary);
    if (file.fail() || !file.is_open()) {
        emulatingErrors.push_back(inputFilePath + " opening failed.");
        return false;
    }

    /* reading the contents of sections (program segments) */
    file.read((char *)(&nOfIterations), sizeof(nOfIterations)); // the number of "rows" (sections) in the section table

    for (unsigned i = 0; i < nOfIterations; i++) { // reading section by section
        ProgramSegmentData ps;

        /* ps.segmentData (program segment data) */
        file.read((char *)(&tmp), sizeof(tmp)); // program segment data length

        ps.segmentData.resize(tmp);
        file.read((char *)(&ps.segmentData[0]), ps.segmentData.size() * sizeof(ps.segmentData[0]));

        /* ps.baseAddress */
        file.read((char *)(&ps.baseAddress), sizeof(ps.baseAddress));

        /* we load a new segment into the 'memory' array at its address */
        if (ps.baseAddress + ps.segmentData.size() - 1 > MMAP_REGISTERS_START_ADDRESS) {
            emulatingErrors.push_back("Program segment overlaps with memory reserved for registers.");
            return false;
        }
        memory.insert(memory.begin() + ps.baseAddress, ps.segmentData.begin(), ps.segmentData.end());
    }

    /* file closing */
    file.close();

    return true; // everything went well
}

bool Emulator::commandFetchAndDecode() { // fetching and decoding a command
    // cout << "\n*** FETCH & DECODE: ***" << endl;

    /* reading the first byte of the instruction [opcode (4b) | modifier (4b)] */
    short byte = readFromMemory(0xFFFF & registers[R_INDEX::pc], BYTE); // first byte
    char operationCode = (0x0F & byte >> 4);
    char modificator = 0x0F & byte;

    registers[R_INDEX::pc]++;

    // cout << "INSTR : " << (byte & 0xFF) << endl;
    // cout << "OPCODE : " << (0xFF  & operationCode) << " | MOD : " << (0xFF & modificator) << endl;

    /* we proceed further depending on the operation code */
    switch (operationCode) {
        case 0x0: case 0x2: case 0x4:
            if (modificator != 0x0) {
                emulatingErrors.push_back("Wrong command specified modificator for operation code: " + (0xFF & operationCode));
                return false;
            }
            cd.mnemonic = operationCode == 0x0 ? MNEMONIC::halt : (operationCode == 0x2 ? MNEMONIC::iret : MNEMONIC::ret);
            break;
        case 0x3: case 0x5:
            switch (byte) { // byte == (operationCode << 4) | modificator
                case 0x30:
                    cd.mnemonic = MNEMONIC::call; break;
                case 0x50:
                    cd.mnemonic = MNEMONIC::jmp; break;
                case 0x51:
                    cd.mnemonic = MNEMONIC::jeq; break;
                case 0x52:
                    cd.mnemonic = MNEMONIC::jne; break;
                case 0x53:
                    cd.mnemonic = MNEMONIC::jgt; break;
                default: // wrong modificator for the given opcode
                    emulatingErrors.push_back("Wrong command specified modificator for operation code: " + (0xFF & operationCode));
                    return false;
            }

            /* we read the second byte - [rDst (4b) | rSrc (4b)] */
            byte = readFromMemory(0xFFFF & registers[R_INDEX::pc], BYTE);
            cd.rDst = 0x0F & byte >> 4;
            cd.rSrc = 0x0F & byte;

            // cout << "R_DST = " << cd.rDst << " | DST_VAL = " << (0x0F & (byte >> 4)) << endl;

            registers[R_INDEX::pc]++;

            /* we read the third byte - [update (4b) | addressing mode (4b)] */
            byte = readFromMemory(0xFFFF & registers[R_INDEX::pc], BYTE);
            cd.updateType = 0x0F & byte >> 4;
            cd.addressingMode = 0x0F & byte;

            registers[R_INDEX::pc]++;

            if (cd.addressingMode > ADDRESSING_MODE::regdir_disp) {
                emulatingErrors.push_back("Wrong command specified addressing mode: " + cd.addressingMode);
                return false;
            }
            if (cd.updateType != UPDATE_TYPE::no_update) {
                emulatingErrors.push_back("Wrong command specified update type: " + cd.updateType);
                return false;
            }

            // depending on 'cd.addressingMode', the command has 3B [regdir, regind] or 5B [immed, regdir_disp, regind_disp, memdir]
            if (cd.addressingMode != ADDRESSING_MODE::regdir && cd.addressingMode != ADDRESSING_MODE::regind) {
                cd.payload = readFromMemory(0xFFFF & registers[R_INDEX::pc], WORD, BIG_ENDIAN_ORDER);
                registers[R_INDEX::pc] += 2;
            }
            break;
        case 0x1: case 0x6: case 0x7: case 0x8: case 0x9:
            switch (byte) { // byte == (operationCode << 4) | modificator
                case 0x10:
                    cd.mnemonic = MNEMONIC::_int; break;
                case 0x60:
                    cd.mnemonic = MNEMONIC::xchg; break;
                case 0x70:
                    cd.mnemonic = MNEMONIC::add; break;
                case 0x71:
                    cd.mnemonic = MNEMONIC::sub; break;
                case 0x72:
                    cd.mnemonic = MNEMONIC::mul; break;
                case 0x73:
                    cd.mnemonic = MNEMONIC::_div; break;
                case 0x74:
                    cd.mnemonic = MNEMONIC::cmp; break;
                case 0x80:
                    cd.mnemonic = MNEMONIC::_not; break;
                case 0x81:
                    cd.mnemonic = MNEMONIC::_and; break;
                case 0x82:
                    cd.mnemonic = MNEMONIC::_or; break;
                case 0x83:
                    cd.mnemonic = MNEMONIC::_xor; break;
                case 0x84:
                    cd.mnemonic = MNEMONIC::test; break;
                case 0x90:
                    cd.mnemonic = MNEMONIC::shl; break;
                case 0x91:
                    cd.mnemonic = MNEMONIC::shr; break;
                default: // wrong modificator for the given opcode
                    emulatingErrors.push_back("Wrong command specified modificator for operation code: " + (0xFF & operationCode));
                    return false;
            }

            /* citamo drugi (poslednji) bajt - [rDst (4b) | rSrc (4b)] */
            byte = readFromMemory(0xFFFF & registers[R_INDEX::pc], BYTE);
            cd.rDst = 0x0F & byte >> 4;
            cd.rSrc = 0x0F & byte;

            registers[R_INDEX::pc]++;

            if (cd.rDst > R_INDEX::psw
                || (cd.mnemonic != MNEMONIC::_int && cd.mnemonic != MNEMONIC::_not && cd.rSrc > R_INDEX::psw)
                || ((cd.mnemonic == MNEMONIC::_int || cd.mnemonic == MNEMONIC::_not) && cd.rSrc != 0xF)) {
                string errorMessage = "Wrong command specified register indices [rDst = " + cd.rDst;
                errorMessage += ", rSrc = ";
                errorMessage += cd.rSrc + "].";
                emulatingErrors.push_back(errorMessage);
                return false;
            }
            break;
        case 0xA: case 0xB:
            if (modificator != 0x0) {
                emulatingErrors.push_back("Wrong command specified modificator for operation code: " + (0xFF & operationCode));
                return false;
            }
            cd.mnemonic = operationCode == 0xA ? MNEMONIC::ldr_pop : MNEMONIC::str_push;

            /* we read the second byte - [rDst (4b) | rSrc (4b)] */
            byte = readFromMemory(0xFFFF & registers[R_INDEX::pc], BYTE);
            cd.rDst = 0x0F & byte >> 4;
            cd.rSrc = 0x0F & byte;

            // cout << "PC = " << registers[pc] << endl;
            // cout << "R_DST = " << cd.rDst << " | VAL_DST = " << (0x0F & byte) << endl;

            registers[R_INDEX::pc]++;

            if (cd.rDst > R_INDEX::psw) {
                string errorMessage = "Wrong command specified register indices [rDst = " + cd.rDst;
                errorMessage += ", rSrc = ";
                errorMessage += cd.rSrc + "].";
                emulatingErrors.push_back(errorMessage);
                return false;
            }

            /* we read the third byte - [update (4b) | addressing mode (4b)] */
            byte = readFromMemory(0xFFFF & registers[R_INDEX::pc], BYTE);
            cd.updateType = 0x0F & byte >> 4;
            cd.addressingMode = 0x0F & byte;

            registers[R_INDEX::pc]++;

            if (cd.addressingMode > ADDRESSING_MODE::memdir || (cd.mnemonic == MNEMONIC::str_push && cd.addressingMode == ADDRESSING_MODE::immed)) {
                emulatingErrors.push_back("Wrong command specified addressing mode: " + cd.addressingMode);
                return false;
            }

            // depending on 'cd.addressingMode', the command has 3B [regdir, regind] or 5B [immed, regind_disp, memdir]
            if (cd.addressingMode != ADDRESSING_MODE::regdir && cd.addressingMode != ADDRESSING_MODE::regind) {
                cd.payload = readFromMemory(0xFFFF & registers[R_INDEX::pc], WORD, BIG_ENDIAN_ORDER);
                registers[R_INDEX::pc] += 2;
            }
            break;
        default:
            emulatingErrors.push_back("Wrong command operation code: " + (0xFF & operationCode));
            return false;
    }
    return true;
}

bool Emulator::commandExecute(bool &running) { // command execution
    short tmp;

    switch (cd.mnemonic) {
        case MNEMONIC::halt:
            /* stops further execution */
            // cout << "HALT" << endl;
            running = false;
            break;
        case MNEMONIC::_int:
            /* software interrupt (the number of the IVT table entry for which the interrupt request is generated is in 'rDst') */
            // cout << "INT" << endl;

            // [push pc; push psw; pc <= mem[(rDst mod 8)*2]]
            pushOnStack(registers[R_INDEX::pc]);
            pushOnStack(registers[R_INDEX::psw]);
            registers[R_INDEX::pc] = readFromMemory(0xFFFF & (registers[cd.rDst] % 8) * 2, WORD);

            // interrupts should be masked too (but we don't have to because they are not implemented)
            break;
        case MNEMONIC::iret:
            /* return from interrupt routine */
            // cout << "IRET" << endl;

            // [pop psw; pop pc]
            registers[R_INDEX::psw] = popFromStack();
            registers[R_INDEX::pc] = popFromStack();
            break;
        case MNEMONIC::call:
            /* jump to subroutine */
            // cout << "CALL" << endl;

            // [push pc; pc <= operand]
            pushOnStack(registers[R_INDEX::pc]);
            registers[R_INDEX::pc] = getOperand();
            if (emulatingErrors.size() != 0) return false;
            break;
        case MNEMONIC::ret:
            /* return from subroutine */
            // cout << "RET" << endl;

            // [push pc; pc <= operand]
            registers[R_INDEX::pc] = popFromStack();
            break;
        case MNEMONIC::jmp: case MNEMONIC::jeq: case MNEMONIC::jne: case MNEMONIC::jgt:
            /* conditional jump commands (except 'jmp') */
            // cout << "SOME_JUMP_COMMAND" << endl;

            // [pc <= operand]
            if (evaluateJumpCondition()) {
                registers[R_INDEX::pc] = getOperand();
                if (emulatingErrors.size() != 0)
                    return false;
            }
            break;
        case MNEMONIC::xchg:
            /* atomic replacement of rDst and rSrc values (in relation to an asynchronous interrupt request) */
            // cout << "XCHG" << endl;

            swap(registers[cd.rDst], registers[cd.rSrc]);
            break;
        case MNEMONIC::add:
            /* addition of rSrc and rDst */
            // cout << "ADD" << endl;

            registers[cd.rDst] += registers[cd.rSrc];
            break;
        case MNEMONIC::sub:
            /* subtraction of rSrc and rDst */
            // cout << "SUB" << endl;

            registers[cd.rDst] -= registers[cd.rSrc];
            break;
        case MNEMONIC::mul:
            /* multiplication of rSrc and rDst */
            // cout << "MUL" << endl;

            registers[cd.rDst] *= registers[cd.rSrc];
            break;
        case MNEMONIC::_div:
            /* division of rSrc and rDst */
            // cout << "DIV" << endl;

            if (registers[cd.rSrc] == 0) {
                emulatingErrors.push_back("Division with zero is undefined.");
                return false;
            }
            registers[cd.rDst] /= registers[cd.rSrc];
            break;
        case MNEMONIC::cmp:
            /* subtraction of rSrc and rDst and change of PSW flags */
            // cout << "CMP" << endl;

            tmp = registers[cd.rDst] - registers[cd.rSrc];
            updatePswFlags(tmp);
            break;
        case MNEMONIC::_not:
            /* bitwise not of rDst */
            // cout << "NOT" << endl;

            registers[cd.rDst] = ~registers[cd.rDst];
            break;
        case MNEMONIC::_and:
            /* bitwise and of rDst and rSrc */
            // cout << "AND" << endl;

            registers[cd.rDst] &= registers[cd.rSrc];
            break;
        case MNEMONIC::_or:
            /* vrsi bitsko ili nad rDst and rSrc */
            // cout << "OR" << endl;

            registers[cd.rDst] |= registers[cd.rSrc];
            break;
        case MNEMONIC::_xor:
            /* bitwise xor of rDst and rSrc */
            // cout << "XOR" << endl;

            registers[cd.rDst] ^= registers[cd.rSrc];
            break;
        case MNEMONIC::test:
            /* bitwise and of rDst and rSrc and change of PSW flags */
            // cout << "TEST" << endl;

            tmp = registers[cd.rDst] & registers[cd.rSrc];
            updatePswFlags(tmp);
            break;
        case MNEMONIC::shl: case MNEMONIC::shr:
            /* bitwise shift left/right and change of PSW flags */
            // cout << (cd.mnemonic == MNEMONIC::shl ? "SHL" : "SHR") << endl;

            // [rDst <= (rDst << rSrc); update psw] - SHL
            // [rDst <= (rDst >> rSrc); update psw] - SHR
            if (cd.mnemonic == MNEMONIC::shl) tmp = registers[cd.rDst] << registers[cd.rSrc];
            else tmp = registers[cd.rDst] >> registers[cd.rSrc];
            updatePswFlags(tmp);
            registers[cd.rDst] = tmp;
            break;
        case MNEMONIC::ldr_pop:
            /* loads an operand to 'rDst' */
            // cout << "LOAD" << endl;

            // [rDst <= operand]
            registers[cd.rDst] = getOperand();
            if (emulatingErrors.size() != 0) return false;
            updateSource(); // za pop naredbu
            break;
        case MNEMONIC::str_push:
            /* stores 'rDst' to an operand place */
            // cout << "STORE" << endl;

            // [operand <= rDst]
            updateSource(); // za push naredbu
            if (!setOperand()) return false;
            break;
        default:
            emulatingErrors.push_back("Can not proceed executing unknown instruction.");
            return false;
    }
    return true;
}

/* utility methods */
short Emulator::readFromMemory(int startAddress, unsigned nOfBytes, bool littleEndian) {                                                                  // nOfBytes == 1 || nOfBytes == 2
    int lowerByte = memory[startAddress];                          // lower byte
    int higherByte = nOfBytes == WORD ? memory[startAddress + 1] : 0; // higher byte

    if (littleEndian)
        return higherByte << 8 | (0xFF & lowerByte); // 'lowerByte' is the least significant byte
    return lowerByte << 8 | (0xFF & higherByte);     // 'lowerByte' is the most significant byte
}

void Emulator::writeToMemory(int startAddress, unsigned nOfBytes, short value) {
    if (nOfBytes == BYTE) memory[startAddress] = 0xFF & value;
    else {
        /* let's prepare lower and younger byte values for writing */
        char firstByte = 0xFF & value;         // lower byte
        char secondByte = 0xFF & (value >> 8); // higher byte

        /* memory write (always little endian) */
        memory[startAddress] = firstByte;
        memory[startAddress + 1] = secondByte;
    }
}

void Emulator::updateSource() {
    switch (cd.updateType) {
        case UPDATE_TYPE::pre_decrement: case UPDATE_TYPE::post_decrement:
            registers[cd.rSrc] -= 2; break;
        case UPDATE_TYPE::pre_increment: case UPDATE_TYPE::post_increment:
            registers[cd.rSrc] += 2; break;
    }
}

short Emulator::getOperand() {
    switch (cd.addressingMode) {
    case ADDRESSING_MODE::immed:
        return cd.payload;
    case ADDRESSING_MODE::regdir:
        return registers[cd.rSrc];
    case ADDRESSING_MODE::regind:
        return readFromMemory(0xFFFF & registers[cd.rSrc], WORD);
    case ADDRESSING_MODE::regind_disp:
        return readFromMemory(0xFFFF & registers[cd.rSrc] + cd.payload, WORD);
    case ADDRESSING_MODE::memdir:
        return readFromMemory(0xFFFF & cd.payload, WORD);
    case ADDRESSING_MODE::regdir_disp:
        return registers[cd.rSrc] + cd.payload;
    default:
        emulatingErrors.push_back("Unrecognised addressing mode: " + cd.addressingMode);
        return -1;
    }
}

bool Emulator::setOperand() {
    switch (cd.addressingMode) {
        case ADDRESSING_MODE::regdir:
            registers[cd.rSrc] = registers[cd.rDst];
            break;
        case ADDRESSING_MODE::regind:
            writeToMemory(0xFFFF & registers[cd.rSrc], WORD, registers[cd.rDst]);
            break;
        case ADDRESSING_MODE::regind_disp:
            writeToMemory(0xFFFF & registers[cd.rSrc] + cd.payload, WORD, registers[cd.rDst]);
            break;
        case ADDRESSING_MODE::memdir:
            writeToMemory(0xFFFF & cd.payload, WORD, registers[cd.rDst]);
            break;
        default:
            emulatingErrors.push_back("Unrecognised or unsuitable addressing mode: " + cd.addressingMode);
            return false;
    }
    return true;
}

void Emulator::pushOnStack(short value) {
    registers[R_INDEX::sp] -= 2;
    writeToMemory(0xFFFF & registers[R_INDEX::sp], WORD, value);
}

short Emulator::popFromStack() {
    short value = readFromMemory(0xFFFF & registers[R_INDEX::sp], WORD);
    registers[R_INDEX::sp] += 2;
    return value;
}

bool Emulator::evaluateJumpCondition() {
    short condition = registers[R_INDEX::psw] & FLAG_MASK::z; // getting the zero-flag
    switch (cd.mnemonic) {
        case MNEMONIC::jeq:
            return condition;
        case MNEMONIC::jne:
            return !condition;
        case MNEMONIC::jgt:
            condition |= registers[R_INDEX::psw] & FLAG_MASK::o; // getting and adding the overflow-flag
            condition |= registers[R_INDEX::psw] & FLAG_MASK::n; // getting and adding the negative-flag
            return !condition;                                   // if the z, o and n flags are not set
    }
    return true; // cd.mnemonic == MNEMONIC::jmp
}

void Emulator::updatePswFlags(short result) {
    /* let's set the z-flag */
    if (result == 0) registers[R_INDEX::psw] |= FLAG_MASK::z;
    else registers[R_INDEX::psw] &= ~FLAG_MASK::z;

    /* let's set the n-flag */
    if (result < 0) registers[R_INDEX::psw] |= FLAG_MASK::n;
    else registers[R_INDEX::psw] &= ~FLAG_MASK::n;

    /* setting additional flags according to the command */
    short op1, op2;
    switch (cd.mnemonic) {
        case MNEMONIC::cmp:
            /* let's set the carry-flag (for an expression op1 - op2) */
            if (registers[cd.rDst] < registers[cd.rSrc])
                registers[R_INDEX::psw] |= FLAG_MASK::c;
            else registers[R_INDEX::psw] &= ~FLAG_MASK::c;

            /* let's set the o-flag (for an expression op1 - op2) */
            op1 = registers[cd.rDst];
            op2 = registers[cd.rSrc];

            if ((op1 < 0 && op2 > 0 && (op1 - op2) > 0) || (op1 > 0 && op2 < 0 && (op1 - op2) < 0))
                registers[R_INDEX::psw] |= FLAG_MASK::o;
            else registers[R_INDEX::psw] &= ~FLAG_MASK::o;
            break;
        case MNEMONIC::shl:
            /* let's set the c-flag (for an expression op1 << op2) */
            op1 = registers[cd.rDst];
            op2 = registers[cd.rSrc];

            // c-flag is set if at op1 << op2 the last shifted bit is 1
            if ((op1 >> (16 - op2)) & 1) registers[R_INDEX::psw] |= FLAG_MASK::c;
            else registers[R_INDEX::psw] &= ~FLAG_MASK::c;
            break;
        case MNEMONIC::shr:
            /* let's set the c-flag (for an expression op1 >> op2) */
            op1 = registers[cd.rDst];
            op2 = registers[cd.rSrc];

            // c-flag is set if at op1 >> op2 the last shifted bit is 1
            if ((op1 >> (op2 - 1)) & 1) registers[R_INDEX::psw] |= FLAG_MASK::c;
            else registers[R_INDEX::psw] &= ~FLAG_MASK::c;
            break;
    }
}

/* printing methods */
bool Emulator::memoryDump() {                  // printing contents of the memory to a file
    ofstream file; // output text .hex file

    /* file opening */
    file.open("emulator_out_memory_sample.hex");
    if (!file.is_open()) {
        emulatingErrors.push_back("emulator_out_memory_sample.hex opening failed.");
        return false;
    }

    /* writing into a file */
    file << "Memory sample:" << endl;

    unsigned cnt = 0;
    file << hex;
    for (unsigned i = 0; i < memory.size(); i++) {
        if (cnt % 8 == 0 && cnt != 0)
            file << "\n";
        if (cnt % 8 == 0)
            file << setfill('0') << setw(4) << cnt << ": ";
        file << setfill('0') << setw(2) << (0xFF & memory[i]) << " ";

        cnt++;
    }
    file << dec;

    /* file closing */
    file.close();
    return true; // everything went well
}

void Emulator::printErrorMessages() {
    cout << "\n\nEmulating errors:" << endl;
    for (string e : emulatingErrors)
        cout << e << endl;

    cout << "\nUnsuccessful instruction:" << endl;
    cout << "Instruction at: " << registers[R_INDEX::pc] << endl;
    for (int i = 0; i < registers.size(); i++)
        cout << "r" << hex << i << " = " << registers[i] << dec << endl;
}
