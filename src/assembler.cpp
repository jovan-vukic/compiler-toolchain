#include <iostream>
#include <fstream>
#include <iomanip>

#include "../inc/assembler.h"
#include "../inc/regexes.h"

unsigned Assembler::nextSymbolID = 0;
unsigned Assembler::nextSectionID = 0;

/* main program */
int main(int argc, const char *argv[]) {
    // expected format: './asembler -o <output_file> <input_file>'
    if (argc < 2) {
        cout << "Files paths are not specified." << endl;
        return -1;
    }
    if ((string)argv[1] == "-o" && argc < 4) {
        cout << "Input file path is not specified." << endl;
        return -1;
    }

    /* creating the 'assembler' object */
    string outputFilePath = (string)argv[1] == "-o" ? argv[2] : "assembler_output_generic.o";
    string inputFilePath = (string)argv[1] == "-o" ? argv[3] : argv[1];
    Assembler assembler(inputFilePath, outputFilePath);

    /* assembling start */
    if (!assembler.assemble()) {
        assembler.printErrorMessages();
        return -1;
    }
    return 0;
}

/* constructor */
Assembler::Assembler(string inputFilePath, string outputFilePath) : inputFilePath(inputFilePath), outputFilePath(outputFilePath), locationCounter(0), errorOccurred(false) {
    // adding section 'UNDEF' with id == 0 to the section and symbol tables
    // 'UNDEF' will contain undefined global symbols
    addSectionSymbol("UNDEF");

    // adding section 'ABS' with id == 1 to the section and symbol tables
    // 'ABS' will contain symbols defined via .equ (not implemented for this assembler)
    addSectionSymbol("ABS");

    currentSection = "";
}

/* assemble() and methods called by it */
bool Assembler::assemble() {
    /* opening and reading the input file */
    if (!readFile()) {
        cout << "Can't open the file " << inputFilePath << "." << endl;
        return false;
    }

    /* assembler pass using the input file vector 'inputFile' */
    if (!assemblePass() || !backpatching()) return false;

    /* printing of text and object files */
    if (!writeTextFile() || !writeBinaryFile()) {
        cout << "Can't open the file " << outputFilePath << " for writing." << endl;
        return false;
    }

    return true;
}

bool Assembler::readFile() {
    ifstream file; // input assembly source code (aka .s file)
    string inputLine;

    /* file opening */
    file.open(inputFilePath);
    if (!file.is_open()) return false;

    /* reading the input file line by line */
    unsigned inputFileLineNumber = 0;  // effectively starts at 1
    inputFileLineNumbers.push_back(0); // we will count only from currentLine == 1
    while (getline(file, inputLine)) {
        inputFileLineNumber++;
        inputLine = regex_replace(inputLine, commentsRegex, "$1", regex_constants::format_first_only);
        inputLine = regex_replace(inputLine, tabsRegex, " ");

        inputLine = regex_replace(inputLine, extraSpacesRegex, " ");       // let's replace all extra spaces with a single space
        inputLine = regex_replace(inputLine, extraBoundsSpacesRegex, "$2"); // let's replace all extra spaces with a single space on both ends
        inputLine = regex_replace(inputLine, commaSpacesRegex, ",");       // '... , ...' -> ','
        inputLine = regex_replace(inputLine, colonSpacesRegex, ":");       // '... : ...' -> ':'

        if (inputLine != " " && inputLine != "") {
            inputFileLineNumbers.push_back(inputFileLineNumber);
            Assembler::inputFile.push_back(inputLine);
        }
    }

    /* file closing */
    file.close();
    return true; // everything went well
}

bool Assembler::assemblePass() {
    // cout << "Pass:\n" << endl;

    currentLine = 0;
    for (string inputLine : inputFile) {
        bool match1 = false, match2 = false;
        smatch matchedLineParts; // match object that contains matched string
        currentLine++;

        // cout << "(" << currentSection << ":" << locationCounter << ")" << inputLine << endl;

        /* label at the beginning of the 'inputLine' */
        if ((match1 = regex_search(inputLine, matchedLineParts, labelRegex)) || (match2 = regex_search(inputLine, matchedLineParts, labelWithInstructionRegex))) {
            string labelName = matchedLineParts.str(1); // let's extract 'labelName' from 'labelName: <instruction>'

            // cout << "RX_label:" << endl;
            // cout << "Find:#" << labelName << endl;

            if (!addSymbol(labelName)) errorOccurred = true;

            /* label followed with an instruction (command or directive) */
            if (match2)
                inputLine = matchedLineParts.str(2); // '<instruction>' part to be processed in one of the following if statements
            else continue;
        }

        /* .extern and .global directives */
        if ((match1 = regex_search(inputLine, matchedLineParts, externDirectiveRegex)) || (match2 = regex_search(inputLine, matchedLineParts, globalDirectiveRegex))) {
            string symbol, symbolList = matchedLineParts.str(1); // '.extern/.global <s1>, ... <sn>' -> '<s1>, ..., <sn>'
            stringstream ss(symbolList);

            // cout << "Extern/Global:" << endl;
            // cout << "Find:#" << symbolList << endl;

            /* we take one symbol at a time from the list and add it to the symbol table */
            while (getline(ss, symbol, ',')) {
                // cout << "#" << symbol << endl;
                if (match1 && !addExternSymbol(symbol)) errorOccurred = true;
                if (match2 && !addGlobalSymbol(symbol)) errorOccurred = true;
            }
            // cout << "###" << endl;
            continue;
        }

        /* .section directive */
        if (regex_search(inputLine, matchedLineParts, sectionDirectiveRegex)) {
            string sectionName = matchedLineParts.str(1); // '.section sectionName' -> 'sectionName'

            // cout << "Section_start:" << endl;
            // cout << "Find:#" << sectionName << endl;

            if (!addSectionSymbol(sectionName)) errorOccurred = true;
            // cout << "###" << endl;
            continue;
        }

        /* .word directive */
        if (regex_search(inputLine, matchedLineParts, wordDirectiveRegex)) {
            string literalOrSymbol, valueList = matchedLineParts.str(1); // '.word <s1/l1>, ..., <sn/ln>' -> '<s1/l1>, ..., <sn/ln>'

            // cout << "WORD:" << endl;
            // cout << "Find:#" << valueList << endl;

            /* we take one symbol/literal at a time from the list and add symbol to the symbol table */
            stringstream ss(valueList);
            while (getline(ss, literalOrSymbol, ',')) {
                // cout << "#" << literalOrSymbol << endl;
                if (!processWordDirective(literalOrSymbol)) errorOccurred = true;
            }
            // cout << "###" << endl;
            continue;
        }

        /* .skip directive */
        if (regex_search(inputLine, matchedLineParts, skipDirectiveRegex)) {
            string literal = matchedLineParts.str(1);

            // cout << "SKIP_found:" << endl;
            // cout << "Find:#" << literal << endl;

            if (!processSkipDirective(literal)) errorOccurred = true;
            // cout << "###" << endl;
            continue;
        }

        /* .end directive */
        if (regex_search(inputLine, matchedLineParts, endDirectiveRegex)) {
            break; // end of an assembler pass
        }

        /* assembler command in the 'inputLine' */
        // cout << "Command_Found:" << endl;
        if (!processCommand(inputLine)) errorOccurred = true;
        // cout << "###" << endl;
    }

    /* closing the last section in the file */
    if (currentSection != "")
        sectionTable[currentSection].length = locationCounter;

    return !errorOccurred;
}

