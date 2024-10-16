#include "asmParser.h"
#include "capstone/capstone.h"
#include "capstone/x86.h"
#include <elf.h>
#include <stdlib.h>
#include <string.h>

// #define DEBUG_PRINT

// If reg has already been modified this block, a copy of that operation gets substituted
//   Otherwise simply return a DATA Operation with the register ProgramData as its unaryOperand
Operation* copyLookupOrCreateRegOp(CodeBlock* block, x86_reg reg);

// Updates block such that its impacts reflects those described by the instruction
void updateImpactsOfArithmetic(CodeBlock* block, cs_insn* insn);

// Updates block such that its impacts reflects those described by the instruction
void updateImpactsOfMov(CodeBlock* block, cs_insn* insn);

// Updates block such that its impacts reflects those described by the instruction
void updateImpactsOfLea(CodeBlock* block, cs_insn* insn);

// Updates flag operation
void updateFlagOp(CodeBlock* block, cs_insn* insn, OperationKind kind);

bool registerIs32bit(x86_reg reg);
bool registerIs64bit(x86_reg reg);
x86_reg get64bitParent(x86_reg reg);
x86_reg get32bitChild(x86_reg reg);

bool isJumpInsn(x86_insn insn);

// Finds the impact ascociated with data in block
//   If no existing impact creates one.
CodeImpact* findOrCreateImpact(CodeBlock* block, Operation* location);

// Creates an operation describing the pointer arithmetic ascociated with mem
Operation* memoryOp(CodeBlock* block, x86_op_mem mem);
// Registers don't get expanded
Operation* memoryOpPrimitive(x86_op_mem mem);

// Returns a pointer to an "impact" field of an impact of the given block
// This impact can either be newly created or already existing and ready to be overwritten
CodeImpact* getImpactToUpdate(CodeBlock* block, cs_x86_op op);

// Creates DEREF op or retrieves a copy of the value pointed to
Operation* derefMem(CodeBlock* block, x86_op_mem mem);

Operation* createRegOp(x86_reg reg);

ExecutableUnit* findJumpedInside(ParsedProgram* program, Elf64_Addr addr, bool* shouldAddToToBeBlock, bool* shouldDeleteReturn, ExecutableUnit*** previousNextPtr);
// NOTE: FOR DEBUG PRINTING ONLY!!
csh handle;

ExecutableUnit* findUnitAt(ParsedProgram* program, Elf64_Addr addr);

typedef struct _ToBeBlocked {
    struct _ToBeBlocked* next;
    bool isJumpDest;
    bool evaluateLastInstr;
    Elf64_Addr addr;
} ToBeBlocked;

void insertNextToBlock(ToBeBlocked** head, Elf64_Addr addr, bool isJumpDest) {
    if (*head == NULL || addr <= (*head)->addr){
        if (*(head) && addr == (*head)->addr){
            (*head)->isJumpDest |= isJumpDest;
            return;
        }
        ToBeBlocked* new = malloc(sizeof(ToBeBlocked));
        new->addr = addr;
        new->isJumpDest = isJumpDest;
        new->next = *head;
        *head = new;
        return;
    }
    ToBeBlocked* current = *head;
    while (current->next &&
           current->next->addr < addr) {
        current = current->next;
    }
    // Addr <= current->next addr
    if (current->next && addr == current->next->addr) {
        current->next->isJumpDest |= isJumpDest;
        return;
    }
    ToBeBlocked* new = malloc(sizeof(ToBeBlocked));
    new->addr = addr;
    new->isJumpDest = isJumpDest;
    new->next = current->next;
    current->next = new;
    return;
}

void printToBlock(ToBeBlocked* head) {
    printf("Block queue:\n");
    while(head) {
        printf(" > 0x%08lx\n", head->addr);
        head = head->next;
    }
}

// Returns true if u1 should go before u2 in the program
bool shouldGoBefore(ExecutableUnit* u1, ExecutableUnit* u2){
    if (u1->firstInstAddr < u2->firstInstAddr)
        return true;
    if (u1->firstInstAddr == u2->firstInstAddr)
        return u1->kind == JUMP_DEST;
    return false;
}

void insertUnitIntoProgram(ParsedProgram* program, ExecutableUnit* unit) {
    if (program->head == NULL || shouldGoBefore(unit, program->head)) {
        unit->next = program->head;
        program->head = unit;
        return;
    }
    ExecutableUnit* current = program->head;
    while (current->next && shouldGoBefore(current->next, unit)) {
        current = current->next;
    }
    // Next is null
    // Unit should come after next
    unit->next = current->next;
    current->next = unit;
}

