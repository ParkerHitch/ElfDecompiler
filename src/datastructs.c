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


    out->dependencyCount = 0;
    out->dependencyCapacity = 10;
    out->dependencies = malloc(sizeof(ProgramData*) * 10);

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

        case ADD:
        case SUB:
        case MUL:
        case DIV:
        case EQUAL:
        case NOTEQAL:
        case GREATER:
        case LESS:
        case GREATER_OR_EQ:
        case LESS_OR_EQ:
            return 2;
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
    if (op1->kind == op2->kind && op1->width==op2->width) {
        switch (numOperands(op1->kind)) {
            case 2:
                return operationsEquivalent(op1->info.binaryOperands.op1, op2->info.binaryOperands.op1);
                return operationsEquivalent(op1->info.binaryOperands.op2, op2->info.binaryOperands.op2);
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
        char assigned[256] = {0};
        operationToStr(block->impacts[i].impact, assigned, handle);

        printf(" >  %s = %s;\n", assignee, assigned);
    } 
    printf("\n");
}

void operationToStr(Operation* op, char* outBuff, csh handle){
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
                case NOTEQAL:
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
                    sprintf(outBuff, "0x%lx", op->info.data.info.lit);
                    break;
                case REGISTER:
                    sprintf(outBuff, "%s", cs_reg_name(handle, op->info.data.info.reg));
                    break;
                case ADDRESS:
                    sprintf(outBuff, "A0x%x", op->info.data.info.reg);
                    break;
            }
            break;
    }
}

