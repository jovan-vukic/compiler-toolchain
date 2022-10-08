#include <iostream>
#include <fstream>
#include <iomanip>
#include <regex>

#include "../inc/linker.h"

/* main program */
int main(int argc, const char *argv[]) {
    // expected format: './linker -hex/-relocatable <-place=<section>@address> -o <output_file> <input_files>'
    // -relocatable & -place are not implemented
    if (argc < 2) {
        cout << "Files paths are not specified." << endl;
        return -1;
    }

    /* variable definitions */
    regex placeOptionRegex("^-place=([a-zA-Z_][a-zA-Z_0-9]*)@(0[xX][0-9A-Fa-f]+)$");
    bool dashOFound = false, hexOutput = false;
    string outputFilePath = "linker_output_generic.o";

    smatch matchedPlaceOptionParts;
    vector<string> inputFiles; // input files paths

    /* reading command line arguments */
    int i = 1;
    string currentArgument;
    while (i < argc) {
        currentArgument = argv[i++];

        if (currentArgument == "-o") dashOFound = true;
        else if (currentArgument == "-hex") hexOutput = true;
        else if (currentArgument == "-relocatable") {
            cout << "-relocatable is not implemented." << endl;
            return -1;
        } else if (regex_search(currentArgument, matchedPlaceOptionParts, placeOptionRegex)) {
            cout << "-place is not implemented." << endl;
            return -1;
        } else if (dashOFound) { // output file path
            outputFilePath = currentArgument;
            dashOFound = false; // this prevents us of entering this branch again
        } else inputFiles.push_back(currentArgument); // input files paths
    }

    /* solving possible errors */
    if (!hexOutput) {
        cout << "Either -relocatable or -hex has to be used." << endl;
        return -1;
    }
    if (inputFiles.size() == 0) {
        cout << "Input files paths are not specified." << endl;
        return -1;
    }

    /* linker object creation and linking */
    Linker linker(inputFiles, outputFilePath);

    if (!linker.link()) {
        linker.printErrorMessages();
        return -1;
    }
    return 0;
}

/* constructor */
Linker::Linker(vector<string> inputFiles, string outputPath) : outputFilePath(outputPath), inputFilesPaths(inputFiles) {}

/* link() and methods called by it */
bool Linker::link() {
    /* extracting data from the input files */
    if (!fillOutputTablesFromInputFiles()) return false;

    if (!resolveExternSymbols() || !setSectionsBaseAddress()) return false;
    resolveRelocations(); // modifies address fields in aggregated sections according to relocation records

    /* output files creation */
    if (!writeHexFile() || !writeBinaryFile()) return false;
    return true;
}

