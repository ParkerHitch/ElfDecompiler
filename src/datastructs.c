#include "datastructs.h"

#include "capstone/capstone.h"

// Makes a blank code block
CodeBlock* initCodeBlock(){
    CodeBlock* out = malloc(sizeof(CodeBlock));

    out->instructionCount = 0;
    out->instructionCapacity = 10;
    out->instructions = malloc(sizeof(cs_insn*) * 10);

    out->impactCount = 0;
    out->impactCapacity = 10;
    out->impacts = malloc(sizeof(CodeImpact*) * 10);


    out->dependencyCount = 0;
    out->dependencyCapacity = 10;
    out->dependencies = malloc(sizeof(ProgramData*) * 10);

    return out;
};

// Append an uninitialized instruction block's array
void appendBlankInsn(CodeBlock* block, csh* csHandle){

    cs_insn* newInsn = cs_malloc(*csHandle);


    if (block->instructionCapacity == block->instructionCount+1) {
        block->instructions = realloc(block->instructions, sizeof(cs_insn*)*block->instructionCapacity*2);
        block->instructionCapacity *= 2;
    }

    block->instructions[block->instructionCount++] = newInsn;
}
