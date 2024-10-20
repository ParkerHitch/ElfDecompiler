#include "cGen.h"
#include "asmParser.h"
#include "cfgRecovery.h"
#include "datastructs.h"
#include <stdio.h>

#define EXPR_LEN 256
#define VAR_LEN 64

void writeRecursive(FILE* out, StructuredCodeTree* tree, uint depth, uint id, ParsedElf* elf);
void writeLine(FILE* out, CodeImpact* impact, uint depth);
void operationToCExpression(char* out, Operation* op, ParsedElf* elf);
bool isLocalVaiableAddr(Operation* op);
void localVaiableAddrToName(char* out, Operation* op);
void writeIndent(FILE* out, uint depth);
void writeImpacts(FILE* out, CodeBlock* block, uint depth, ParsedElf* elf);
Operation* getCondition(StructuredCodeTree* tree, uint node);
void logicalInvert(Operation* op);

void writeC(FILE* outFile, StructuredCodeTree* tree, ParsedElf* elf){

    // Boilerplate
    fprintf(outFile, "#include <unistd.h>\n\nint main(int argc, char** argv) {\n");

    writeRecursive(outFile, tree, 1, tree->rootNode, elf);

    fprintf(outFile, "}\n");
}

void writeRecursive(FILE* out, StructuredCodeTree* tree, uint depth, uint id, ParsedElf* elf){
    StructuredCfgNode* node = &tree->cfgNodes[id-1];
    switch (node->kind) {
        case BASE:{
            ExecutableUnit* unit = node->info.base.startUnit;
            for (int i=0; i<node->info.base.numUnits; (i++, unit=unit->next)) {
                if (unit->kind == CODE_BLOCK) {
                    writeImpacts(out, unit->info.block, depth, elf);
                } else if (unit->kind == WRITE_CALL) {
                    char params[3][VAR_LEN] = {{0}, {0}, {0}};
                    operationToCExpression(params[0], unit->info.writeCall.writeTo, elf);
                    operationToCExpression(params[1], unit->info.writeCall.charPtr, elf);
                    operationToCExpression(params[2], unit->info.writeCall.charLen, elf);
                    writeIndent(out, depth);
                    fprintf(out, "write(%s, %s, %s);\n", params[0], params[1], params[2]);
                } else if (unit->kind == RETURN_NOW) {
                    writeIndent(out, depth);
                    fprintf(out, "return;\n");
                }
            }
        }
            break;
        case BLOCK:
            for (int i=0; i<node->info.block.nodeCount; i++) {
                writeRecursive(out, tree, depth, node->info.block.nodes[i], elf);
            }
            break;
        case IF_THEN: {
            writeRecursive(out, tree, depth, node->info.ifThen.before, elf);
            Operation* cond = getCondition(tree, node->info.ifThen.before);
            logicalInvert(cond);
            char condStr[EXPR_LEN];
            operationToCExpression(condStr, cond, elf);
            writeIndent(out, depth);
            fprintf(out, "if (%s) {\n", condStr);
            writeRecursive(out, tree, depth+1, node->info.ifThen.body, elf);
            writeIndent(out, depth);
            fprintf(out, "}\n");
        }
            break;
        case IF_THEN_ELSE: {
            writeRecursive(out, tree, depth, node->info.ifThenElse.before, elf);
            Operation* cond = getCondition(tree, node->info.ifThenElse.before);
            char condStr[EXPR_LEN] = {0};
            operationToCExpression(condStr, cond, elf);
            writeIndent(out, depth);
            fprintf(out, "if (%s) {\n", condStr);

            writeRecursive(out, tree, depth+1, node->info.ifThenElse.trueBody, elf);

            writeIndent(out, depth);
            fprintf(out, "} else {\n");

            writeRecursive(out, tree, depth+1, node->info.ifThenElse.falseBody, elf);

            writeIndent(out, depth);
            fprintf(out, "}\n");
        }
            break;
        case INFINITE_LOOP: {
            writeIndent(out, depth);
            fprintf(out, "while (true) {\n");

            writeRecursive(out, tree, depth+1, node->info.infiniteLoop.body, elf);

            writeIndent(out, depth);
            fprintf(out, "}\n");
        }
            break;
        case DO_WHILE_LOOP: {

            Operation* cond = getCondition(tree, node->info.doWhileLoop.body);
            char condStr[EXPR_LEN] = {0};
            operationToCExpression(condStr, cond, elf);

            writeIndent(out, depth);
            fprintf(out, "do {\n");

            writeRecursive(out, tree, depth+1, node->info.doWhileLoop.body, elf);

            writeIndent(out, depth);
            fprintf(out, "} while(%s);\n", condStr);
        }
            break;
        case WHILE_LOOP: {

            Operation* cond = getCondition(tree, node->info.whileLoop.condition);
            char condStr[EXPR_LEN] = {0};
            operationToCExpression(condStr, cond, elf);

            writeIndent(out, depth);
            fprintf(out, "while(%s) {\n", condStr);

            writeRecursive(out, tree, depth+1, node->info.whileLoop.body, elf);

            writeIndent(out, depth);
            fprintf(out, "}\n");
        }
            break;
        default:
            printf("OTHER\n");
    }
}