bool Linker::fillOutputTablesFromInputFiles() {
    ifstream file; // input binary object file (.o file)
    unsigned tmp, nOfIterations;

    /* reading binary input files one by one */
    for (string filePath : inputFilesPaths) {
        /* file opening */
        file.open(filePath, ios::binary);
        if (file.fail() || !file.is_open()) {
            linkingErrors.push_back(filePath + " opening failed.");
            return false;
        }

        /* reading the section table */
        file.read((char *)(&nOfIterations), sizeof(nOfIterations)); // the number of "rows" (sections) in the section table

        for (int i = 0; i < nOfIterations; i++) { // reading section by section
            SectionTableRecord section;

            /* section.id and section.length */
            file.read((char *)(&section.id), sizeof(section.id));
            file.read((char *)(&section.length), sizeof(section.length));

            /* section.name */
            file.read((char *)(&tmp), sizeof(tmp)); // number of characters (bytes) in the section name

            section.name.resize(tmp);
            file.read((char *)section.name.c_str(), tmp);

            /* section.sectionData */
            file.read((char *)(&tmp), sizeof(tmp)); // section data length (section.sectionData.size())

            section.sectionData.resize(tmp);
            file.read((char *)(&section.sectionData[0]), section.sectionData.size() * sizeof(section.sectionData[0]));

            addOutputSection(section, filePath);
        }

        /* reading the symbol table */
        file.read((char *)(&nOfIterations), sizeof(nOfIterations)); // number of "rows" (symbols) in the symbol table

        for (int i = 0; i < nOfIterations; i++) { // reading symbol by symbol
            SymbolTableRecord symbol;

            /* symbol.id and symbol.offset */
            file.read((char *)(&symbol.id), sizeof(symbol.id));
            file.read((char *)(&symbol.offset), sizeof(symbol.offset));

            /* symbol.isDefined, symbol.isLocal and symbol.isExtern */
            file.read((char *)(&symbol.isDefined), sizeof(symbol.isDefined));
            file.read((char *)(&symbol.isLocal), sizeof(symbol.isLocal));
            file.read((char *)(&symbol.isExtern), sizeof(symbol.isExtern));

            /* symbol.section */
            file.read((char *)(&tmp), sizeof(tmp)); // number of characters (bytes) in the section name

            symbol.section.resize(tmp);
            file.read((char *)symbol.section.c_str(), tmp);

            /* symbol.name */
            file.read((char *)(&tmp), sizeof(tmp)); // number of characters (bytes) in the symbol name

            symbol.name.resize(tmp);
            file.read((char *)symbol.name.c_str(), tmp);

            symbol.file = filePath;
            addOutputSymbol(symbol);
        }

        /* reading the relocation table */
        file.read((char *)(&nOfIterations), sizeof(nOfIterations)); // number of relocation records in the relocation table

        for (int i = 0; i < nOfIterations; i++) { // reading record by record
            RelocationTableRecord r;

            /* r.section */
            file.read((char *)(&tmp), sizeof(tmp)); // number of characters (bytes) in the section name

            r.section.resize(tmp);
            file.read((char *)r.section.c_str(), tmp);

            /* r.offset */
            file.read((char *)(&r.offset), sizeof(r.offset));

            /* r.type */
            file.read((char *)(&tmp), sizeof(tmp)); // number of characters (bytes) in the relocation type string

            r.type.resize(tmp);
            file.read((char *)r.type.c_str(), tmp);

            /* r.symbol */
            file.read((char *)(&tmp), sizeof(tmp)); // number of characters (bytes) in the symbol name

            r.symbol.resize(tmp);
            file.read((char *)r.symbol.c_str(), tmp);

            /* r.addend */
            // file.read((char*)(&r.addend), sizeof(r.addend)); // unused

            r.file = filePath;
            addOutputRelocation(r);
        }

        /* file closing */
        file.close();
    }

    return true; // everything went well
}

void Linker::addOutputSection(SectionTableRecord &section, string fileName) { // section is not a reference but an object copy
    /* let's add section to the output table and define additional data */
    /*
        if earlier calls added a section of the same name
        to the output 'sectionTable' (from the previous input file),
        we want to aggregate a new section with it
    */
    auto previousSectionIterator = sectionTable.find(section.name); // previously added section of the same name

    /* let's define additional data for a section */
    if (section.name != "UNDEF") { // including: section.name == "ABS"
        InputSectionData a;
        a.name = section.name;

        a.file = fileName;         // input file to which the section belongs
        a.length = section.length; // size of a section from the input file

        /* base address of the section in the aggregated section */
        unsigned previousSectionEnd = previousSectionIterator != sectionTable.end() ? previousSectionIterator->second.length : 0;
        a.baseAddressOfUnaggregatedSection = previousSectionEnd; // end of the previous section of the same name and the beginning of the new one

        InputSectionsData[section.name].insert({fileName, a});
    }

    /* let's add a new section (or aggregate with an existing one, if necessary) */
    if (previousSectionIterator != sectionTable.end()) {
        // let's aggregate the previous section (which already exists in the output table) with this one
        SectionTableRecord &previousSection = previousSectionIterator->second;
        previousSection.length += section.length; // size of the aggregated section

        /* we add the content of this section to the aggregated section corresponding to it in the output table */
        if (previousSection.length != 0)
            previousSection.sectionData.insert(previousSection.sectionData.end(), section.sectionData.begin(), section.sectionData.end());
    } else { // we don't aggregate sections (this section is unique so far)
        section.id = section.name == "UNDEF" ? 0 : (section.name == "ABS" ? 1 : sectionTable.size());

        section.baseAddress = 0; // we will modify this when we add all the sections to the output table
        sectionTable.insert({section.name, section});

        /* let's add a new section to the symbol table */
        SymbolTableRecord symbol;
        symbol.id = section.name == "UNDEF" ? 0 : (section.name == "ABS" ? 1 : symbolTable.size());
        symbol.isDefined = symbol.isLocal = true;
        symbol.isExtern = false;

        symbol.name = symbol.section = section.name;
        symbol.offset = section.baseAddress; // we will modify this when we add all the sections to the output table

        symbol.file = fileName;
        symbolTable.insert({symbol.name, symbol});
    }
}

