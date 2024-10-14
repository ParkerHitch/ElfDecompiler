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

ParsedProgram* parseMainFn(Elf64_Addr mainStartAddr,
                           ParsedElf* elf,
                           csh* csHandle);

CodeBlock* createCodeBlock(Elf64_Addr startAddr,
                           ParsedElf* elf,
                           Elf64_Addr maxAddr, // All instructions used must have addresses < this. 0 for no limit
                           csh* csHandle);