Operation* getCondition(StructuredCodeTree* tree, uint nodeId){
    StructuredCfgNode* node = &(tree->cfgNodes[nodeId-1]);
    switch (node->kind) {
        case BASE:
            return node->info.base.lastUnit->info.jumpInsn.condition;
            break;
        case BLOCK:
            return getCondition(tree, node->info.block.nodes[node->info.block.nodeCount-1]);
            break;
        default:
            printf("invalid attempt to get condition\n");
    }
    return NULL;
}

void writeImpacts(FILE* out, CodeBlock* block, uint depth, ParsedElf* elf) {
    for (int i=0; i<block->impactCount; i++) {
        CodeImpact* impact = &block->impacts[i];
        if (impact->impactedLocation->kind == DEREF &&
            isLocalVaiableAddr(impact->impactedLocation->info.unaryOperand)) {
            char varname[VAR_LEN] = {0};
            char result[EXPR_LEN] = {0};
            localVaiableAddrToName(varname, impact->impactedLocation->info.unaryOperand);
            operationToCExpression(result, impact->impact, elf);
            writeIndent(out, depth);
            fprintf(out, "%s = %s;\n", varname, result);
        }
    }
}


void operationToCExpression(char* out, Operation* op, ParsedElf* elf){
    if (op == NULL) {
        return;
    }

    switch (numOperands(op->kind)) {
        case 2: {

            if (isLocalVaiableAddr(op)){
                out[0] = '&';
                localVaiableAddrToName(&(out[1]), op);
                return;
            }

            out[0] = '(';
            operationToCExpression(&(out[1]), op->info.binaryOperands.op1, elf);

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

            strcat(out, operator);

            int newStart = strlen(out);

            operationToCExpression(&out[newStart], op->info.binaryOperands.op2, elf);
            strcat(out, ")");
        }
            break;
        case 1: {
            if (op->kind==DEREF){
                if (isLocalVaiableAddr(op->info.unaryOperand)) {
                    localVaiableAddrToName(out, op->info.unaryOperand);
                } else {
                    out[0] = '*';
                    operationToCExpression(&(out[1]), op->info.unaryOperand, elf);
                }
            } else {
                printf("Printing invalid unaryop: %d\n", op->kind);
            }
        }
            break;
        case 0:
            switch(op->info.data.kind) {
                case LITERAL:
                    sprintf(out, "%ld", op->info.data.info.lit);
                    break;
                case REGISTER:
                    sprintf(out, "register:%d!", op->info.data.info.reg);
                    // sprintf(out, "%s", cs_reg_name(handle, op->info.data.info.reg));
                    break;
                case ADDRESS: {
                    uint8_t* val = readVAddr(elf, op->info.data.info.adr);
                    if (val && val[0]) {
                        out[0] = '"';
                        int i = 0;
                        int j = 1;
                        char c = val[0];
                        while (c) {
                            if (32 <= c && c <= 126) {
                                out[j++] = c;
                            } else if (c=='\n') {
                                out[j++] = '\\';
                                out[j++] = 'n';
                            } else {
                                sprintf(&(out[j]), "\\0x%02x", c);
                                j += 5;
                            }
                            i++;
                            c = val[i];
                        }
                        out[j] = '"';
                    } else {
                        sprintf(out, "0x%lx", op->info.data.info.adr);
                    }
                }
                    break;
                case PARAM:
                    if (op->info.data.info.paramInd == 0) {
                        sprintf(out, "argc");
                    } else {
                        sprintf(out, "argv");
                    }
                    break;
            }
            break;
        default:
            printf("WTF!!!\n");
    }
}

void logicalInvert(Operation* op){
    switch(op->kind) {
        case EQUAL:
            op->kind = NOT_EQUAL;
            break;
        case NOT_EQUAL:
            op->kind = EQUAL;
            break;
        case GREATER:
            op->kind = LESS_OR_EQ;
            break;
        case LESS_OR_EQ:
            op->kind = GREATER;
            break;
        case GREATER_OR_EQ:
            op->kind = LESS;
            break;
        case LESS:
            op->kind = GREATER_OR_EQ;
            break;
        default:
            printf("Invalid logic\n");
    }
}

bool isLocalVaiableAddr(Operation* op) {
    Operation* op1 = op->info.binaryOperands.op1;
    Operation* op2 = op->info.binaryOperands.op2;
    switch (op->kind) {
        case ADD:
        case SUB:
            return (
                op1->kind == DATA && op2->kind == DATA &&
                op1->info.data.kind == REGISTER &&
                op1->info.data.info.adr == X86_REG_RBP &&
                op2->info.data.kind == LITERAL
            );
        default:
            return false;
    }
}

void localVaiableAddrToName(char* out, Operation* op){
    sprintf(out, "var%ld", op->info.binaryOperands.op2->info.data.info.lit);
}

void writeIndent(FILE* out, uint depth) {
    for (int i=0; i<depth; i++) {
        fwrite("    ", 1, 4, out);
    }
}