bool Assembler::backpatching() {
    // cout << "\nBackpatching:\n" << endl;
    for (ForwardReferenceTableRecord &record : forwardReferenceTable) {
        auto item = symbolTable.find(record.symbol);

        if (item != symbolTable.end()) { // symbol found in the symbol table
            short fillValue;

            /* we change LC and currentSection for the following calls to make the correct relocation records */
            currentSection = record.section;

            if (record.operation == 'R') { // relative addressing
                locationCounter = record.offset - 3; // record.offset was initialized with 'locationCounter + 3'
                fillValue = relativeAddressing(record.symbol);
            } else { // absolute addressing
                locationCounter = record.offset - (record.isLittleEndian ? 0 : 3);
                fillValue = absoluteAddressing(record.symbol, record.isLittleEndian, record.operation);
                if (record.operation == '-') fillValue = -fillValue;
            }

            /* we modify a 2B of data with the value 'fillValue' now that we have the symbol in the table */
            SectionTableRecord &section = sectionTable[currentSection];
            if (record.isLittleEndian) { // directives use little endian
                section.sectionData[record.offset] = 0xFF & fillValue;
                section.sectionData[record.offset + 1] = 0xFF & (fillValue >> 8);
            } else { // commands use big endian
                section.sectionData[record.offset] = 0xFF & (fillValue >> 8);
                section.sectionData[record.offset + 1] = 0xFF & fillValue;
            }
        } else {
            errorMessages.insert({record.currentLine, "Symbol " + record.symbol + " is not in the symbol table."});
            errorOccurred = true;
        }
    }
    return !errorOccurred;
}

bool Assembler::writeTextFile() {
    ofstream file; // output text .o file

    /* file opening */
    string outputFileName = outputFilePath.substr(0, outputFilePath.size() - 2) + "_text.o";
    file.open(outputFileName);
    if (!file.is_open()) return false;

    /* writing to 'file' */
    file << "Relocatable object file" << endl;

    printSymbolTable(file);     // writing the symbol table
    printSectionTable(file);    // writing the section table
    printSectionData(file);     // writing the sections data
    printRelocationTable(file); // writing the relocation table

    /* file closing */
    file.close();
    return true; // everything went well
}

bool Assembler::writeBinaryFile() {
    ofstream file; // output binary .o file

    /* file opening */
    file.open(outputFilePath, ios::out | ios::binary);
    if (!file.is_open()) return false;

    /* writing the section table */
    unsigned tmp = sectionTable.size(); // the number of "rows" (sections) in the section table
    file.write((char *)(&tmp), sizeof(tmp));

    // linker implementation will have to read sections sorted by the id (not by the name)
    map<int, SectionTableRecord> sectionTableOrderedByID; // map by default sorts elements by the integer key in the ascending order
    for (auto item = sectionTable.begin(); item != sectionTable.end(); item++) {
        SectionTableRecord &section = item->second;
        sectionTableOrderedByID.insert({section.id, section});
    }

    for (auto item = sectionTableOrderedByID.begin(); item != sectionTableOrderedByID.end(); item++) {
        SectionTableRecord &section = item->second;

        /* section.id and section.length */
        file.write((char *)(&section.id), sizeof(section.id));
        file.write((char *)(&section.length), sizeof(section.length));

        /* section.name */
        tmp = section.name.length(); // number of characters (bytes) in the section name

        file.write((char *)(&tmp), sizeof(tmp));
        file.write((char *)section.name.c_str(), tmp);

        /* section.sectionData */
        tmp = section.sectionData.size(); // section data length

        file.write((char *)(&tmp), sizeof(tmp));
        file.write((char *)(&section.sectionData[0]), section.sectionData.size() * sizeof(section.sectionData[0]));
    }

    /* writing the symbol table */
    tmp = symbolTable.size(); // number of "rows" (symbols) in the symbol table
    file.write((char *)(&tmp), sizeof(tmp));

    for (auto item = symbolTable.begin(); item != symbolTable.end(); item++) {
        SymbolTableRecord &symbol = item->second;

        /* symbol.id and symbol.offset */
        file.write((char *)(&symbol.id), sizeof(symbol.id));
        file.write((char *)(&symbol.offset), sizeof(symbol.offset));

        /* symbol.isDefined, symbol.isLocal and symbol.isExtern */
        file.write((char *)(&symbol.isDefined), sizeof(symbol.isDefined));
        file.write((char *)(&symbol.isLocal), sizeof(symbol.isLocal));
        file.write((char *)(&symbol.isExtern), sizeof(symbol.isExtern));

        /* symbol.section */
        tmp = symbol.section.length(); // number of characters (bytes) in the section name

        file.write((char *)(&tmp), sizeof(tmp));
        file.write((char *)symbol.section.c_str(), tmp);

        /* symbol.name */
        tmp = symbol.name.length(); // number of characters (bytes) in the symbol name

        file.write((char *)(&tmp), sizeof(tmp));
        file.write((char *)symbol.name.c_str(), tmp);
    }

    /* writing the relocation table */
    tmp = relocationTable.size(); // number of relocation records in the relocation table
    file.write((char *)(&tmp), sizeof(tmp));

    for (RelocationTableRecord r : relocationTable) {
        /* r.section */
        tmp = r.section.length(); // number of characters (bytes) in the section name

        file.write((char *)(&tmp), sizeof(tmp));
        file.write((char *)r.section.c_str(), tmp);

        /* r.offset */
        file.write((char *)(&r.offset), sizeof(r.offset));

        /* r.type */
        tmp = r.type.length(); // number of characters (bytes) in the relocation type string

        file.write((char *)(&tmp), sizeof(tmp));
        file.write((char *)r.type.c_str(), tmp);

        /* r.symbol */
        tmp = r.symbol.length(); // number of characters (bytes) in the symbol name

        file.write((char *)(&tmp), sizeof(tmp));
        file.write((char *)r.symbol.c_str(), tmp);

        /* r.addend */
        // file.write((char*)(&r.addend), sizeof(r.addend)); // unused
    }

    /* file closing */
    file.close();
    return true; // everything went well
}

