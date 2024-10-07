#include <elf.h>
#include <stdint.h>

// A structure representing all the data we need for this bare-bones decompiler
// Please only calloc, for nullptrs in all fields
typedef struct _ParsedElf {
    // Elf header
    Elf64_Ehdr header;

    // // Number of segments (program headers). Extracted from elf header
    // Elf64_Half numSegments;
    // // Pointer to all segment headers loaded into an array
    // Elf64_Phdr* segmentHdrs;
    // // Extracted from the headers. Virtual addresses where each segment is loaded.
    // Elf64_Addr* segmentMemLocations;
    // // Extracted from the headers. Length of memory block where each segment is loaded.
    // Elf64_Addr* segmentMemLens;

    // // Each member in this array is a pointer to a segment loaded into memory 
    // //   as it would be when executing the .elf
    // uint8_t** loadedSegments;

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

} ParsedElf;

// Reads an elf, extracting info and loading segments into memory.
ParsedElf* readElf(char fname[]);

// Read the byte at an address in virtual memory
// Returns -1 for invalid address
int16_t readVAddr(ParsedElf* elf, Elf64_Addr addr);

// Releases all memory ascociated with a parsed elf
void freeParsedElf(ParsedElf* elf);
