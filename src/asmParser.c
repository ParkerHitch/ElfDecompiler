#include "asmParser.h"
#include "capstone/capstone.h"
#include "capstone/x86.h"
#include <stdlib.h>

// If reg has already been modified this block, a copy of that operation gets substituted
//   Otherwise simply return a DATA Operation with the register ProgramData as its unaryOperand
Operation* copyLookupOrCreateRegOp(CodeBlock* block, x86_reg reg);

// Updates block such that its impacts reflects those described by the instruction
void updateImpactsOfArithmetic(CodeBlock* block, cs_insn* insn);

// Updates block such that its impacts reflects those described by the instruction
void updateImpactsOfMov(CodeBlock* block, cs_insn* insn);

// Updates block such that its impacts reflects those described by the instruction
void updateImpactsOfLea(CodeBlock* block, cs_insn* insn);

// Finds the impact ascociated with data in block
//   If no existing impact creates one.
CodeImpact* findOrCreateImpact(CodeBlock* block, Operation* location);

// Creates an operation describing the pointer arithmetic ascociated with mem
Operation* memoryOp(CodeBlock* block, x86_op_mem mem);

// Finds any impact with that location and returns a copy of the operation stored to that location.
// NULL if no location found
Operation* findAndCopyImpactOperation(CodeBlock* block, Operation* location);

// Returns a pointer to an "impact" field of an impact of the given block
// This impact can either be newly created or already existing and ready to be overwritten
CodeImpact* getImpactToUpdate(CodeBlock* block, cs_x86_op op);

// Creates DEREF op or retrieves a copy of the value pointed to
Operation* derefMem(CodeBlock* block, x86_op_mem mem);

// NOTE: FOR DEBUG PRINTING ONLY!!
csh handle;

CodeBlock* parseAsmFromInstruction(AsmParserState* parser,
                                   Elf64_Addr startInstAddr,
                                   ParsedElf* elf,
                                   csh* csHandle){

    handle = *csHandle;

    CodeBlock* block = initCodeBlock();

    Elf64_Addr currentAddr = startInstAddr;

    // Get which segment we are reading from;
    int segInd = getVAddrIndex(elf, currentAddr);
    if (segInd == -1)
        return block;

    uint8_t* inCode = &(elf->loadedSegments[segInd][currentAddr - elf->segmentMemLocations[segInd]]);
    size_t len = (elf->segmentMemLens[segInd] - (currentAddr - elf->segmentMemLocations[segInd]));

    int good;

    do {
        // Initialize a blank instruction to be read
        appendBlankInsn(block, csHandle);
        cs_insn* insn = block->instructions[block->instructionCount-1];


        // Get a pointer into that segment

        good = cs_disasm_iter(*csHandle, &inCode,
                       &len,
                       &currentAddr, 
                       insn);

        printf("0x%"PRIx64":\t%s\t\t%s\n", insn->address, insn->mnemonic,
               insn->op_str);

        switch(insn->id) {
            case X86_INS_ADD:
            case X86_INS_SUB:
            case X86_INS_MUL:
            case X86_INS_DIV:
                updateImpactsOfArithmetic(block, insn);
                break;
            case X86_INS_MOV:
            case X86_INS_MOVABS:
                updateImpactsOfMov(block, insn);
                break;
            case X86_INS_LEA:
                updateImpactsOfLea(block, insn);
                break;
            default:
                printf("Instruction: %s unknown :(\n", insn->mnemonic);
        }

        printImpacts(block, *csHandle);
    } while (good);


    return NULL;
}