/* methods for directives and commands processing - called by assemblePass() */
bool Assembler::addSymbol(string symbolLabel) {
    /* we check if any section is open */
    if (currentSection == "") {
        errorMessages.insert({currentLine, "Symbol as label has to be defined in a section."});
        return false;
    }

    /* we check if the symbol of the same name is already defined */
    auto item = symbolTable.find(symbolLabel);

    if (item != symbolTable.end()) { // symbol found in the symbol table
        SymbolTableRecord &symbol = item->second;
        if (symbol.isDefined || symbol.isExtern) {
            string messageString = symbol.isDefined ? "Symbol is previously defined." : "Symbol with the same name is already imported.";
            errorMessages.insert({currentLine, messageString});
            return false;
        }

        // otherwise the symbol is just mentioned earlier in the code and therefore added to the symbol table
        symbol.isDefined = true; // now we also know the symbol definition
        symbol.offset = locationCounter;
        symbol.section = currentSection;
    }
    else { // we add a new symbol to the symbol table
        SymbolTableRecord symbol;
        symbol.id = nextSymbolID++;
        symbol.isDefined = symbol.isLocal = true;
        symbol.isExtern = false;

        symbol.name = symbolLabel;
        symbol.section = currentSection;
        symbol.offset = locationCounter;

        symbolTable.insert({symbol.name, symbol});
    }
    return true;
}

bool Assembler::addSectionSymbol(string sectionName) { // .section directive
    /* we check if this is the first section to be open */
    if (currentSection != "") {
        sectionTable[currentSection].length = locationCounter;
        // cout << "End section: " << sectionTable[currentSection].length << "-" << locationCounter << endl;
    }

    /* the new section opening */
    locationCounter = 0; // let's reset the 'locationCounter'
    currentSection = sectionName;

    /* we add a new section to the section table */
    SectionTableRecord section;
    section.id = nextSectionID++;
    section.name = sectionName;

    section.length = 0;
    sectionTable.insert({section.name, section});

    /* we add the new section name to the symbol table */
    addSymbol(sectionName); // adds the section as a symbol to the symbol table
                            // the offset for section symbols is always 0
    return true;
}

bool Assembler::addGlobalSymbol(string symbolName) { // .global directive
    /* we check if the symbol of the same name is already defined */
    auto item = symbolTable.find(symbolName);

    if (item != symbolTable.end()) { // symbol found in the symbol table
        SymbolTableRecord &symbol = item->second;
        if (symbol.isExtern) {
            errorMessages.insert({currentLine, "Symbol with the same name has an external definition."});
            return false;
        }

        // otherwise the symbol is (un)defined from earlier
        symbol.isLocal = false;
    }
    else { // we add a new symbol to the symbol table
        SymbolTableRecord symbol;

        symbol.id = nextSymbolID++;
        symbol.isDefined = false; // because we add it to the table encountering '.global symbolName'
        symbol.isLocal = symbol.isExtern = false;

        symbol.name = symbolName;
        symbol.section = "UNDEF"; // because we add it to the table encountering '.global symbolName'
        symbol.offset = 0;

        symbolTable.insert({symbol.name, symbol});
    }
    return true;
}

bool Assembler::addExternSymbol(string symbolName) { // .extern directive
    /* we check if the symbol of the same name is already defined */
    auto item = symbolTable.find(symbolName);

    if (item != symbolTable.end()) { // symbol found in the symbol table
        SymbolTableRecord &symbol = item->second;
        if (symbol.isDefined) { // <=> symbol.isDefined || symbol.isLocal
            errorMessages.insert({currentLine, "Symbol is previously defined locally."});
            return false;
        }

        // otherwise the symbol has only been mentioned before but not defined so far
        symbol.isExtern = true;
    }
    else { // we add a new symbol in the symbol table
        SymbolTableRecord symbol;

        symbol.id = nextSymbolID++;
        symbol.isDefined = false; // because we add it to the table encountering '.extern symbolName'
        symbol.isLocal = false;
        symbol.isExtern = true;

        symbol.name = symbolName;
        symbol.section = "UNDEF"; // because we add it to the table encountering '.extern symbolName'
        symbol.offset = 0;

        symbolTable.insert({symbol.name, symbol});
    }
    return true;
}

