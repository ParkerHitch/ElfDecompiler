#ifndef __CGEN
#define __CGEN

#include "cfgRecovery.h"
#include <stdint.h>
#include <stdio.h>

#include <unistd.h>

void writeC(FILE* outFile, StructuredCodeTree* tree);

#endif // !__CGEN
