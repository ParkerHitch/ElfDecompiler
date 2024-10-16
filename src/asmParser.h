#ifndef ASM_PARSER
#define ASM_PARSER

#include "capstone/capstone.h"
#include "datastructs.h"

#include "elfParser.h"

#include <elf.h>
#include <stdint.h>

ParsedProgram* parseMainFn(Elf64_Addr mainStartAddr,
                           ParsedElf* elf,
                           csh* csHandle);

CodeBlock* createCodeBlock(Elf64_Addr startAddr,
                           ParsedElf* elf,
                           Elf64_Addr maxAddr, // All instructions used must have addresses < this. 0 for no limit
                           csh* csHandle);

// Finds any impact with that location and returns a copy of the operation stored to that location.
// NULL if no location found
Operation* findAndCopyImpactOperation(CodeBlock* block, Operation* location);

#endif