bool Assembler::processWordDirective(string literalOrSymbol) { // .word directive
    /* .word directive must be specified within a section */
    if (currentSection == "") {
        errorMessages.insert({currentLine, "Directive .word is not specified within a section."});
        return false;
    }
    // cout << "WORD_pass:" << literalOrSymbol << "->";

    /* the .word argument list can contain literals and symbols; 2B is allocated for all list elements */
    SectionTableRecord &section = sectionTable[currentSection];

    int fillValue;
    if (regex_match(literalOrSymbol, regex("^(" + symbolPattern + ")$"))) { // we are processing a symbol
        /*
            let's allocate 2B for a symbol and in an address field leave:
            - symbol.offset for local symbols leave
            - their value (contained in the symbol.offset) for absolute symbols
            - 0 for global symbols
            - 0 for undefined (extern) symbols
        */
        /*
            - for local, global and extern symbols we create a relocation record;
              these non-absolute symbols before linking have no final value,
              so for the given 'locationCounter' we create a relocation record
            - if the symbol is not in the symbol table, a forward referencing record is created
        */
        fillValue = absoluteAddressing(literalOrSymbol, true, '+'); // isLittleEndian == true (because the .word is a directive)
    } else fillValue = getDecimalFromLiteral(literalOrSymbol); // we are processing a literal

    /* the .word directive allocates 2B filled with the 'fillValue' */
    section.sectionData.push_back(0xFF & fillValue);        // first byte
    section.sectionData.push_back(0xFF & (fillValue >> 8)); // second byte
    locationCounter += 2;
    return true;
}

bool Assembler::processSkipDirective(string literal) { // .skip directive
    /* .skip directive must be specified within a section */
    if (currentSection == "") {
        errorMessages.insert({currentLine, "Directive .skip is not specified within a section."});
        return false;
    }

    /* let's interpret the decimal value from the 'literal' */
    int nOfBytes = getDecimalFromLiteral(literal);

    /* let's allocate space of 'nOfBytes' bytes */
    SectionTableRecord &section = sectionTable[currentSection];

    /* the .skip directive starting from the 'locationCounter' writes 5 bytes of zeros */
    for (int i = 0; i < nOfBytes; i++)
        section.sectionData.push_back(0);
    locationCounter += nOfBytes; // locationCounter is updated

    return true;
}

