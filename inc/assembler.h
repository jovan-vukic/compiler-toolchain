#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <string>
#include <vector>
#include <map>

using namespace std;

class Assembler {
private:
    string inputFilePath;  // path to the input file
    string outputFilePath; // path to the output file

    vector<string> inputFile;              // cleared input file (without additional spaces, comments etc.)
    vector<unsigned> inputFileLineNumbers; // line numbers in the input file; the index in the array is the line number in the 'inputFile'
    unsigned currentLine;                  // current line number

    bool errorOccurred;
    map<unsigned, string> errorMessages; // the error and the line in which it occurred

    /* symbol table and more */
    static unsigned nextSymbolID; // id of the next symbol in the symbol table

    struct SymbolTableRecord {
        unsigned id; // symbol id
        int offset;  // symbol offset (symbol value for ABS symbols)

        bool isDefined;
        bool isLocal;  // is the symbol local or global
        bool isExtern; // is the symbol imported

        string section; // the section in which the symbol is defined
        string name;    // symbol identifier
    };
    map<string, SymbolTableRecord> symbolTable;

    /* section table and more */
    static unsigned nextSectionID; // id of the next section in the section table

    struct SectionTableRecord {
        unsigned id;     // section id
        unsigned length; // section size
        string name;     // section identifier

        vector<char> sectionData;
    };
    map<string, SectionTableRecord> sectionTable;

    /* forward reference table and more */
    struct ForwardReferenceTableRecord {
        string section;      // section in which the symbol was used
        unsigned offset;     // offset to the field that needs to be modified, in the 'section'
        bool isLittleEndian; // how do we place bytes starting from 'offset' (directives -> little endian, commands -> big endian)

        char operation; // '+', '-' (absolute addressing) or 'R' (PC relative addressing)
        // int size; // the number of bytes occupied by the symbol to be modified is always 2 (see the text of the project)
        unsigned currentLine; // for error printing purposes after backpatching()

        string symbol; // identifier of the referenced symbol
    };
    vector<ForwardReferenceTableRecord> forwardReferenceTable;

    /* relocation table and more */
    struct RelocationTableRecord {
        string section;  // section to which the given record is linked
        unsigned offset; // offset to the first byte of the field to be modified in the 'section'

        string type;   // relocation type (absolute or PC relative)
        string symbol; // local symbols -> the name of the section; global symbols -> the name of the symbol itself

        // int addend; // unused
    };
    vector<RelocationTableRecord> relocationTable;

    unsigned locationCounter;
    string currentSection; // name of the current section

    /* utility methods */
    int getDecimalFromLiteral(string);
    string decimalToHexadecimal(int);

    /* processing of the symbol addressing */
    int absoluteAddressing(string, bool, char); // absolute addressing
    int relativeAddressing(string);             // pc relative addressing

    /* methods called by assemble() */
    bool readFile();
    bool assemblePass();
    bool backpatching();

    bool writeTextFile();
    bool writeBinaryFile();

    /* methods for directives and commands processing - called by assemblePass() */
    bool addSymbol(string);        // label:
    bool addSectionSymbol(string); // .section
    bool addGlobalSymbol(string);  // .global
    bool addExternSymbol(string);  // .extern

    bool processSkipDirective(string); // .skip
    bool processWordDirective(string); // .word

    bool processCommand(string); // processing an assembler command

    /* methods called by writeTextFile() */
    void printSymbolTable(ostream &);
    void printSectionTable(ostream &);
    void printSectionData(ostream &);
    void printRelocationTable(ostream &);

public:
    Assembler(string, string); // constructor

    bool assemble();
    void printErrorMessages();
};

#endif