ParsedProgram* parseMainFn(Elf64_Addr mainStartAddr,
                           ParsedElf* elf,
                           csh* csHandle) {
    
    ParsedProgram* out = malloc(sizeof(ParsedProgram));
    out->numJumpDests = 0;
    out->head = NULL;
    // ExecutableUnit** tailNullptr = &out->head;

    // Points to a queue of destinations to be converted to code blocks. Top one should have lowest address
    ToBeBlocked* toBeBlocked = malloc(sizeof(ToBeBlocked));
    toBeBlocked->next = NULL;
    toBeBlocked->isJumpDest = false;
    toBeBlocked->addr = mainStartAddr;

    int good = true;

    while (toBeBlocked) {
#ifdef DEBUG_PRINT
        printToBlock(toBeBlocked);
#endif /* ifdef DEBUG_PRINT */
        // Pop!
        ToBeBlocked* evaluating = toBeBlocked;
        toBeBlocked = toBeBlocked->next;
        Elf64_Addr currentAddr = evaluating->addr;
        // Jump dest this block should stop at
        Elf64_Addr stopAddr = evaluating->next ? evaluating->next->addr : 0;

        CodeBlock* newBlock = createCodeBlock(currentAddr,
                                              elf,
                                              stopAddr,
                                              csHandle);
        free(evaluating);

        bool shouldDeleteBlock = false;

        if (newBlock->impactCount > 0) {
            ExecutableUnit* newUnit = malloc(sizeof(ExecutableUnit));
            newUnit->kind = CODE_BLOCK;
            newUnit->info.block = newBlock;
            newUnit->firstInstAddr = newBlock->firstInstAddr;

            insertUnitIntoProgram(out, newUnit);
        } else {
            // Can't delete rn because we need to use for making bad conditions
            shouldDeleteBlock = true;
        }
        
        // printf("With block:\n");
        // printParsedProgram(out);
        // printf("With post block:\n");
        // Pop the last instruction
        cs_insn* endingInstruction = newBlock->instructions[--newBlock->instructionCount];
        // Addr after endingInstruction
        currentAddr = newBlock->nextInstAddr;
        // Decrease this so instructions < this actually always impact this block
        // We're kinda popping
        newBlock->nextInstAddr = endingInstruction->address;

        ExecutableUnit* nextUnitExists = findUnitAt(out, endingInstruction->address);
        if (nextUnitExists)
            goto afterFollowInsn; //hehe

        if (endingInstruction->id == X86_INS_RET) {
            good = false;
            ExecutableUnit* returnStatement = malloc(sizeof(ExecutableUnit));
            returnStatement->kind = RETURN_NOW;
            returnStatement->firstInstAddr = endingInstruction->address;

            insertUnitIntoProgram(out, returnStatement);

            // *tailNullptr = returnStatement;
            // tailNullptr = &(returnStatement->next);


        } else if (endingInstruction->id == X86_INS_CALL) {

            ExecutableUnit* writeCall = malloc(sizeof(ExecutableUnit));
            writeCall->kind = WRITE_CALL;
            writeCall->firstInstAddr = endingInstruction->address;

            writeCall->info.writeCall.charLen = createRegOp(X86_REG_RDX);
            writeCall->info.writeCall.charPtr = createRegOp(X86_REG_RSI);
            writeCall->info.writeCall.writeTo = createRegOp(X86_REG_RDI);

            writeCall->next = NULL;

            // Want to make a new block right after this call!
            insertNextToBlock(&toBeBlocked, currentAddr, false);
            insertUnitIntoProgram(out, writeCall);

            // *tailNullptr = writeCall;
            // tailNullptr = &(writeCall->next);

        } else if (isJumpInsn(endingInstruction->id)) {

            cs_x86 detail = endingInstruction->detail->x86;
            if (detail.op_count != 1) {
                printf("Bad op count for jump\n");
                break;
            } else if (detail.operands[0].type != X86_OP_IMM) {
                printf("ERROR: Non-immmediate jumps not supported\n");
                break;
            }        
            
            ExecutableUnit* jumpInsn = malloc(sizeof(ExecutableUnit));
            jumpInsn->firstInstAddr = endingInstruction->address;
            jumpInsn->kind = JUMP_INSN;

            OperationKind compareKind;
            switch (endingInstruction->id) {
                case X86_INS_JMP:
                    compareKind = 0;
                    break;
                case X86_INS_JE:
                    compareKind = EQUAL;
                    break;
                case X86_INS_JNE:
                    compareKind = NOT_EQUAL;
                    break;
                case X86_INS_JNS:
                case X86_INS_JG:
                    compareKind = GREATER;
                    break;
                case X86_INS_JL:
                    compareKind = LESS;
                    break;
                case X86_INS_JLE:
                    compareKind = LESS_OR_EQ;
                    break;
                case X86_INS_JGE:
                    compareKind = GREATER_OR_EQ;
                    break;
                default:
                    // unreachable;
                    break;  
            }

            if (compareKind) {
#ifdef DEBUG_PRINT
                char buff[128] = {0};
                operationToStr(newBlock->lastFlagSet, buff, handle);
                printf("lastFlagOp: %s\n", buff);
#endif /* ifdef DEBUG_PRINT */
                Operation* condition = deepCopyOperation(newBlock->lastFlagSet);
                condition->kind = compareKind;
                jumpInsn->info.jumpInsn.condition = condition;
                // Could potentially not jump.
                insertNextToBlock(&toBeBlocked, currentAddr, false);
                // printf("Done\n");

            } else {
                jumpInsn->info.jumpInsn.condition = NULL;
            }

            Elf64_Addr jumpDest = detail.operands[0].imm;

            uint jumpDestId = out->numJumpDests;
            for(int i=0; i<out->numJumpDests; i++) {
                if (out->jumpDestLookup[i] == jumpDest) {
                    jumpDestId = i;
                    break;
                }
            }
            jumpInsn->info.jumpInsn.destId = jumpDestId;

            insertUnitIntoProgram(out, jumpInsn);

            // If we are adding a new destination
            if (jumpDestId == out->numJumpDests){
                ExecutableUnit* newJumpDest = malloc(sizeof(ExecutableUnit));
                newJumpDest->kind = JUMP_DEST;
                newJumpDest->firstInstAddr = jumpDest;
                newJumpDest->info.jumpDestId = out->numJumpDests;

                // Add to the list
                out->jumpDestLookup[out->numJumpDests++] = jumpDest;

                bool shouldAddToToBeBlock;
                bool shouldDeleteReturn;
                ExecutableUnit** previousNextPtr = NULL;
                ExecutableUnit* jumpedInside = findJumpedInside(out, jumpDest, &shouldAddToToBeBlock, &shouldDeleteReturn, &previousNextPtr);

                if (shouldAddToToBeBlock) {
                    insertNextToBlock(&toBeBlocked, jumpDest, true);
                }
                if (shouldDeleteReturn) {

                    *previousNextPtr = newJumpDest;
                    newJumpDest->next = jumpedInside->next;

                    insertNextToBlock(&toBeBlocked, jumpedInside->firstInstAddr, false);
                    insertNextToBlock(&toBeBlocked, jumpDest, true);

                    deleteExecutableUnit(jumpedInside);

                } else {
                    insertUnitIntoProgram(out, newJumpDest);
                }
            } else {
                // Do nothing!
                // We have already handled jumping here, since there's already a dest object here
            }
        }

afterFollowInsn:

        if(shouldDeleteBlock) {
            deleteCodeBlock(newBlock);
        }

#ifdef DEBUG_PRINT
        printParsedProgram(out);
#endif

    }

    deepPrintParsedProgram(out, handle);


    return out;
}

