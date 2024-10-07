#include "asmParser.h"
#include "capstone/capstone.h"

CodeBlock* parseAsmFromInstruction(AsmParserState* parser,
                                   Elf64_Addr startInstAddr,
                                   ParsedElf* elf,
                                   csh* csHandle){

    CodeBlock* block = initCodeBlock();

    Elf64_Addr currentAddr = startInstAddr;

    do {
        appendBlankInsn(block, csHandle);
        cs_insn* instr = block->instructions[block->instructionCount-1];

        int segInd = getVAddrIndex(elf, currentAddr);
        cs_disasm_iter(*csHandle, &(elf->loadedSegments[segInd]),
                       &(elf->segmentMemLens[segInd]),
                       &currentAddr, 
                       instr);

    } while (1);
    return NULL;
}