bool Assembler::processCommand(string inputLine) {
    smatch matchedLineParts;
    // cout << "COMMAND_pass:" << endl;

    /* assembler command must be specified within a section */
    if (currentSection == "") {
        errorMessages.insert({currentLine, "Command is not specified within a section. " + inputLine});
        return false;
    }

    /* command without operands (size == 1B) */
    if (regex_search(inputLine, matchedLineParts, regex("^(halt|iret|ret)$"))) {
        string command = matchedLineParts.str(1);
        // cout << "Command_NOP_1B:#" << i;

        /* let's allocate space and fill it properly */
        SectionTableRecord &section = sectionTable[currentSection];
        section.sectionData.push_back(command == "halt" ? 0x00 : (command == "iret" ? 0x20 : 0x40));
        locationCounter++;

        return true;
    }

    /* command with a register as an operand (size == 2B [int, not] || size == 3B [push, pop]) */
    if (regex_search(inputLine, matchedLineParts, regex("^(int|push|pop|not) (r[0-7]|psw)$"))) {
        string command = matchedLineParts.str(1), r = matchedLineParts.str(2);
        // cout << "Command_1OP:#" << i << "#reg:" << r;

        /* let's allocate space and fill it properly */
        char rIndex = r != "psw" ? (r.at(1) - '0') : 8; // r == 'rX' -> X
        SectionTableRecord &section = sectionTable[currentSection];

        if (command == "int" || command == "not") { // size == 2B
            /* int - software interrupt (the number of the IVT table entry for which the interrupt request is generated is in the 'r') */
            /* not - bitwise not */
            section.sectionData.push_back(command == "int" ? 0x10 : 0x80); // first byte
            section.sectionData.push_back(0x0F | (rIndex << 4));     // second byte - _ _ _ _ [reg] | 1 1 1 1
        } else { // command == "push" || command == "pop"; size == 3B
            /* push - places a value from the register in mem16[sp], but before that it executes sp <= sp - 2 (the stack grows downwards) */
            /* pop - loads a value from mem16[sp] into the register, and then executes sp <= sp + 2 (sp points to the last occupied location) */
            section.sectionData.push_back(command == "push" ? 0xB0 : 0xA0); // first byte
            section.sectionData.push_back(0x06 | (rIndex << 4));      // second byte - _ _ _ _ [reg] | 0 1 1 0 [sp]
            section.sectionData.push_back(command == "push" ? 0x12 : 0x42); // third byte - 1 or 4 [1: (sp--) x 2 before; 4: (sp++) x 2 after] | 0 0 1 0 [regind]
        }
        locationCounter += (command == "int" || command == "not" ? 2 : 3);

        return true;
    }

    /* command with two registers as operands (size == 2B) */
    if (regex_search(inputLine, matchedLineParts, regex("^(xchg|add|sub|mul|div|cmp|and|or|xor|test|shl|shr) (r[0-7]|psw),(r[0-7]|psw)$"))) {
        string command = matchedLineParts.str(1), rD = matchedLineParts.str(2), rS = matchedLineParts.str(3);
        // cout << "Command_2OP:#" << i << "#reg1:" << rD << "#reg2:" << rS;

        /* let's allocate space and fill it properly */
        char rDIndex = (rD != "psw" ? (rD.at(1) - '0') : 8), rSIndex = (rS != "psw" ? (rS.at(1) - '0') : 8);
        SectionTableRecord &section = sectionTable[currentSection];

        char tmpValue = 0x60; // command == "xchg"; value of the first byte
        if (command == "add") tmpValue = 0x70;
        else if (command == "sub") tmpValue = 0x71;
        else if (command == "mul") tmpValue = 0x72;
        else if (command == "div") tmpValue = 0x73;
        else if (command == "cmp") tmpValue = 0x74;
        else if (command == "and") tmpValue = 0x81;
        else if (command == "or") tmpValue = 0x82;
        else if (command == "xor") tmpValue = 0x83;
        else if (command == "test") tmpValue = 0x84;
        else if (command == "shl") tmpValue = 0x90;
        else if (command == "shr") tmpValue = 0x91;

        section.sectionData.push_back(tmpValue);                 // first byte
        section.sectionData.push_back(rSIndex | (rDIndex << 4)); // second byte - _ _ _ _ [rDst] | _ _ _ _ [rSrc]

        locationCounter += 2;
        return true;
    }

    /* jump commands (all with one operand) */
    if (regex_search(inputLine, matchedLineParts, regex("^(call|jmp|jeq|jne|jgt) (.*)$"))) {
        string command = matchedLineParts.str(1), operand = matchedLineParts.str(2);
        // cout << "JUMPS_1OP:#" << i << "#op:" << operand;

        /*
            we expect <[LC <= LC + 3 | a command has no payload]> for the following:
                - register direct addressing <-> jmp *rX
                - register indirect addressing <-> jmp *[rX]
            we expect <[LC <= LC + 5 | a command has a payload]> for the following:
                - absolute addressing of symbols and literals <-> jmp <symbol/literal>
                - pc relative symbol addressing <-> jmp %<symbol>
                - register indirect addressing with displacement <-> jmp *[rX +/- <symbol/literal>]
                - memory direct addressing <-> jmp *<symbol/literal>
        */

        /* let's allocate space and fill it properly */
        SectionTableRecord &section = sectionTable[currentSection];

        int tmpValue = (command == "call" ? 0x30 : (command == "jmp" ? 0x50 : (command == "jeq" ? 0x51 : (command == "jne" ? 0x52 : 0x53))));
        section.sectionData.push_back(0xFF & tmpValue); // first byte

        /* register direct addressing <-> jmp *rX */
        if (regex_search(operand, matchedLineParts, regex("^\\*(r[0-7]|psw)$"))) {
            string r = matchedLineParts.str(1);
            char rIndex = r != "psw" ? (r.at(1) - '0') : 8; // r == 'rX' -> X

            section.sectionData.push_back(0xF0 | rIndex); // second byte - 1 1 1 1 | _ _ _ _ [rSrc == operand]
            section.sectionData.push_back(0x01);          // third byte - 0 0 0 0 | 0 0 0 1 [regdir]

            locationCounter += 3;
            return true;
        }

        /* register indirect addressing <-> jmp *[rX] */
        if (regex_search(operand, matchedLineParts, regex("^\\*\\[(r[0-7]|psw)\\]$"))) {
            string r = matchedLineParts.str(1);
            char rIndex = r != "psw" ? (r.at(1) - '0') : 8; // r == 'rX' -> X

            section.sectionData.push_back(0xF0 | rIndex); // second byte - 1 1 1 1 | _ _ _ _ [rSrc == operand]
            section.sectionData.push_back(0x02);          // third byte - 0 0 0 0 | 0 0 1 0 [regind]

            locationCounter += 3;
            return true;
        }

        /* absolute addressing of symbols and literals <-> jmp <symbol/literal> */
        if (regex_search(operand, matchedLineParts, regex("^(" + literalOrSymbolPattern + ")$"))) {
            section.sectionData.push_back(0xFF); // second byte - 1 1 1 1 | 1 1 1 1 [reg; irrelevant, unused]
            section.sectionData.push_back(0x00); // third byte - 0 0 0 0 | 0 0 0 0 [immed]

            /* 4th and 5th byte are the payload - the value that remains in the address field of the instruction */
            if (regex_match(operand, regex("^(" + symbolPattern + ")$"))) // operand == symbol
                tmpValue = absoluteAddressing(operand, false, '+');       // a relocation (or forward referencing) record is also created
            else tmpValue = getDecimalFromLiteral(operand);

            section.sectionData.push_back(0xFF & (tmpValue >> 8)); // fourth byte - most significant byte of the payload
            section.sectionData.push_back(0xFF & tmpValue);        // fifth byte - least significant byte of the payload

            locationCounter += 5;
            return true;
        }

        /* pc relative symbol addressing <-> jmp %<symbol> */
        if (regex_search(operand, matchedLineParts, regex("^%(" + symbolPattern + ")$"))) {
            section.sectionData.push_back(0xF7); // second byte - 1 1 1 1 | 0 1 1 1 [rSrc == PC]
            section.sectionData.push_back(0x05); // third byte - 0 0 0 0 | 0 1 0 1 [regdir with displacement; rSrc == PC]

            /* 4th and 5th byte are the payload - the value that remains in the address field of the instruction */
            tmpValue = relativeAddressing(matchedLineParts.str(1)); // a relocation (or forward referencing) record is also created

            section.sectionData.push_back(0xFF & (tmpValue >> 8)); // fourth byte - most significant byte of the payload
            section.sectionData.push_back(0xFF & tmpValue);        // fifth byte - least significant byte of the payload

            locationCounter += 5;
            return true;
        }

        /* register indirect addressing with displacement <-> jmp *[rX +/- <symbol/literal>] */
        if (regex_search(operand, matchedLineParts, regex("^\\*\\[(r[0-7]|psw) \\+ (" + literalOrSymbolPattern + ")\\]$"))
            || regex_search(operand, matchedLineParts, regex("^\\*\\[(r[0-7]|psw) \\- (" + literalOrSymbolPattern + ")\\]$"))) {
            string reg = matchedLineParts.str(1), displacement = matchedLineParts.str(2);
            char regIndex = reg != "psw" ? (reg.at(1) - '0') : 8; // r == 'rX' -> X

            section.sectionData.push_back(0xF0 | regIndex); // second byte - 1 1 1 1 | _ _ _ _ [reg]
            section.sectionData.push_back(0x03);            // third byte - 0 0 0 0 | 0 0 1 1 [regind with displacement]

            /* 4th and 5th byte are the payload - the value that remains in the address field of the instruction */
            char operation = '+';
            if (regex_search(operand, matchedLineParts, regex("^\\*\\[(r[0-7]|psw) \\- (" + literalOrSymbolPattern + ")\\]$")))
                operation = '-';

            if (regex_match(displacement, regex("^(" + symbolPattern + ")$"))) // operand == symbol
                tmpValue = absoluteAddressing(displacement, false, operation); // a relocation (or forward referencing) record is also created
            else tmpValue = getDecimalFromLiteral((operation == '-' ? "-" : "") + displacement);

            section.sectionData.push_back(0xFF & (tmpValue >> 8)); // fourth byte - most significant byte of the payload
            section.sectionData.push_back(0xFF & tmpValue);        // fifth byte - least significant byte of the payload

            locationCounter += 5;
            return true;
        }

        /* memory direct addressing <-> jmp *<symbol/literal> */
        if (regex_search(operand, matchedLineParts, regex("^\\*(" + literalOrSymbolPattern + ")$"))) {
            section.sectionData.push_back(0xFF); // second byte - 1 1 1 1 | 1 1 1 1 [reg; irrelevant, unused]
            section.sectionData.push_back(0x04); // third byte - 0 0 0 0 | 0 1 0 0 [memdir]

            /* 4th and 5th byte are the payload - the value that remains in the address field of the instruction */
            string literalOrSymbol = matchedLineParts.str(1);                     // removes a '*'
            if (regex_match(literalOrSymbol, regex("^(" + symbolPattern + ")$"))) // operand == symbol
                tmpValue = absoluteAddressing(literalOrSymbol, false, '+');       // a relocation (or forward referencing) record is also created
            else tmpValue = getDecimalFromLiteral(literalOrSymbol);

            section.sectionData.push_back(0xFF & (tmpValue >> 8)); // fourth byte - most significant byte of the payload
            section.sectionData.push_back(0xFF & tmpValue);        // fifth byte - least significant byte of the payload

            locationCounter += 5;
            return true;
        }

        errorMessages.insert({currentLine, "The addressing mode is not supported. " + inputLine});
        return false;
    }

    /* load and store commands */
    if (regex_search(inputLine, matchedLineParts, regex("^(ldr|str) (r[0-7]|psw),(.*)$"))) {
        string command = matchedLineParts.str(1), rD = matchedLineParts.str(2), operand = matchedLineParts.str(3);
        // cout << "LDR/STR_2OP:#" << i << "#reg:" << rD << " #op:" << operand;

        /*
            we expect <[LC <= LC + 3 | a command has no payload]> for the following:
                - register direct addressing <-> ldr <ri>, rX
                - register indirect addressing <-> ldr <ri>, [rX]
            we expect <[LC <= LC + 5 | a command has a payload]> for the following:
                - absolute addressing of symbols and literals <-> jmp <symbol/literal>
                - pc relative symbol addressing <-> ldr <ri>, %<symbol>
                - register indirect addressing with displacement <-> ldr <ri>, [rX +/- <symbol/literal>]
                - memory direct addressing <-> ldr <ri>, <symbol/literal>
        */

        /* let's allocate space and fill it properly */
        char rDIndex = (rD != "psw" ? (rD.at(1) - '0') : 8);
        SectionTableRecord &section = sectionTable[currentSection];

        int tmpValue = (command == "ldr" ? 0xA0 : 0xB0);
        section.sectionData.push_back(0xFF & tmpValue); // first byte

        /* register direct addressing <-> ldr <ri>, rX */
        if (regex_search(operand, matchedLineParts, regex("^(r[0-7]|psw)$"))) {
            string rX = matchedLineParts.str(1);
            char rXIndex = rX != "psw" ? (rX.at(1) - '0') : 8; // r == 'rX' -> X

            section.sectionData.push_back(rXIndex | (rDIndex << 4)); // second byte - _ _ _ _ [rDst] | _ _ _ _ [rSrc]
            section.sectionData.push_back(0x01);                     // third byte - 0 0 0 0 | 0 0 0 1 [regdir]

            locationCounter += 3;
            return true;
        }

        /* register indirect addressing <-> ldr <ri>, [rX] */
        if (regex_search(operand, matchedLineParts, regex("^\\[(r[0-7]|psw)\\]$"))) {
            string rX = matchedLineParts.str(1);
            char rXIndex = rX != "psw" ? (rX.at(1) - '0') : 8; // r == 'rX' -> X

            section.sectionData.push_back(rXIndex | (rDIndex << 4)); // second byte - _ _ _ _ [rDst] | _ _ _ _ [rSrc]
            section.sectionData.push_back(0x02);                     // third byte - 0 0 0 0 | 0 0 1 0 [regind]

            locationCounter += 3;
            return true;
        }

        /* absolute addressing of symbols and literals <-> ldr <ri>, $<symbol/literal> */
        if (regex_search(operand, matchedLineParts, regex("^\\$(" + literalOrSymbolPattern + ")$"))) {
            section.sectionData.push_back(0x0F | (rDIndex << 4)); // second byte - _ _ _ _ [rDst] | 1 1 1 1 [rSrc; irrelevant, unused]
            section.sectionData.push_back(0x00);                  // third byte - 0 0 0 0 | 0 0 0 0 [immed]

            /* 4th and 5th byte are the payload - the value that remains in the address field of the instruction */
            string literalOrSymbol = matchedLineParts.str(1);
            if (regex_match(literalOrSymbol, regex("^(" + symbolPattern + ")$"))) // operand == symbol
                tmpValue = absoluteAddressing(literalOrSymbol, false, '+');       // a relocation (or forward referencing) record is also created
            else tmpValue = getDecimalFromLiteral(literalOrSymbol);

            section.sectionData.push_back(0xFF & (tmpValue >> 8)); // fourth byte - most significant byte of the payload
            section.sectionData.push_back(0xFF & tmpValue);        // fifth byte - least significant byte of the payload

            locationCounter += 5;
            return true;
        }

        /* pc relative symbol addressing <-> ldr <ri>, %<symbol> */
        if (regex_search(operand, matchedLineParts, regex("^%(" + symbolPattern + ")$"))) {
            section.sectionData.push_back(0x07 | (rDIndex << 4)); // second byte - _ _ _ _ [rDst] | 0 1 1 1 [rSrc == PC]
            section.sectionData.push_back(0x03);                  // third byte - 0 0 0 0 | 0 0 1 1 [regind with displacement; rSrc == PC]

            /* 4th and 5th byte are the payload - the value that remains in the address field of the instruction */
            tmpValue = relativeAddressing(matchedLineParts.str(1)); // a relocation (or forward referencing) record is also created
            section.sectionData.push_back(0xFF & (tmpValue >> 8)); // fourth byte - most significant byte of the payload
            section.sectionData.push_back(0xFF & tmpValue);        // fifth byte - least significant byte of the payload

            locationCounter += 5;
            return true;
        }

        /* register indirect addressing with displacement <-> ldr <ri>, [rX +/- <symbol/literal>] */
        if (regex_search(operand, matchedLineParts, regex("^\\[(r[0-7]|psw) \\+ (" + literalOrSymbolPattern + ")\\]$"))
            || regex_search(operand, matchedLineParts, regex("^\\[(r[0-7]|psw) \\- (" + literalOrSymbolPattern + ")\\]$"))) {
            string rX = matchedLineParts.str(1), displacement = matchedLineParts.str(2);
            char rXIndex = rX != "psw" ? (rX.at(1) - '0') : 8; // r == 'rX' -> X

            section.sectionData.push_back(rXIndex | (rDIndex << 4)); // second byte - _ _ _ _ [rDst] | _ _ _ _ [rSrc]
            section.sectionData.push_back(0x03);                     // third byte - 0 0 0 0 | 0 0 1 1 [regind with displacement]

            /* 4th and 5th byte are the payload - the value that remains in the address field of the instruction */
            char operation = '+';
            if (regex_search(operand, matchedLineParts, regex("^\\*\\[(r[0-7]|psw) \\- (" + literalOrSymbolPattern + ")\\]$")))
                operation = '-';

            if (regex_match(displacement, regex("^(" + symbolPattern + ")$"))) // operand == symbol
                tmpValue = absoluteAddressing(displacement, false, operation); // a relocation (or forward referencing) record is also created
            else tmpValue = getDecimalFromLiteral((operation == '-' ? "-" : "") + displacement);

            section.sectionData.push_back(0xFF & (tmpValue >> 8)); // fourth byte - most significant byte of the payload
            section.sectionData.push_back(0xFF & tmpValue);        // fifth byte - least significant byte of the payload

            locationCounter += 5;
            return true;
        }

        /* memory direct addressing <-> ldr <ri>, <symbol/literal> */
        if (regex_search(operand, matchedLineParts, regex("^(" + literalOrSymbolPattern + ")$"))) {
            section.sectionData.push_back(0x0F | (rDIndex << 4)); // second byte - _ _ _ _ [rDst] | 1 1 1 1 [rSrc; irrelevant, unused]
            section.sectionData.push_back(0x04);                  // third byte - 0 0 0 0 | 0 1 0 0 [memdir]

            /* 4th and 5th byte are the payload - the value that remains in the address field of the instruction */
            if (regex_match(operand, regex("^(" + symbolPattern + ")$"))) // operand == symbol
                tmpValue = absoluteAddressing(operand, false, '+');       // a relocation (or forward referencing) record is also created
            else tmpValue = getDecimalFromLiteral(operand);

            section.sectionData.push_back(0xFF & (tmpValue >> 8)); // fourth byte - most significant byte of the payload
            section.sectionData.push_back(0xFF & tmpValue);        // fifth byte - least significant byte of the payload

            locationCounter += 5;
            return true;
        }

        errorMessages.insert({currentLine, "The addressing mode is not supported. " + inputLine});
        return false;
    }

    /* unsupported command */
    errorMessages.insert({currentLine, "The assembler command is not supported. " + inputLine});
    return false;
}

