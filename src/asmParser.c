#include "asmParser.h"
#include "capstone/capstone.h"

CodeBlock* parseAsmFromInstruction(AsmParserState* parser,
                                   Elf64_Addr startInstAddr,
                                   ParsedElf* elf,
                                   csh* csHandle){

    CodeBlock* block = initCodeBlock();

    Elf64_Addr currentAddr = startInstAddr;

    // Get which segment we are reading from;
    int segInd = getVAddrIndex(elf, currentAddr);
    if (segInd == -1)
        return block;

    printf("segInd:%d\n", segInd);

    uint8_t* inCode = &(elf->loadedSegments[segInd][currentAddr - elf->segmentMemLocations[segInd]]);
    size_t len = (elf->segmentMemLens[segInd] - (currentAddr - elf->segmentMemLocations[segInd]));

    int good;

    do {
        // Initialize a blank instruction to be read
        appendBlankInsn(block, csHandle);
        cs_insn* instr = block->instructions[block->instructionCount-1];


        // Get a pointer into that segment

        good = cs_disasm_iter(*csHandle, &inCode,
                       &len,
                       &currentAddr, 
                       instr);

        printf("0x%"PRIx64":\t%s\t\t%s\n", instr->address, instr->mnemonic,
               instr->op_str);

    } while (good);
    return NULL;
}
