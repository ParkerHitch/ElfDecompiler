#include <elf.h>
#include <stdint.h>


#ifndef ELF_PARSER
#define ELF_PARSER

// A structure representing all the data we need for this bare-bones decompiler
// Please only calloc, for nullptrs in all fields
typedef struct _ParsedElf {
    // Elf header
    Elf64_Ehdr header;

    // A collection of all the null terminated section names
    char** sectionNames;
    Elf64_Half sectionHeaderCount;
    Elf64_Shdr* sectionHeaders;

    Elf64_Half programHeaderCount;
    Elf64_Phdr* programHeaders;

    // Binary contents of the .text section 
    uint8_t* textSection;
    // Amount of bytes in textSection
    Elf64_Addr textSectionSize;
    // Virtual address of the first byte of the text section when loadded into memory
    Elf64_Addr textSectionVAddr;
    // Index into the section headers at which the text header is
    Elf64_Half textSectionInd;


    unsigned int loadedSegmentCount;
    // Extracted from the headers. Virtual addresses where each segment is loaded.
    Elf64_Addr* segmentMemLocations;
    // Extracted from the headers. Length of memory block where each segment is loaded.
    Elf64_Addr* segmentMemLens;
    // Each member in this array is a pointer to a segment loaded into memory 
    //   as it would be when executing the .elf
    uint8_t** loadedSegments;
    // Length of above 3 arrays is equal to programHeaderCount
    
    Elf64_Addr mainFnStart;

} ParsedElf;

// Reads an elf, extracting info and loading segments into memory.
ParsedElf* readElf(char fname[]);

// Returns a pointer into one of the buffers in the ParsedElf struct corresponding to that virtual address
// NULL if addr is invalid, i.e. not part of a segment
uint8_t* readVAddr(ParsedElf* elf, Elf64_Addr addr);

int getVAddrIndex(ParsedElf* elf, Elf64_Addr addr);

// Releases all memory ascociated with a parsed elf
void freeParsedElf(ParsedElf* elf);
#endif