ExecutableUnit* findJumpedInside(ParsedProgram* program, Elf64_Addr addr, bool* shouldAddToToBeBlock, bool* shouldDeleteReturn, ExecutableUnit*** previousNextPtr){
    if (program->head == NULL) {
        *shouldAddToToBeBlock = true;
        *shouldDeleteReturn = false;
        return NULL;
    }
    if(addr < program->head->firstInstAddr) {
        *shouldAddToToBeBlock = true;
        *shouldDeleteReturn = false;
        return NULL;
    } else if (addr == program->head->firstInstAddr){
        *shouldAddToToBeBlock = false;
        *shouldDeleteReturn = false;
        return NULL;
    }

    ExecutableUnit** previous = &(program->head);
    ExecutableUnit* current = program->head;

    while (current) {
        if (current->firstInstAddr > addr) {
            // Continue
        } else if(current->kind != CODE_BLOCK) {
            // Continue
        } else if(addr < current->info.block->nextInstAddr){
            *previousNextPtr = previous;
            *shouldDeleteReturn = true;
            *shouldAddToToBeBlock = true;
            return current;
        }
        previous = &(current->next);
        current = current->next;
    }

    // After
    *shouldAddToToBeBlock = true;
    *shouldDeleteReturn = false;
    return NULL;

}
// Returns a code block. (NO WAY!)
// Last instruction in block->instructions is the one that triggered this ending. It has not impacts on this block
// block->nextInstAddr points to the one after that
CodeBlock* createCodeBlock(Elf64_Addr startAddr,
                           ParsedElf* elf,
                           Elf64_Addr maxAddr, // All instructions used must have addresses < this. 0 for no limit
                           csh* csHandle){

    handle = *csHandle;

    CodeBlock* block = initCodeBlock();
    block->firstInstAddr = startAddr;

    Elf64_Addr currentAddr = startAddr;

    // Get which segment we are reading from;
    int segInd = getVAddrIndex(elf, currentAddr);
    if (segInd == -1)
        return block;

    const uint8_t* inCode = &(elf->loadedSegments[segInd][currentAddr - elf->segmentMemLocations[segInd]]);
    size_t len = (elf->segmentMemLens[segInd] - (currentAddr - elf->segmentMemLocations[segInd]));

    int good;
    cs_insn* insn;

    do {
        // Initialize a blank instruction to be read
        appendBlankInsn(block, csHandle);
        insn = block->instructions[block->instructionCount-1];


        // Get a pointer into that segment

        good = cs_disasm_iter(*csHandle, &inCode,
                       &len,
                       &currentAddr, 
                       insn);

        if (!good)
            break;
        if (maxAddr && insn->address >= maxAddr)
            break;

#ifdef DEBUG_PRINT
        printf("0x%"PRIx64":\t%s\t\t%s\n", insn->address, insn->mnemonic,insn->op_str);
#endif /* ifdef DEBUG_PRINT */

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
            case X86_INS_CALL:
            case X86_INS_RET:
                good = false;
                break;
            case X86_INS_CMP:
                updateFlagOp(block, insn, SUB);
                break;
            case X86_INS_TEST:
                updateFlagOp(block, insn, BW_AND);
                break;
            default:
                if (isJumpInsn(insn->id)) {
                    good = false;
                    break;
                }
                printf("Instruction: %s unknown :(\n", insn->mnemonic);
        }

        // printImpacts(block, *csHandle);
    } while (good);

    block->nextInstAddr = currentAddr;

    return block;
}