/* utility methods */
int Assembler::getDecimalFromLiteral(string literal) {
    smatch matchedValue;
    int converted;

    if (regex_search(literal, matchedValue, regex("^(" + hexadecimalPattern + ")$")))
        converted = stoi(matchedValue.str(1), nullptr, 16); // hex -> decimal
    if (regex_search(literal, matchedValue, regex("^(" + decimalPattern + ")$")))
        converted = stoi(matchedValue.str(1)); // string -> decimal
    return converted;
}

string Assembler::decimalToHexadecimal(int value) {
    stringstream ss;
    ss << hex << value; // decimal -> hexadecimal

    return ss.str();
}

/* processing of the symbol addressing */
int Assembler::absoluteAddressing(string symbol, bool isLittleEndian, char operation) { // absolute addressing of a symbol in an assembler instruction
    // cout << "ABS_ADDRESSING: " << symbol << endl;

    auto item = symbolTable.find(symbol);
    if (item != symbolTable.end()) { // symbol found in the symbol table
        SymbolTableRecord &symbol = item->second;

        /* symbol is of known absolute value (defined via .equ directive) */
        if (symbol.section == "ABS") {
            // cout << "EQU_Symbol_From_ABS:" << symbol.offset << endl;
            return symbol.offset;
        }

        /* symbol is defined within a section */
        // since there is no final (absolute) value, we create a relocation record
        RelocationTableRecord record;
        record.section = currentSection;
        record.offset = locationCounter + (isLittleEndian ? 0 : 4); // relative to the position of the 1st byte of the instruction +4 takes us to the 'DataLow' byte
        // with commands we want the lower byte of the symbol to be placed at +4, and the older byte at +3 (big endian)

        record.type = isLittleEndian ? "R_HYP_16" : "R_HYP_16_C"; // '_C' at the end suggests a command and big endian is used then
        record.symbol = (!symbol.isLocal || symbol.isExtern ? symbol.name : symbol.section);
        relocationTable.push_back(record);

        return !symbol.isLocal || symbol.isExtern ? 0 : symbol.offset; // we leave this in an address field
    }

    /* symbol is not in the symbol table -> a potential forward referencing or an error */
    ForwardReferenceTableRecord record;
    record.section = currentSection;
    record.offset = locationCounter + (isLittleEndian ? 0 : 3); // for commands 'HighData' byte is at +3 (first byte of the address field)
    record.isLittleEndian = isLittleEndian;                     // with commands we want the lower byte of the symbol to be placed at +4, and the older byte at +3 (big endian)

    record.operation = operation; // '+', '-' (absolute addressing)
    record.symbol = symbol;
    record.currentLine = currentLine; // for error printing purposes after backpatching()

    forwardReferenceTable.push_back(record);
    return 0;
}

