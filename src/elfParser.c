#include "elfParser.h"

#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Funny helper functions
int readHeader(FILE* elff, Elf64_Ehdr* header);

// Reads an elf, extracting info and loading segments into memory.
ParsedElf* readElf(char fname[]) {
    ParsedElf* out = malloc(sizeof(ParsedElf));
    FILE* elff = fopen(fname, "r");

    // Read in the header
    int headerBad = readHeader(elff, &out->header);
    if (headerBad < 0) {
        // TODO: Error messanging
        free(out);
        fclose(elff);
        return NULL;
    }

    // Load program segments
    out->numSegments = out->header.e_phnum;

    return out;
}

// Read the byte at an address in virtual memory
// Returns -1 for invalid address
int16_t readVAddr(ParsedElf* elf, Elf64_Addr addr);

#define READ_TO_HEADER(header, field, elff) { \
    result = fread(&header->field, sizeof(header->field), 1, elff); \
    if (result != sizeof(typeof(header->field))) { \
        return -1; \
    } \
}

int readHeader(FILE* elff, Elf64_Ehdr* header) {
    unsigned long result;

    // Check elf identifier
    READ_TO_HEADER(header, e_ident, elff);
    // Check for magic
    if (!memcmp(header->e_ident, ELFMAG, SELFMAG)) {
        return -1;
    }
    // Make sure we 64bit
    if (header->e_ident[EI_CLASS]!=ELFCLASS64) {
        return -2;
    }
    // Make sure we little endian
    if (header->e_ident[EI_DATA]!=ELFDATA2LSB) {
        return -3;
    }
    // Make sure we linux
    if (header->e_ident[EI_OSABI]!=ELFOSABI_NONE && header->e_ident[EI_OSABI]!=ELFOSABI_LINUX) {
        return -4;
    }


    // Elftype + make sure we executable
    READ_TO_HEADER(header, e_type, elff)
    if (header->e_type != ET_EXEC){
        return -5;
    }

    // Machine type + make sure we amd64
    READ_TO_HEADER(header, e_machine, elff);
    if (header->e_machine != EM_X86_64){
        return -6;
    }

    // Version
    READ_TO_HEADER(header, e_version, elff);
    if (header->e_version != EV_CURRENT){
        return -7;
    }

    // Other stuff
    READ_TO_HEADER(header, e_entry, elff);
    READ_TO_HEADER(header, e_phoff, elff);
    READ_TO_HEADER(header, e_shoff, elff);
    READ_TO_HEADER(header, e_flags, elff);
    READ_TO_HEADER(header, e_ehsize, elff);
    READ_TO_HEADER(header, e_phentsize, elff);
    READ_TO_HEADER(header, e_phnum, elff);
    READ_TO_HEADER(header, e_shentsize, elff);
    READ_TO_HEADER(header, e_shnum, elff);
    READ_TO_HEADER(header, e_shstrndx, elff);

    return 0;
}
