#include "cfgRecovery.h"

#include "asmParser.h"
#include "datastructs.h"
#include <stdint.h>
#include <stdio.h>

// NOTE: There is potentially some really nasty bugs if it doesn't end with a return.
// Oh well.
StructuredCodeTree* initBaseAndResolveDependencies(ParsedProgram* program) {
    StructuredCodeTree* out = calloc(1, sizeof(*out));

    uint* jumpDestIdToCfgNodeLookup = calloc(program->numJumpDests, sizeof(uint));

    ExecutableUnit* current = program->head;
    for (; current; current=current->next) {
        if (current->kind == JUMP_DEST) {
            current->previousCoupled = NULL;
            jumpDestIdToCfgNodeLookup[current->info.jumpDestId] = out->numCfgNodes+1;
            current = current->next;
            if (current->kind == JUMP_DEST) {
                printf("HUH???? 2 Jump dests in a row!\n");
                return NULL;
            }
        }
        
        printf("Creating for addr: %lx\n", current->firstInstAddr);

        StructuredCfgNode* cfgNode = &(out->cfgNodes[out->numCfgNodes]);
        cfgNode->kind = BASE;
        cfgNode->id = out->numCfgNodes + 1;

        BaseCfgNode* baseNode = &cfgNode->info.base;

        baseNode->startUnit = current;
        baseNode->numUnits = 1;

        current->previousCoupled = NULL;

        // Setup number of units and double link
        while (current->kind == CODE_BLOCK || current->kind == WRITE_CALL) {
            // We're doubly linking but only within an ExecutableUnit.
            // Could be global but whatever
            current->next->previousCoupled = current;
            current = current->next;
            baseNode->numUnits++;
        }
        // Can't be first in cfgNode by assertion in if at start of for
        if (current->kind == JUMP_DEST) {
            baseNode->numUnits -= 1;
            // Need to let the loop naturally advance onto this dest
            current = current->previousCoupled;
            // Just in case
            current->next->previousCoupled = NULL;
        }

        baseNode->lastUnit = current;

        printf("Last unit: %lx\n", baseNode->lastUnit->firstInstAddr);

        ExecutableUnit* needsDepVals = baseNode->startUnit->next;
        for (int i=1; i<baseNode->numUnits; i++) {

            // Don't need to worry about case where needs is 1st due to i=1 in for loop.
            uint numDependencies = 0;
            // Array of pointers to operation*
            Operation*** dependencies = locateDependencies(needsDepVals, &numDependencies);

            printf("%d deps located\n", numDependencies);

            ExecutableUnit* providesDepVals = needsDepVals->previousCoupled;
            for (;providesDepVals; providesDepVals = providesDepVals->previousCoupled) {

                if (providesDepVals->kind != CODE_BLOCK)
                    continue;

                CodeBlock* provider = providesDepVals->info.block;

                for (int j=0; j<numDependencies; j++) {
                    Operation** toBeReplaced = dependencies[j];
                    // Don't wanna replace with older info
                    if (!toBeReplaced)
                        continue;
                    Operation* valueProvided = findAndCopyImpactOperation(provider, *toBeReplaced);

                    if (valueProvided) {
                        deleteOperation(*toBeReplaced);
                        *toBeReplaced = valueProvided;
                        // Don't wanna replace with older info
                        dependencies[j] = NULL;
                    }
                }
            }
            free(dependencies);

            needsDepVals = needsDepVals->next;
        }
        
        // Current is the last in a cfgNode
        // current->Next should either be a jump dest or normal block if
        out->numCfgNodes++;
    }

    for (int i=0; i<out->numCfgNodes; i++) {
        StructuredCfgNode* cfgNode = &out->cfgNodes[i];
        ExecutableUnit* lastOfCfg = cfgNode->info.base.lastUnit;

        switch (lastOfCfg->kind) {
            case CODE_BLOCK:
            case WRITE_CALL:
                // +2 since id is i+1 since 0 is no jump
                cfgNode->after1 = i+2;
                cfgNode->after2 = 0;
                break;
            case RETURN_NOW:
                // No followers
                cfgNode->after1 = 0;
                cfgNode->after2 = 0;
                break;
            case JUMP_INSN:
                cfgNode->after1 = jumpDestIdToCfgNodeLookup[lastOfCfg->info.jumpInsn.destId];
                if (lastOfCfg->info.jumpInsn.condition) {
                    // Conditional jump = we could go to next cfgnode
                    cfgNode->after2 = i+2;
                } else {
                    // condition is NULL. No second following node
                    cfgNode->after2 = 0;
                }
                break;
            case JUMP_DEST:
                // Uh oh
                printf("Unreachable jump_dest last in cfgNode reached\n");
                return NULL;
        }

        // Add this node as a possible previous node to its successors
        addPossiblePrevious(out, cfgNode->id, cfgNode->after1);
        addPossiblePrevious(out, cfgNode->id, cfgNode->after2);

    }

    return out;
}



void printCfg(StructuredCodeTree* tree) {
    for (int i=0; i<tree->numCfgNodes; i++) {
        StructuredCfgNode node = tree->cfgNodes[i];

        printf(" |Pre:");
        for (int j=0; j<tree->numCfgNodes; j++) {
            if (setContains(&node.possiblePredecessors, tree->cfgNodes[j].id)) {
                printf(" %d", tree->cfgNodes[j].id);
            }
        }
        printf(" |\n");

        printf(" |Node : %-2d|\n", node.id);
        printf(" |Kind : %-2d|\n", node.kind);
        if (node.after1 && node.after2) {
            printf("  /      \\\n");
            printf(" %02d      %02d\n", node.after1, node.after2);
        } else if (node.after1) {
            printf("     |     \n");
            printf("     %02d\n", node.after1);
        }
    }
}


// All the helper junk!!!!


void addPossiblePrevious(StructuredCodeTree *tree, uint idBefore, uint idAfter) {
    
    if (idAfter == 0 || idBefore == 0)
        return;

    printf("Add %d, %d\n", idBefore, idAfter);
    setAdd(&tree->cfgNodes[idAfter-1].possiblePredecessors, idBefore);

}

void setAdd(CfgNodeSet* set, uint num){
    int ind = 0;
    while (num >= 64) {
        num /= 64;
        ind ++;
    }
    set->backing[ind] |= 1 << (num);
}

bool setContains(CfgNodeSet* set, uint num){
    int ind = 0;
    while (num >= 64) {
        num /= 64;
        ind ++;
    }
    return (set->backing[ind] & 1 << (num)) > 0;
}