int Assembler::relativeAddressing(string symbol) { // relative addressing of a symbol in an assembler command
    // cout << "REL_ADDRESSING: " << symbol << endl;

    auto item = symbolTable.find(symbol);
    if (item != symbolTable.end()) { // symbol found in the symbol table
        SymbolTableRecord &symbol = item->second;

        /* symbol is of known absolute value (defined via .equ directive) */
        if (symbol.section == "ABS") {
            // cout << "EQU_Symbol_From_ABS:" << symbol.offset << endl;
            return symbol.offset + (-2); // addend == -2
        } else if (symbol.isDefined && symbol.section == currentSection) {
            // symbol is either local or global - it has no known absolute value
            // it's defined in the same section as the given command -> their distance is absolute value

            return symbol.offset - (locationCounter + 3) + (-2); // S - P + A
            // S - the offset of the symbol in the (same) section whose value is the jump point
            // P - offset to the first byte of the unresolved address field of this instruction (the 'DataHigh' byte)
            // A - addend in this case is -2 because the unresolved address field is 2B in length
            // (-P + A) <-> PC register value == locationCounter + 5 (points to the next command)
        }

        /* symbol is defined within a section */
        // since there is no final (absolute) value, we create a relocation record
        RelocationTableRecord record;
        record.section = currentSection;
        record.offset = locationCounter + 4; // relative to the position of the 1st byte of the instruction +4 takes us to the 'DataLow' byte
        // with commands we want the lower byte of the symbol to be placed at +4, and the older byte at +3 (big endian)

        record.type = "R_HYP_16_PC_C";
        record.symbol = (!symbol.isLocal || symbol.isExtern ? symbol.name : symbol.section);
        relocationTable.push_back(record);

        // leave -2 in the address field (addend for symbols whose value we don't know yet)
        // or leave symbol.offset - 2 (for an defined local symbol from another section)
        return !symbol.isLocal || symbol.isExtern ? -2 : symbol.offset + (-2);
    }

    /* symbol is not in the symbol table -> a potential forward referencing or an error */
    ForwardReferenceTableRecord record;
    record.section = currentSection;
    record.offset = locationCounter + 3; // for commands 'DataHigh' byte is at +3 (first byte of the address field)
    record.isLittleEndian = false;       // with commands we want the lower byte of the symbol to be placed at +4, and the older byte at +3 (big endian)

    record.operation = 'R'; // 'R' (PC relative addressing)
    record.symbol = symbol;
    record.currentLine = currentLine; // for error printing purposes after backpatching()

    forwardReferenceTable.push_back(record);
    return 0;
}

