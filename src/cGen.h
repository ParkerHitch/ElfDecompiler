#ifndef __CGEN
#define __CGEN

#include "asmParser.h"
#include "cfgRecovery.h"
#include <stdint.h>
#include <stdio.h>

#include <unistd.h>

void writeC(FILE* outFile, StructuredCodeTree* tree, ParsedElf* elf);

#endif // !__CGEN
