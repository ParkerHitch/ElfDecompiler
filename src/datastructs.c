#include "datastructs.h"

#include "capstone/capstone.h"
#include <stdlib.h>
#include <string.h>

// Makes a blank code block
CodeBlock* initCodeBlock(){
    CodeBlock* out = malloc(sizeof(CodeBlock));

    out->instructionCount = 0;
    out->instructionCapacity = 10;
    out->instructions = malloc(sizeof(cs_insn*) * 10);

    out->impactCount = 0;
    out->impactCapacity = 10;
    out->impacts = malloc(sizeof(CodeImpact) * 10);

    out->nextInstAddr = 0;
    out->lastFlagSet = NULL;

    return out;
};

// 0 = data
// 1 = unary
// 2 = binary
uint numOperands(OperationKind kind) {
    switch (kind) {
        case DATA:
            return 0;
        case DEREF:
        case FNCALL:
        case FNRETURN:
            return 1;

        case BW_XOR:
        case BW_OR:
        case BW_AND:
        case BW_NOT:
        case ADD:
        case SUB:
        case MUL:
        case DIV:
        case EQUAL:
        case NOT_EQUAL:
        case GREATER:
        case LESS:
        case GREATER_OR_EQ:
        case LESS_OR_EQ:
        case LSHIFT:
        case RSHIFT:
            return 2;
        default:
            printf("Unknown number: %d\n", kind);
    }
}

// Append an uninitialized instruction block's array
void appendBlankInsn(CodeBlock* block, csh* csHandle){

    cs_insn* newInsn = cs_malloc(*csHandle);


    if (block->instructionCapacity == block->instructionCount+1) {
        block->instructions = realloc(block->instructions, sizeof(cs_insn*)*block->instructionCapacity*2);
        block->instructionCapacity *= 2;
    }

    block->instructions[block->instructionCount++] = newInsn;
}

Operation* createDataOperation(ProgramDataKind kind, void* data){
    Operation* newOp = malloc(sizeof(Operation));
    newOp->kind = DATA;
    newOp->info.data.kind = kind;
    switch (kind) {
        case LITERAL:
            newOp->info.data.info.lit = * ((typeof(newOp->info.data.info.lit)*) data );
            break;
        case REGISTER:
            newOp->info.data.info.reg = * ((typeof(newOp->info.data.info.reg)*) data );
            break;
        case ADDRESS:
            newOp->info.data.info.adr = * ((typeof(newOp->info.data.info.adr)*) data );
            break;
        case PARAM:
            newOp->info.data.info.paramInd = * ((typeof(newOp->info.data.info.paramInd)*) data );
            break;
    }
    return newOp;
}

void deleteOperation(Operation* op) {
    if (op==NULL)
        return;
    switch (numOperands(op->kind)) {
        case 2:
            deleteOperation(op->info.binaryOperands.op1);
            deleteOperation(op->info.binaryOperands.op2);
            break;
        case 1:
            deleteOperation(op->info.unaryOperand);
            break;
        case 0:
            break;
    }
    free(op);
}

Operation* deepCopyOperation(Operation* op) {
    // Should never hit I think
    if (op == NULL) {
        printf("NULL COPPIED!!!\n");
        return NULL;
    }
    Operation* copy = malloc(sizeof(Operation));
    copy->kind = op->kind;
    copy->width = op->width;
    switch (numOperands(op->kind)) {
        case 2:
            copy->info.binaryOperands.op1 = deepCopyOperation(op->info.binaryOperands.op1);
            copy->info.binaryOperands.op2 = deepCopyOperation(op->info.binaryOperands.op2);
            break;
        case 1:
            copy->info.unaryOperand = deepCopyOperation(op->info.unaryOperand);
            break;
        case 0:
            copy->info.data = op->info.data;
            break;
    }

    return copy;
}

