#include "elfParser.h"

#include <elf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Funny helper functions
int readElfHeader(FILE* elff, Elf64_Ehdr* header);
int readSectionHeaders(FILE* elff, Elf64_Ehdr* header, Elf64_Shdr** sectHdrs);
int readProgramHeaders(FILE* elff, Elf64_Ehdr* header, Elf64_Phdr** progHdrs);

// Reads an elf, extracting info and loading segments into memory.
ParsedElf* readElf(char fname[]) {
    ParsedElf* out = calloc(sizeof(ParsedElf), 1);
    FILE* elff = fopen(fname, "r");

    if (!elff) {
        printf("Failed to open %s\n", fname);
        free(out);
        return NULL;
    }

    // Read in the header
    int result = readElfHeader(elff, &out->header);
    if (result) {
        // TODO: Error messanging
        printf("Bad header on elf. Code: %d\n", result);
        free(out);
        fclose(elff);
        return NULL;
    }

    out->sectionHeaderCount = out->header.e_shnum;
    out->programHeaderCount = out->header.e_phnum;

    // Read section & program headers
    if (readSectionHeaders(elff, &out->header, &out->sectionHeaders)){
        freeParsedElf(out);
        printf("Bad section headers. Code: %d\n", result);
        fclose(elff);
        return NULL;
    }

    if (readProgramHeaders(elff, &out->header, &out->programHeaders)){
        freeParsedElf(out);
        printf("Bad program headers. Code: %d\n", result);
        fclose(elff);
        return NULL;
    }

    out->sectionNames = malloc(sizeof(char*) * out->sectionHeaderCount);
    int strtablestart = out->sectionHeaders[out->header.e_shstrndx].sh_offset;

    int symbolTableStart = 0;
    int symbolTableEnd = 0;
    int symbolStrTableStart = 0;

    for (int i=0; i<out->sectionHeaderCount; i++) {
        int shstrOff = out->sectionHeaders[i].sh_name;
        fseek(elff, strtablestart + shstrOff, SEEK_SET);
        int count = 0;
        while (fgetc(elff)) {
            count ++;
        }
        fseek(elff, strtablestart + shstrOff, SEEK_SET);
        out->sectionNames[i] = malloc(count);
        fread(out->sectionNames[i], count, 1, elff);

        if (!strcmp(out->sectionNames[i], ".text")) {
            printf(".text section found!\n");
            Elf64_Shdr* textSectHeader = &out->sectionHeaders[i];
            out->textSectionInd = i;
            out->textSectionVAddr = textSectHeader->sh_addr;
            out->textSectionSize = textSectHeader->sh_size;
            fseek(elff, textSectHeader->sh_offset, SEEK_SET);
            out->textSection = malloc(textSectHeader->sh_size);
            result = fread(out->textSection, textSectHeader->sh_size, 1, elff);
            if (result != 1) {
                printf("Failed to read text section\n");
            }
        } else if (!strcmp(out->sectionNames[i], ".symtab")) {
            symbolTableStart = out->sectionHeaders[i].sh_offset;
            symbolTableEnd = symbolTableStart + out->sectionHeaders[i].sh_size;
        } else if (!strcmp(out->sectionNames[i], ".strtab")) {
            symbolStrTableStart = out->sectionHeaders[i].sh_offset;
        }
    }

    if (!symbolStrTableStart || !symbolStrTableStart) {
        printf("Error! No symbol table\n");
        return NULL;
    }
    do {
        fseek(elff, symbolTableStart, SEEK_SET);
        Elf64_Sym sym;
        fread(&sym, sizeof(Elf64_Sym), 1, elff);
        fseek(elff, symbolStrTableStart + sym.st_name, SEEK_SET);
        int count = 0;
        while (fgetc(elff)) {
            count ++;
        }
        fseek(elff, symbolStrTableStart + sym.st_name, SEEK_SET);
        char* name = malloc(count);
        fread(name, count, 1, elff);

        if (!strcmp(name, "main")){
            printf("Main found: 0x%08lx\n", sym.st_value);
            out->mainFnStart = sym.st_value;
            break;
        }

        symbolTableStart += sizeof(Elf64_Sym);

    } while (symbolTableStart <= symbolTableEnd);

    int loadCount=0;
    for(int i=0; i<out->programHeaderCount; i++)
        if (out->programHeaders[i].p_type == PT_LOAD)
            loadCount++;

    out->loadedSegmentCount = loadCount;

    // Load segments into memory
    out->segmentMemLocations = malloc(sizeof(Elf64_Addr) * loadCount);
    out->segmentMemLens      = malloc(sizeof(Elf64_Addr) * loadCount);
    out->loadedSegments      = malloc(sizeof(uint8_t*  ) * loadCount);
    int j = 0;
    for (int i=0; i<out->programHeaderCount; i++) {
        Elf64_Phdr phdr = out->programHeaders[i];
        if (phdr.p_type != PT_LOAD)
            continue;
        out->segmentMemLocations[j] = phdr.p_vaddr;
        out->segmentMemLens[j] = phdr.p_memsz;

        out->loadedSegments[j] = calloc(sizeof(uint8_t), phdr.p_memsz);

        printf("Segment no: %d, Addr: 0x%08lx, MemLen: %lu, FileSize: %lu\n", j, phdr.p_vaddr, phdr.p_memsz, phdr.p_filesz);

        fseek(elff, phdr.p_offset, SEEK_SET);
        result = fread(out->loadedSegments[j], phdr.p_filesz, 1, elff);

        if (result != 1) {
            printf("Failed to load segment into memory. Code: %d\n", result);
            // TODO: Better error handling. Return. Same above.
        }
        j++;
    }

    return out;
}