void updateImpactsOfMov(CodeBlock* block, cs_insn* insn){
    cs_x86 detail = insn->detail->x86;
    if (detail.op_count != 2){
        printf("!Bad operand count for mov\n");
    }

    CodeImpact* impactToUpdate = getImpactToUpdate(block, detail.operands[0]);
    Operation** operationToUpdate = &impactToUpdate->impact;

    Operation* valueAssigned;

    if (detail.operands[1].type == X86_OP_REG) {
        // Copy value of register or just run it
        valueAssigned = copyLookupOrCreateRegOp(block, detail.operands[1].reg);
    } else if (detail.operands[1].type == X86_OP_IMM) {
        valueAssigned = createDataOperation(LITERAL, &detail.operands[1].imm);
    } else if (detail.operands[1].type == X86_OP_MEM) {
        valueAssigned = derefMem(block, detail.operands[1].mem);
    }

    valueAssigned->width = detail.operands[1].size;

    if (*operationToUpdate) {
        deleteOperation(*operationToUpdate);
    }

    *operationToUpdate = valueAssigned;
}

void updateImpactsOfArithmetic(CodeBlock* block, cs_insn* insn){
    cs_x86 detail = insn->detail->x86;
    if (detail.op_count != 2){
        printf("!Bad operand count for arithmetic\n");
    }


    Operation* operation = calloc(sizeof(Operation), 1);

    switch(insn->id) {
        case X86_INS_ADD:
            operation->kind = ADD;
            break;
        case X86_INS_SUB:
            operation->kind = SUB;
            break;
        case X86_INS_MUL:
            operation->kind = SUB;
            break;
        case X86_INS_DIV:
            operation->kind = SUB;
            break;
        default:
            // Unreachable by case in parsing loop.
            break;
    }

    Operation* operands[2];

    for (int i=0; i<2; i++) {
        if (detail.operands[i].type == X86_OP_REG) {
            // Copy value of register or just run it
            operands[i] = copyLookupOrCreateRegOp(block, detail.operands[i].reg);
        } else if (detail.operands[i].type == X86_OP_IMM) {
            operands[i] = createDataOperation(LITERAL, &detail.operands[i].imm);
        } else if (detail.operands[i].type == X86_OP_MEM) {
            operands[i] = derefMem(block, detail.operands[i].mem);
        } else {
            printf("AAAAA! Bad operand to instruction: %s\n", insn->op_str);
        }
    }


    operation->info.binaryOperands.op1 = operands[0];
    operation->info.binaryOperands.op2 = operands[1];
    operation->width = detail.operands[0].size;

    char test[256];
    operationToStr(operation, test, handle);
    printf("Arithmetic: %s\n", test);

    CodeImpact* impactToUpdate = getImpactToUpdate(block, detail.operands[0]);
    Operation** operationToUpdate = &impactToUpdate->impact;

    if (*operationToUpdate) {
        deleteOperation(*operationToUpdate);
    }

    *operationToUpdate = operation;
}

void updateImpactsOfLea(CodeBlock* block, cs_insn* insn) { 
    cs_x86 detail = insn->detail->x86;
    if (detail.op_count != 2){
        printf("!Bad operand count for lea\n");
    }

    CodeImpact* impactToUpdate = getImpactToUpdate(block, detail.operands[0]);
    Operation** operationToUpdate = &impactToUpdate->impact;

    Operation* newVal = memoryOp(block, detail.operands[1].mem);

    if (*operationToUpdate)
        deleteOperation(*operationToUpdate);

    *operationToUpdate = newVal;
}

CodeImpact* getImpactToUpdate(CodeBlock* block, cs_x86_op op) {
    Operation* destLocation;
    x86_reg impactedSegment = X86_REG_INVALID;

    if (op.type == X86_OP_REG) {
        // Storing in regiser
        destLocation = createDataOperation(REGISTER, &op.reg);
    } else if (op.type == X86_OP_IMM) {
        // Storing in hard coded address
        destLocation = createDataOperation(LITERAL, &op.imm);
    } else if (op.type == X86_OP_MEM){
        x86_op_mem mem = op.mem;

        impactedSegment = mem.segment;

        destLocation = calloc(sizeof(Operation), 1);
        destLocation->kind = DEREF;
        destLocation->info.unaryOperand = memoryOp(block, mem);
    }
    destLocation->width = op.size;

    CodeImpact* impactToUpdate = findOrCreateImpact(block, destLocation);

    // destLocation gets copied in findOrCreate, or it already existed
    deleteOperation(destLocation);

    return impactToUpdate;
}