bool operationsEquivalent(Operation* op1, Operation* op2){
    if (!op1 || !op2)
        return false;
    // if (op1->kind == op2->kind && op1->width==op2->width) {
    if (op1->kind == op2->kind) {
        switch (numOperands(op1->kind)) {
            case 2:
                return operationsEquivalent(op1->info.binaryOperands.op1, op2->info.binaryOperands.op1)
                       && operationsEquivalent(op1->info.binaryOperands.op2, op2->info.binaryOperands.op2);
                break;
            case 1:
                return operationsEquivalent(op1->info.unaryOperand, op2->info.unaryOperand);
                break;
            case 0:
                if (op1->info.data.kind == op2->info.data.kind) {
                    switch(op1->info.data.kind) {
                        case LITERAL:
                            return op1->info.data.info.lit == op2->info.data.info.lit;
                        case REGISTER:
                            return op1->info.data.info.reg == op2->info.data.info.reg;
                        case ADDRESS:
                            return op1->info.data.info.adr == op2->info.data.info.adr;
                    }
                }
                break;
        }
    }
    return false;
}

void printImpacts(CodeBlock* block, csh handle) {
    printf("IMPACTS: \n");
    for (int i=0; i<block->impactCount; i++) {
        char assignee[256] = {0};
        operationToStr(block->impacts[i].impactedLocation, assignee, handle);
        char assigned[1024] = {0};
        operationToStr(block->impacts[i].impact, assigned, handle);

        printf(" >  %s = %s;\n", assignee, assigned);
    } 
    printf("\n");
}

void operationToStr(Operation* op, char* outBuff, csh handle){
    if (op == NULL) {
        strcat(outBuff, "NULL");
        return;
    }

    switch (numOperands(op->kind)) {
        case 2: {
            outBuff[0] = '(';
            operationToStr(op->info.binaryOperands.op1, &(outBuff[1]), handle);

            char* operator;
            switch(op->kind) {
                case ADD:
                    operator = " + ";
                    break;
                case SUB:
                    operator = " - ";
                    break;
                case MUL:
                    operator = " * ";
                    break;
                case DIV:
                    operator = " / ";
                    break;
                case EQUAL:
                    operator = " == ";
                    break;
                case NOT_EQUAL:
                    operator = " != ";
                    break;
                case GREATER:
                    operator = " > ";
                    break;
                case LESS:
                    operator = " < ";
                    break;
                case GREATER_OR_EQ:
                    operator = " >= ";
                    break;
                case LESS_OR_EQ:
                    operator = " <= ";
                    break;
                case LSHIFT:
                    operator = " << ";
                    break;
                case RSHIFT:
                    operator = " >> ";
                    break;
                default:
                    printf("Printing invalid binaryOp\n");
            }

            strcat(outBuff, operator);

            int newStart = strlen(outBuff);

            operationToStr(op->info.binaryOperands.op2, &outBuff[newStart], handle);
            strcat(outBuff, ")");
        }
            break;
        case 1: {
            char* opname;
            switch(op->kind) {
                case DEREF:
                    opname = "DEREF ";
                    break;
                default:
                    printf("Printing invalid unaryop: %d\n", op->kind);
            }

            strcat(outBuff, opname);
            int newStart = strlen(outBuff);

            operationToStr(op->info.unaryOperand, &outBuff[newStart], handle);
        }
            break;
        case 0:
            switch(op->info.data.kind) {
                case LITERAL:
                    sprintf(outBuff, "%ld", op->info.data.info.lit);
                    break;
                case REGISTER:
                    sprintf(outBuff, "%s", cs_reg_name(handle, op->info.data.info.reg));
                    break;
                case ADDRESS:
                    sprintf(outBuff, "A0x%x", op->info.data.info.reg);
                    break;
            }
            break;
        default:
            printf("WTF!!!\n");
    }
}

void deepPrintParsedProgram(ParsedProgram* program, csh handle){
    ExecutableUnit* unit = program->head;
    int count = 100;
    char buff[256];

    while(unit && count > 0) {
        printf(" 0x%08lx : ", unit->firstInstAddr);
        switch(unit->kind) {
            case CODE_BLOCK:
                printf("Block\n");
                printImpacts(unit->info.block, handle);
                break;
            case WRITE_CALL:
                printf("Write(\n");
                memset(buff, 0, 256);
                operationToStr(unit->info.writeCall.writeTo, buff, handle);
                printf("    %s,\n", buff);
                memset(buff, 0, 256);
                operationToStr(unit->info.writeCall.charPtr, buff, handle);
                printf("    %s,\n", buff);
                memset(buff, 0, 256);
                operationToStr(unit->info.writeCall.charLen, buff, handle);
                printf("    %s\n);\n", buff);
                break;
            case RETURN_NOW:
                printf("Return\n");
                break;
            case JUMP_DEST:
                printf("Dest %d:\n", unit->info.jumpDestId);
                break;
            case JUMP_INSN:
                if (unit->info.jumpInsn.condition) {
                    memset(buff, 0, 256);
                    operationToStr(unit->info.jumpInsn.condition, buff, handle);
                    printf("Condition: %s\n", buff);
                    printf("              JumpTo: %d\n", unit->info.jumpInsn.destId);
                } else {
                    printf("JumpTo: %d\n", unit->info.jumpInsn.destId);
                }
                break;
        }
        unit = unit->next;
        count --;
    }
}