bool Linker::addOutputSymbol(SymbolTableRecord &symbol) {
    /* we will solve extern symbols later */
    if (symbol.isExtern) {
        externSymbols.push_back(symbol.name);
        return true;
    }

    /* has the symbol already been added (multiple definition of the symbol in different files) */
    if (symbolTable.find(symbol.name) != symbolTable.end()) { // symbol found in the symbol table
        linkingErrors.push_back("Multiple definitions of " + symbol.name + " symbol.");
        return false;
    }

    /* otherwise we should add the symbol to the linker's symbol table */
    symbol.id = symbolTable.size();
    symbolTable.insert({symbol.name, symbol});
    return true;
}

void Linker::addOutputRelocation(RelocationTableRecord &record) {
    relocationTable.push_back(record);
    return;
}

bool Linker::resolveExternSymbols() {
    /* let's check if a defined symbol of the same name is found in another file */
    for (string externSymbol : externSymbols) {
        if (symbolTable.find(externSymbol) == symbolTable.end()) { // not found -> error: unresolved extern symbol
            linkingErrors.push_back("Unresolved definition of " + externSymbol + " symbol.");
            return false;
        }
    }

    return true;
}

bool Linker::setSectionsBaseAddress() {
    // '-place' is not implemented so the first free address to place sections is 0
    unsigned currentSectionVA = 0;

    // order of loading input files into the linker is such that the section of the IVT table is read first
    // it is important to respect the order of the sections, so we order them by 'section.id'
    map<int, string> sectionTableOrderedByID; // map by default sorts elements by the integer key in the ascending order
    for (auto item = sectionTable.begin(); item != sectionTable.end(); item++) {
        SectionTableRecord &section = item->second;
        sectionTableOrderedByID.insert({section.id, section.name});
    }

    for (auto item = sectionTableOrderedByID.begin(); item != sectionTableOrderedByID.end(); item++) {
        SectionTableRecord &section = sectionTable.find(item->second)->second;

        /* 'UNDEF' and 'ABS' sections do not generate content */
        if (section.name == "UNDEF" || section.name == "ABS") continue;

        /* let's define the position of the section in the output file */
        section.baseAddress = currentSectionVA;
        currentSectionVA += section.length;
        if (section.baseAddress >= 0xFF00 && section.baseAddress <= 0xFFFF) { // [0xFF00, 0xFFFF] - for MMAP registers
            linkingErrors.push_back("Section " + section.name + " overlaps with memory reserved for registers.");
            return false;
        }

        /* let's define the position of sections in their aggregated section */
        for (string fileName : inputFilesPaths) {
            auto fileSectionAdditionalData = InputSectionsData[section.name].find(fileName);

            /* let's check if this section exists (has additional data) for the given file 'fileName' */
            if (fileSectionAdditionalData != InputSectionsData[section.name].end()) {
                InputSectionData &a = fileSectionAdditionalData->second; // additional data about 'section' for the given input file

                /*
                    until now we had an offset from the beginning of the aggregated section,
                    and now it becomes an offset from the beginning of the file
                */
                a.baseAddressOfUnaggregatedSection += section.baseAddress;
            }
        }
    }

    /* let's not forget to modify 'symbol.offset' in the symbol table */
    // for sections the offset should be set to 'section.baseAddress'
    // for 'real' symbols the offset is increased by the offset to the non-aggregated section to which they belong
    for (auto item = symbolTable.begin(); item != symbolTable.end(); item++) {
        SymbolTableRecord &symbol = item->second;

        if (symbol.name == symbol.section) symbol.offset = sectionTable.find(symbol.name)->second.baseAddress;
        else if (symbol.section != "ABS") { // symbols in 'ABS' section have an absolute value of 'symbol.offset'
            // for '-relocatable' (all sections are at the starting address 0) only the position of the section in the aggregated section is considered
            // for '-hex' (sections are arranged one after the other) the offset to the non-aggregated section from the beginning of the output file is considered
            symbol.offset += InputSectionsData[symbol.section][symbol.file].baseAddressOfUnaggregatedSection;
        }
    }

    return true;
}

