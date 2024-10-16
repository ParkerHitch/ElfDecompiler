#include "cfgRecovery.h"

#include "asmParser.h"
#include "datastructs.h"
#include <stdint.h>
#include <stdio.h>

// NOTE: There is potentially some really nasty bugs if it doesn't end with a return.
// Oh well.
Cfg* makeCfgAndResolveDependencies(ParsedProgram* program) {
    Cfg* out = calloc(1, sizeof(*out));

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

        CfgNode* cfgNode = &(out->cfgNodes[out->numCfgNodes]);

        cfgNode->startUnit = current;
        cfgNode->numUnits = 1;

        current->previousCoupled = NULL;

        // Setup number of units and double link
        while (current->kind == CODE_BLOCK || current->kind == WRITE_CALL) {
            // We're doubly linking but only within an ExecutableUnit.
            // Could be global but whatever
            current->next->previousCoupled = current;
            current = current->next;
            cfgNode->numUnits++;
        }
        // Can't be first in cfgNode by assertion in if at start of for
        if (current->kind == JUMP_DEST) {
            cfgNode->numUnits -= 1;
            // Need to let the loop naturally advance onto this dest
            current = current->previousCoupled;
            // Just in case
            current->next->previousCoupled = NULL;
        }

        cfgNode->lastUnit = current;

        printf("Last unit: %lx\n", cfgNode->lastUnit->firstInstAddr);

        ExecutableUnit* needsDepVals = cfgNode->startUnit->next;
        for (int i=1; i<cfgNode->numUnits; i++) {

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
        CfgNode* cfgNode = &out->cfgNodes[i];
        ExecutableUnit* lastOfCfg = cfgNode->lastUnit;

        switch (lastOfCfg->kind) {
            case CODE_BLOCK:
            case WRITE_CALL:
                // +2 since id is i+1 since 0 is no jump
                cfgNode->follow1 = i+2;
                cfgNode->follow2 = 0;
                break;
            case RETURN_NOW:
                // No followers
                cfgNode->follow1 = 0;
                cfgNode->follow2 = 0;
                break;
            case JUMP_INSN:
                cfgNode->follow1 = jumpDestIdToCfgNodeLookup[lastOfCfg->info.jumpInsn.destId];
                if (lastOfCfg->info.jumpInsn.condition) {
                    // Conditional jump = we could go to next cfgnode
                    cfgNode->follow2 = i+2;
                } else {
                    // condition is NULL. No second following node
                    cfgNode->follow2 = 0;
                }
                break;
            case JUMP_DEST:
                // Uh oh
                printf("Unreachable jump_dest last in cfgNode reached\n");
                return NULL;
        }

    }

    return out;
}

void printCfg(Cfg* cfg) {
    for (int i=0; i<cfg->numCfgNodes; i++) {
        CfgNode node = cfg->cfgNodes[i];
        printf(" [Node : %-2d]\n", i+1);
        if (node.follow1 && node.follow2) {
            printf("  /      \\\n");
            printf(" %02d     %02d\n", node.follow1, node.follow2);
        } else if (node.follow1) {
            printf("     |     \n");
            printf("     %02d\n", node.follow1);
        }
    }
}