// Returns a pointer into one of the buffers in the ParsedElf struct corresponding to that virtual address
// NULL if addr is invalid, i.e. not part of a segment
uint8_t* readVAddr(ParsedElf* elf, Elf64_Addr addr) {
    for (int i=0; i<elf->loadedSegmentCount; i++) {
        int relativeLoc = addr - elf->segmentMemLocations[i];
        if (relativeLoc > 0 && relativeLoc < elf->segmentMemLens[i]) {
            return &(elf->loadedSegments[i][relativeLoc]);
        }
    }
    return NULL;
}

int getVAddrIndex(ParsedElf* elf, Elf64_Addr addr) {
    for (int i=0; i<elf->loadedSegmentCount; i++) {
        int relativeLoc = addr - elf->segmentMemLocations[i];
        if (relativeLoc > 0 && relativeLoc < elf->segmentMemLens[i]) {
            return i;
        }
    }
    return -1;
}

#define READ_INTO_FIELD(_struct, field, file) { \
    result = fread(&(_struct->field), sizeof(_struct->field), 1, file); \
    if (result != 1) { \
        return -1; \
    } \
}

int readElfHeader(FILE* elff, Elf64_Ehdr* header) {
    unsigned long result;

    // Check elf identifier
    READ_INTO_FIELD(header, e_ident, elff);
    printf("Read ident\n");
    // Check for magic
    if (memcmp(header->e_ident, ELFMAG, SELFMAG)) {
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
    READ_INTO_FIELD(header, e_type, elff)
    if (header->e_type != ET_EXEC){
        return -5;
    }

    // Machine type + make sure we amd64
    READ_INTO_FIELD(header, e_machine, elff);
    if (header->e_machine != EM_X86_64){
        return -6;
    }

    // Version
    READ_INTO_FIELD(header, e_version, elff);
    if (header->e_version != EV_CURRENT){
        return -7;
    }

    // Other stuff
    READ_INTO_FIELD(header, e_entry, elff);
    READ_INTO_FIELD(header, e_phoff, elff);
    READ_INTO_FIELD(header, e_shoff, elff);
    READ_INTO_FIELD(header, e_flags, elff);
    READ_INTO_FIELD(header, e_ehsize, elff);
    READ_INTO_FIELD(header, e_phentsize, elff);
    READ_INTO_FIELD(header, e_phnum, elff);
    READ_INTO_FIELD(header, e_shentsize, elff);
    READ_INTO_FIELD(header, e_shnum, elff);
    READ_INTO_FIELD(header, e_shstrndx, elff);

    return 0;
}

int readSectionHeaders(FILE* elff, Elf64_Ehdr* header, Elf64_Shdr** sectHdrs){
    if (header->e_shnum <= 0) {
        *sectHdrs = NULL;
        return 0;
    }

    if (header->e_shentsize != sizeof(Elf64_Shdr)){
        return -2;
    }

    *sectHdrs = malloc(header->e_shnum * sizeof(Elf64_Shdr));
    fseek(elff, header->e_shoff, SEEK_SET);


    int result = 0;
    for (int i=0; i<header->e_shnum; i++){
        result = fread(&((*sectHdrs)[i]), sizeof(Elf64_Shdr), 1, elff);
        if (result != 1){
            return -1;
        }
    }

    return 0;
}

int readProgramHeaders(FILE* elff, Elf64_Ehdr* header, Elf64_Phdr** progHdrs){
    if (header->e_phnum <= 0) {
        *progHdrs = NULL;
        return 0;
    }

    if (header->e_phentsize != sizeof(Elf64_Phdr)){
        return -2;
    }

    *progHdrs = malloc(header->e_phnum * sizeof(Elf64_Phdr));
    fseek(elff, header->e_phoff, SEEK_SET);


    int result = 0;
    for (int i=0; i<header->e_phnum; i++){
        result = fread(&((*progHdrs)[i]), sizeof(Elf64_Phdr), 1, elff);
        if (result != 1){
            return -1;
        }
    }

    return 0;
}

void freeParsedElf(ParsedElf *elf){
    if(elf->sectionNames) {
        for (int i=0; i<elf->sectionHeaderCount; i++){
            free(elf->sectionNames[i]);
        }
        free(elf->sectionNames);
    }

    if (elf->sectionHeaders) {
        free(elf->sectionHeaders);
    }

    if (elf->programHeaders) {
        free(elf->sectionHeaders);
    }

    if (elf->textSection) {
        free(elf->textSection);
    }

    if (elf->segmentMemLocations) {
        free(elf->segmentMemLocations);
    }

    if (elf->segmentMemLens) {
        free(elf->segmentMemLens);
    }

    if (elf->loadedSegments) {
        for (int i=0; i<elf->programHeaderCount; i++){
            free(elf->loadedSegments[i]);
        }
        free(elf->loadedSegments);
    }
}