void Linker::resolveRelocations() {
    /* we update the relocation table because of the aggregated sections */
    for (RelocationTableRecord &r : relocationTable) {
        /*
            for 'r.offset' we:
            - add an offset to the base address of the given (non-aggregated) section from the beginning of the file
            - subtract an offset to the aggregated section from the beginning of the file

            essentially 'r.offset':
            - is increased by the length between the beginning of the aggregated section and the given section in it,
            - is the offset from the beginning of the aggregated section (in the output file) to the unresolved field
        */
        r.offset += InputSectionsData[r.section][r.file].baseAddressOfUnaggregatedSection;
        r.offset -= sectionTable[r.section].baseAddress;
    }

    /*
        a relocation record points to the unresolved field, where the value should be:
        - [symbol.offset - patching_place_address - 2] for relative addressing
        - [symbol.offset] for absolute addressing

        a relocation record refers to a local, external or global symbol,
        so we need to add (patching_place_addition):
        - for a local symbol - baseAddress of the section (to which it belongs) in the output file
        - for a global or extern symbol - the symbol.offset value from the output symbol table

        for relative addressing, in addition to this, you also need:
        - for local, global and extern symbols (all cases) - to subtract patching_place_address
    */
    for (int i = 0; i < relocationTable.size(); i++) {
        bool isSymbolLocalWithPatchedSection = false;
        bool isLittleEndian = relocationTable[i].type.at(relocationTable[i].type.size() - 1) != 'C' ? true : false;
        unsigned patchingPlaceAddress = 0; // address of the first byte of an unresolved field FROM THE BEGINNING OF THE OUTPUT FILE
        int patchingPlaceAddition;         // baseAddress of local symbol section or symbol.offset of global/extern symbol - BOTH FROM BEGINNING OF OUTPUT FILE

        /* let's check if the symbol in the relocation record is a section (local symbol in the .s file) */
        if (sectionTable.find(relocationTable[i].symbol) != sectionTable.end()) { // symbol 'r.symbol' is a section
            InputSectionData &a = InputSectionsData.find(relocationTable[i].symbol)->second.find(relocationTable[i].file)->second;

            patchingPlaceAddition = a.baseAddressOfUnaggregatedSection;
        } else patchingPlaceAddition = symbolTable.find(relocationTable[i].symbol)->second.offset; // global or extern symbol

        /* for relative addressing let's calculate patchingPlaceAddress (otherwise it remains 0) */
        if (relocationTable[i].type == "R_HYP_16_PC_C") {
            patchingPlaceAddress = isLittleEndian ? relocationTable[i].offset : relocationTable[i].offset - 1; // offset from the beginning of the aggregated section to the unresolved field
            patchingPlaceAddress += sectionTable.find(relocationTable[i].section)->second.baseAddress;         // an then we add the offset to beginning of aggregated section

            /*
                let's check if 'r.symbol' maybe belongs to the section being edited
                - we do this only for extern symbols defined in the section with the same name as 'r.section',
                  and which thereby became local symbols that are defined in that section
                - for relative addressing the displacement for such symbols is absolute,
                  and then we can delete the record 'r' that contains them [isSymbolLocalWithPatchedSection := true]
            */
            if (symbolTable.find(relocationTable[i].symbol)->second.section == relocationTable[i].section) {
                isSymbolLocalWithPatchedSection = true;
            }
        }

        /* modification of an unresolved addressing field */
        char lByte = sectionTable[relocationTable[i].section].sectionData[relocationTable[i].offset];                             // lower byte
        char hByte = sectionTable[relocationTable[i].section].sectionData[relocationTable[i].offset + (isLittleEndian ? 1 : -1)]; // higher byte

        int finalValue = ((int)lByte | hByte << 8) + patchingPlaceAddition - patchingPlaceAddress;

        sectionTable[relocationTable[i].section].sectionData[relocationTable[i].offset] = 0xFF & finalValue;                                    // lower byte
        sectionTable[relocationTable[i].section].sectionData[relocationTable[i].offset + (isLittleEndian ? 1 : -1)] = 0xFF & (finalValue >> 8); // higher byte

        /* removal of the relocation record, if it refers to the local symbol in the modification section */
        if (isSymbolLocalWithPatchedSection)
            relocationTable.erase(relocationTable.begin() + i--); // don't forget: i--
    }
}