/* printing methods */
void Assembler::printSymbolTable(ostream &stream) {
    stream << "\n\nSymbol table:" << endl;
    stream << "ID\t\tOffset\tType\tSection\t\tName" << endl;

    stream << hex;
    for (auto item = symbolTable.begin(); item != symbolTable.end(); item++) {
        SymbolTableRecord &symbol = item->second;

        stream << setfill('0') << setw(4) << symbol.id << "\t";
        stream << setfill('0') << setw(4) << symbol.offset << "\t";
        stream << (symbol.isLocal ? "local\t" : (symbol.isDefined ? "global\t" : (symbol.isExtern ? "extern\t" : "undef\t")));
        stream << symbol.section << "\t\t" << symbol.name << endl;
    }
    stream << dec;
}

void Assembler::printSectionTable(ostream &stream) {
    stream << "\n\nSection Table:" << endl;
    stream << "ID\t\tName\t\tLength" << endl;

    stream << hex;
    for (auto item = sectionTable.begin(); item != sectionTable.end(); item++) {
        SectionTableRecord &section = item->second;

        stream << setfill('0') << setw(4) << section.id << "\t" << section.name << (section.name.size() > 3 ? "\t\t" : "\t\t\t");
        stream << setfill('0') << setw(4) << section.length << endl;
    }
    stream << dec;
}

void Assembler::printSectionData(ostream &stream) {
    stream << "\n\nSection Data:" << endl;

    stream << hex;
    for (auto item = sectionTable.begin(); item != sectionTable.end(); item++) {
        SectionTableRecord &section = item->second;
        if (section.length == 0) continue;

        stream << "\nSection: " << section.name;
        for (int i = 0; i < section.sectionData.size(); i++) {
            if (i % 8 == 0) stream << "\n" << setfill('0') << setw(4) << i << ":  ";
            stream << setfill('0') << setw(2) << (0xFF & section.sectionData[i]) << " ";
        }
        stream << endl;
    }
    stream << dec;
}

void Assembler::printRelocationTable(ostream &stream) {
    bool isCommand;
    stream << "\n\nRelocation Table:" << endl;
    stream << "Offset\tType\t\tData/Command\tSymbol\t\tSection name" << endl;

    stream << hex;
    for (RelocationTableRecord r : relocationTable) {
        stream << setfill('0') << setw(4) << r.offset << "\t";

        bool isCommand = r.type.at(r.type.size() - 1) == 'C'; // we check if the last character is a 'C'
        stream << r.type.substr(0, r.type.size() - (isCommand ? 2 : 0)) << "\t";
        stream << (isCommand ? "C" : "D") << "\t\t\t\t" << r.symbol << "\t\t" << r.section << endl;
    }
    stream << dec;
}

void Assembler::printErrorMessages() {
    cout << "\nAssembling & backpatching errors:" << endl;

    for (auto item = errorMessages.begin(); item != errorMessages.end(); item++) {
        cout << "Line: " << inputFileLineNumbers[item->first] << ":" << item->second << endl;
    }
}
