#include "capstone/capstone.h"
#include "datastructs.h"

#include "elfParser.h"

#include <elf.h>
#include <stdint.h>

typedef struct _AsmParserState {

    // Array of code blocks that already exist
    uint codeBlockCount;
    uint codeBlockCapacity;
    CodeBlock* codeBlocks;

} AsmParserState;

AsmParserState* initAsmParser();

CodeBlock* parseAsmFromInstruction(AsmParserState* parser,
                                   Elf64_Addr startInstAddr,
                                   ParsedElf* elf,
                                   csh* csHandle);