void printParsedProgram(ParsedProgram* program){
    ExecutableUnit* unit = program->head;
    int count = 100;

    while(unit && count > 0) {
        printf(" 0x%08lx : ", unit->firstInstAddr);
        switch(unit->kind) {
            case CODE_BLOCK:
                printf("Block\n");
                break;
            case WRITE_CALL:
                printf("Call\n");
                break;
            case RETURN_NOW:
                printf("Return;\n");
                break;
            case JUMP_DEST:
                printf("Dest %d\n", unit->info.jumpDestId);
                break;
            case JUMP_INSN:
                printf("JumpTo: %d\n", unit->info.jumpInsn.destId);
                break;
        }
        unit = unit->next;
        count --;
    }

}

void deleteCodeBlock(CodeBlock* block){
    // NOTE:
    // Mmmm I love leaking memory 😋
}

void deleteExecutableUnit(ExecutableUnit* block){
    // NOTE:
    // Mmmm I love leaking memory 😋
}

typedef struct _DepArr{
    uint cap;
    uint len;
    Operation*** arr;
} DepArr;

void appendToArr(DepArr* arr, Operation** new) {
    if (arr->len == arr->cap) {
        arr->cap *= 2;
        arr->arr = realloc(arr->arr, sizeof(Operation**) * arr->cap);
    }
    arr->arr[arr->len++] = new;
}

void addDepsRecursive(Operation** op, DepArr* arr) {
    if (op==NULL)
        return;
    // I <3 switches :D
    switch ((*op)->kind) {
        case DATA:
            if ((*op)->info.data.kind == REGISTER) {
                appendToArr(arr, op);
            }
            break;
        case DEREF:
            appendToArr(arr, op);
            break;
        default:
            switch (numOperands((*op)->kind)) {
                case 2:
                    addDepsRecursive(&(*op)->info.binaryOperands.op1, arr);
                    addDepsRecursive(&(*op)->info.binaryOperands.op2, arr);
                    break;
                case 1:
                    addDepsRecursive(&(*op)->info.unaryOperand, arr);
                    break;
            }
    }
}

Operation*** locateDependencies(ExecutableUnit* hasDependencies, uint* numDependencies){
    if (hasDependencies->kind == RETURN_NOW) {
        *numDependencies = 0;
        return NULL;
    } else if (hasDependencies->kind == JUMP_DEST) {
        printf("Trying to find dependencies of dest. BAD!\n");
        *numDependencies = 0;
        return NULL;
    } else {
        DepArr arr;
        arr.len = 0;
        arr.cap = 10;
        arr.arr = malloc(sizeof(Operation**) * arr.cap);
        if (hasDependencies->kind == JUMP_INSN) {
            if (hasDependencies->info.jumpInsn.condition) {
                addDepsRecursive(&hasDependencies->info.jumpInsn.condition, &arr);
            }
        } else if (hasDependencies->kind == WRITE_CALL) {
            addDepsRecursive(&hasDependencies->info.writeCall.writeTo, &arr);
            addDepsRecursive(&hasDependencies->info.writeCall.charPtr, &arr);
            addDepsRecursive(&hasDependencies->info.writeCall.charLen, &arr);
        } else if (hasDependencies->kind == CODE_BLOCK) {
            CodeBlock* block = hasDependencies->info.block;
            for (int i=0; i<block->impactCount; i++) {
                addDepsRecursive(&block->impacts[i].impact, &arr);
            }
        }
        *numDependencies = arr.len;
        return arr.arr;
    }
}
