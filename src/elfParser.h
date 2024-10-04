#include <elf.h>
#include <stdint.h>

// A structure representing all the data we need for this bare-bones decompiler
typedef struct _ParsedElf {
    // Elf header
    Elf64_Ehdr header;

    // Number of segments (program headers). Extracted from elf header
    Elf64_Half numSegments;
    // Pointer to all segment headers loaded into an array
    Elf64_Phdr* segmentHdrs;
    // Extracted from the headers. Virtual addresses where each segment is loaded.
    Elf64_Addr* segmentMemLocations;
    // Extracted from the headers. Length of memory block where each segment is loaded.
    Elf64_Addr* segmentMemLens;

    // Each member in this array is a pointer to a segment loaded into memory 
    //   as it would be when executing the .elf
    uint8_t** loadedSegments;
} ParsedElf;

// Reads an elf, extracting info and loading segments into memory.
ParsedElf* readElf(char fname[]);

// Read the byte at an address in virtual memory
// Returns -1 for invalid address
int16_t readVAddr(ParsedElf* elf, Elf64_Addr addr);