void updateImpactsOfMov(CodeBlock* block, cs_insn* insn){
    cs_x86 detail = insn->detail->x86;
    if (detail.op_count != 2){
        printf("!Bad operand count for mov\n");
    }

    // Evil hack because we hate the stack
    if (detail.operands[0].type == X86_OP_REG && detail.operands[0].reg == X86_REG_RBP &&
        detail.operands[1].type == X86_OP_REG && detail.operands[1].reg == X86_REG_RSP) {
        return;
    }


    CodeImpact* impactToUpdate = getImpactToUpdate(block, detail.operands[0]);
    Operation** operationToUpdate = &impactToUpdate->impact;

    Operation* valueAssigned;

    bool print = false;

    if (detail.operands[1].type == X86_OP_REG) {
        // Copy value of register or just run it
        valueAssigned = copyLookupOrCreateRegOp(block, detail.operands[1].reg);
    } else if (detail.operands[1].type == X86_OP_IMM) {
        print = true;
        valueAssigned = createDataOperation(LITERAL, &detail.operands[1].imm);
    } else if (detail.operands[1].type == X86_OP_MEM) {
        valueAssigned = derefMem(block, detail.operands[1].mem);
    } else {
        printf("AAAAA!! Bad type stored in mov");
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

    // char test[256] = {0};
    // operationToStr(operation, test, handle);
    // printf("Arithmetic: %s\n", test);

    CodeImpact* impactToUpdate = getImpactToUpdate(block, detail.operands[0]);
    Operation** operationToUpdate = &impactToUpdate->impact;

    if (*operationToUpdate) {
        deleteOperation(*operationToUpdate);
    }

    *operationToUpdate = operation;

    if (operation->kind == ADD || 
        operation->kind == SUB || 
        operation->kind == DIV || 
        operation->kind == BW_AND || 
        operation->kind == BW_OR || 
        operation->kind == BW_XOR) {
        updateFlagOp(block, insn, operation->kind);
    }
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

void updateFlagOp(CodeBlock* block, cs_insn* insn, OperationKind kind) {
    // printf("Updating flag!\n");
    cs_x86 detail = insn->detail->x86;
    Operation* operation = calloc(sizeof(Operation), 1);

    operation->kind = kind;

    Operation* operands[2];

    for (int i=0; i<2; i++) {
        if (detail.operands[i].type == X86_OP_REG) {
            // Copy value of register or just run it
            operands[i] = createDataOperation(REGISTER, &detail.operands[i].reg);
        } else if (detail.operands[i].type == X86_OP_IMM) {
            operands[i] = createDataOperation(LITERAL, &detail.operands[i].imm);
        } else if (detail.operands[i].type == X86_OP_MEM) {
            Operation* prim = memoryOpPrimitive(detail.operands[i].mem);
            Operation* derefOp = calloc(sizeof(Operation), 1);
            derefOp->kind = DEREF;
            derefOp->info.unaryOperand = prim;
            operands[i] = derefOp;
        } else {
            printf("AAAAA! Bad operand to instruction: %s\n", insn->op_str);
        }
    }
    
    operation->info.binaryOperands.op1 = operands[0];
    operation->info.binaryOperands.op2 = operands[1];
    operation->width = detail.operands[0].size;

    deleteOperation(block->lastFlagSet);
    block->lastFlagSet = operation;
}

CodeImpact* getImpactToUpdate(CodeBlock* block, cs_x86_op op) {
    Operation* destLocation;
    x86_reg impactedSegment = X86_REG_INVALID;

    if (op.type == X86_OP_REG) {
        // Storing in regiser
        x86_reg reg = op.reg;
        if (registerIs32bit(reg)) {
            reg = get64bitParent(reg);
        }
        destLocation = createDataOperation(REGISTER, &reg);
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

Operation* memoryOpPrimitive(x86_op_mem mem) {
    Operation* base = NULL;
    if (mem.base)
        base = createDataOperation(REGISTER, &mem.base);

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

        Operation* index = createDataOperation(REGISTER, &mem.index);

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
            // printf("Found matching!\n");
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
    if (registerIs32bit(reg)) {
        reg = get64bitParent(reg);
    }

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
    Operation* locationDereffed = calloc(sizeof(Operation), 1);
    locationDereffed->kind = DEREF;
    locationDereffed->info.unaryOperand = memoryOp(block, mem);

    Operation* dereffedValue = findAndCopyImpactOperation(block, locationDereffed);

    if (dereffedValue) {
        deleteOperation(locationDereffed);
    } else {
        dereffedValue = locationDereffed;
    }
    
    return dereffedValue;
}

bool registerIs32bit(x86_reg reg){
    return 
        reg == X86_REG_EAX ||
        reg == X86_REG_EBP ||
        reg == X86_REG_EBX ||
        reg == X86_REG_ECX ||
        reg == X86_REG_EDI ||
        reg == X86_REG_EDX ||
        reg == X86_REG_EIP ||
        reg == X86_REG_ESI ||
        reg == X86_REG_ESP;
}
// bool registerIs64bit(x86_reg reg);
x86_reg get64bitParent(x86_reg reg){
    return 
        reg == X86_REG_EAX ? X86_REG_RAX :
        reg == X86_REG_EBP ? X86_REG_RBP :
        reg == X86_REG_EBX ? X86_REG_RBX :
        reg == X86_REG_ECX ? X86_REG_RCX :
        reg == X86_REG_EDI ? X86_REG_RDI :
        reg == X86_REG_EDX ? X86_REG_RDX :
        reg == X86_REG_EIP ? X86_REG_RIP :
        reg == X86_REG_ESI ? X86_REG_RSI :
        reg == X86_REG_RSP;
}
// x86_reg get32bitChild(x86_reg reg);

bool isJumpInsn(x86_insn insn) {
    return
        insn == X86_INS_JMP ||
        insn == X86_INS_JE ||
        insn == X86_INS_JNE ||
        insn == X86_INS_JG ||
        insn == X86_INS_JL ||
        insn == X86_INS_JLE ||
        insn == X86_INS_JNS ||
        insn == X86_INS_JGE;
}

ExecutableUnit* findUnitAt(ParsedProgram* program, Elf64_Addr addr){
    ExecutableUnit* current = program->head;
    while(current) {
        if (current->firstInstAddr == addr) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

Operation* createRegOp(x86_reg reg) {
    Operation* newOp = calloc(1, sizeof(Operation));
    newOp->kind = DATA;
    newOp->info.data.kind = REGISTER;
    newOp->info.data.info.reg = reg;
    return newOp;
}