Operation* findAndCopyImpactOperation(CodeBlock* block, Operation* location) {
    for (int i=0; i<block->impactCount; i++) {
        if (operationsEquivalent(location, block->impacts[i].impactedLocation)) {
            return deepCopyOperation(block->impacts[i].impact);
        }
    }
    return NULL;
}

Operation* memoryOp(CodeBlock* block, x86_op_mem mem) {
    Operation* base = NULL;
    if (mem.base)
        base = copyLookupOrCreateRegOp(block, mem.base);

    if (mem.disp) {
        Operation* new;
        if (base) {
            new = calloc(1, sizeof(Operation));
            new->kind = ADD;
            new->info.binaryOperands.op1 = base;
            new->info.binaryOperands.op2 = createDataOperation(LITERAL, &mem.disp);
        } else {
            new = createDataOperation(LITERAL, &mem.disp);
        }
        base = new;
    }

    if (mem.index) {

        Operation* index = copyLookupOrCreateRegOp(block, mem.index);

        if (mem.scale != 1) {
            Operation* mul = calloc(1, sizeof(Operation));
            mul->kind = MUL;
            mul->info.binaryOperands.op1 = index;
            mul->info.binaryOperands.op2 = createDataOperation(LITERAL, &mem.scale);
            index = mul;
        }

        Operation* new;

        if (base) {
            new = calloc(1, sizeof(Operation));
            new->kind = ADD;
            new->info.binaryOperands.op1 = base;
            new->info.binaryOperands.op2 = index;
        } else {
            new = index;
        }

        base = new;
    }

    return base;
}

CodeImpact* findOrCreateImpact(CodeBlock* block, Operation* location) {
    for (int i=0; i<block->impactCount; i++) {
        if (operationsEquivalent(location, block->impacts[i].impactedLocation)) {
            return &block->impacts[i];
        }
    }

    if (location->kind==DATA && location->info.data.kind == REGISTER) {
        // TODO:
        // It is possible we are overwriting the lower bits of an already existing register
        if (location->width < 8) {

        }
        // TODO:
        // It is possible we are overwriting a smaller, already existing register
        
        // Naturally this applies to memory as well but I ain't doing all that
    }

    if(block->impactCount == block->impactCapacity) {
        block->impactCapacity *= 2;
        block->impacts = realloc(block->impacts, sizeof(CodeImpact)*block->impactCapacity);
    }

    CodeImpact* newImpact = &block->impacts[block->impactCount++];

    newImpact->impactedLocation = deepCopyOperation(location);
    newImpact->segment = X86_REG_INVALID;
    newImpact->impact = NULL;

    return newImpact;
}

Operation* copyLookupOrCreateRegOp(CodeBlock* block, x86_reg reg){
    // See if we have directly recoreded a register
    for (int i=0; i<block->impactCount; i++) {
        if (block->impacts[i].impactedLocation->kind == DATA &&
            block->impacts[i].impactedLocation->info.data.kind == REGISTER &&
            block->impacts[i].impactedLocation->info.data.info.reg == reg) 
        {
            return deepCopyOperation(block->impacts[i].impact);
        }
    }

    // TODO:
    // What if the register we are looking for is overwritten by a larger register?

    // New op time
    Operation* newOp = calloc(1, sizeof(Operation));
    newOp->kind = DATA;
    newOp->info.data.kind = REGISTER;
    newOp->info.data.info.reg = reg;
    return newOp;
}

Operation* derefMem(CodeBlock* block, x86_op_mem mem){
    Operation* locationDereffed = memoryOp(block, mem);

    Operation* dereffedValue = findAndCopyImpactOperation(block, locationDereffed);

    if (dereffedValue) {
        deleteOperation(locationDereffed);
    } else {
        dereffedValue = calloc(sizeof(Operation), 1);
        dereffedValue->kind = DEREF;
        dereffedValue->info.unaryOperand = locationDereffed;
    }
    
    return dereffedValue;
}