bool Linker::writeHexFile() {
    ofstream file; // output text .hex file

    /* file opening */
    string outputFileName = outputFilePath.substr(0, outputFilePath.size() - 4) + "_text.hex";
    file.open(outputFileName);
    if (!file.is_open()) {
        linkingErrors.push_back(outputFilePath + " opening failed.");
        return false;
    }

    /* writing to the 'file' */
    unsigned cnt = 0;
    file << hex;

    // it is important to follow the sequence of sections when printing to the output file
    // otherwise they would be extracted from the 'sectionTable' map according to the section name (ascending)
    map<int, SectionTableRecord> sectionTableOrderedByID; // map by default sorts elements by the integer key in the ascending order
    for (auto item = sectionTable.begin(); item != sectionTable.end(); item++) {
        SectionTableRecord &section = item->second;
        sectionTableOrderedByID.insert({section.id, section});
    }

    for (auto item = sectionTableOrderedByID.begin(); item != sectionTableOrderedByID.end(); item++) {
        SectionTableRecord &section = item->second;
        if (section.length == 0) continue;

        for (int i = 0; i < section.sectionData.size(); i++) {
            if (cnt % 8 == 0 && cnt != 0) file << "\n";
            if (cnt % 8 == 0) file << setfill('0') << setw(4) << i + section.baseAddress << ": ";
            file << setfill('0') << setw(2) << (0xFF & section.sectionData[i]) << " ";

            cnt++;
        }
    }
    file << dec;

    /* file closing */
    file.close();
    return true; // everything went well
}

bool Linker::writeBinaryFile() {
    ofstream file; // output binary file (unlinkable but needs an emulator to be executed)

    /* file opening */
    file.open(outputFilePath, ios::out | ios::binary);
    if (!file.is_open()) return false;

    /* writing sections data */
    unsigned tmp = sectionTable.size(); // [the number of "rows" (sections)] == [the number of program segments]
    if (sectionTable.find("UNDEF") != sectionTable.end()) tmp--;
    if (sectionTable.find("ABS") != sectionTable.end()) tmp--;

    file.write((char *)(&tmp), sizeof(tmp));

    // it is important to follow the sequence of sections when printing to the output file
    // otherwise they would be extracted from the 'sectionTable' map according to the section name (ascending)
    map<int, SectionTableRecord> sectionTableOrderedByID; // map by default sorts elements by the integer key in the ascending order
    for (auto item = sectionTable.begin(); item != sectionTable.end(); item++) {
        SectionTableRecord &section = item->second;
        sectionTableOrderedByID.insert({section.id, section});
    }

    for (auto item = sectionTableOrderedByID.begin(); item != sectionTableOrderedByID.end(); item++) {
        SectionTableRecord &section = item->second;
        if (section.name == "UNDEF" || section.name == "ABS") continue;

        /* section.sectionData */
        tmp = section.sectionData.size(); // section data length

        file.write((char *)(&tmp), sizeof(tmp));
        file.write((char *)(&section.sectionData[0]), section.sectionData.size() * sizeof(section.sectionData[0]));

        /* section.baseAddress */
        file.write((char *)(&section.baseAddress), sizeof(section.baseAddress));
    }

    /* file closing */
    file.close();
    return true; // everything went well
}

/* printing methods */
void Linker::printErrorMessages() {
    cout << "\n\nLinking errors:" << endl;
    for (string e : linkingErrors)
        cout << e << endl;
}
