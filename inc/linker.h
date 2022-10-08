#ifndef LINKER_H
#define LINKER_H

#include <string>
#include <vector>
#include <map>

using namespace std;

class Linker {
private:
    vector<string> inputFilesPaths; // files we link
    string outputFilePath;

    vector<string> linkingErrors;

    /* section table */
    struct SectionTableRecord {
        unsigned id;     // incremental sections counter
        unsigned length; // length (size) of the section
        string name;     // section identifier

        vector<char> sectionData;  // data for the output object file

        unsigned baseAddress; // address in memory (output file) where the section is loaded
    };
    map<string, SectionTableRecord> sectionTable; // linker output section table

    /* symbol table */
    struct SymbolTableRecord {
        unsigned id; // incremental symbols counter
        int offset;  // symbol offset within section (symbol value)

        bool isDefined;
        bool isLocal;
        bool isExtern;

        string section; // section in which the symbol is defined
        string name;    // symbol identifier

        string file; // file to which the symbol belongs
    };
    map<string, SymbolTableRecord> symbolTable; // linker output symbol table
    vector<string> externSymbols;               // names of extern symbols

    /* relocation table */
    struct RelocationTableRecord {
        string section;  // section to which the given record is linked
        unsigned offset; // offset to the first byte of the field to be modified in the aggregated section

        string type;   // relocation type (absolute or PC relative)
        string symbol; // local symbols -> the name of the section; global symbols -> the name of the symbol itself

        string file; // origin file of the section to which the relocation record refers
    };
    vector<RelocationTableRecord> relocationTable; // linker output relocation table

    /* data about sections from input files */
    struct InputSectionData {
        unsigned length; // size of a section from the input file
        string name;     // section identifier

        string file;                               // input file to which the section belongs
        unsigned baseAddressOfUnaggregatedSection; // base address of the section in the aggregated section (relative to the beginning of the file)
    };
    map<string, map<string, InputSectionData>> InputSectionsData; //[key1 == section.name, key2 == inputFilPath]

    /* methods called by link() */
    bool fillOutputTablesFromInputFiles(); // collects data from input relocatable files

    void addOutputSection(SectionTableRecord &, string); // aggregation of sections into the output table section
    bool addOutputSymbol(SymbolTableRecord &);           // adding symbols to the output symbol table
    void addOutputRelocation(RelocationTableRecord &);   // adding relocations to the output relocation table

    bool resolveExternSymbols();   // checks that all extern symbols are resolved
    bool setSectionsBaseAddress(); // defines the positions of sections in the output file
    void resolveRelocations();     // modifies address fields in aggregated sections according to relocation records

    bool writeHexFile();    // creates a .hex output file
    bool writeBinaryFile(); // creates a binary output file

public:
    Linker(vector<string>, string); // constructor

    bool link();
    void printErrorMessages();
};

#endif
