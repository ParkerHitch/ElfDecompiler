#include "cfgRecovery.h"

#include "asmParser.h"
#include "capstone/x86.h"
#include "datastructs.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// NOTE: There are potentially some really nasty bugs if it doesn't end with a return.
// Oh well.
StructuredCodeTree* initBaseAndResolveDependencies(ParsedProgram* program) {
    StructuredCodeTree* out = calloc(1, sizeof(*out));
    
    out->rootNode = 1;

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

        // Idk dude if I set this always to start/0 I get an error so we ball I guess
        ExecutableUnit* needsDepVals = out->numCfgNodes == 0 ? baseNode->startUnit : baseNode->startUnit->next;
        for (int i=out->numCfgNodes == 0 ? 0 : 1; i<baseNode->numUnits; i++) {

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
                        printf("Found!");
                        deleteOperation(*toBeReplaced);
                        *toBeReplaced = valueProvided;
                        // Don't wanna replace with older info
                        dependencies[j] = NULL;
                    }
                }
            }

            // Function params
            if (out->numCfgNodes == 0) {
                printf("Checking for params\n");
                // resolve param deps
                for (int j=0; j<numDependencies; j++) {
                    Operation** toBeReplaced = dependencies[j];
                    // Don't wanna replace with older info
                    if (!toBeReplaced)
                        continue;

                    Operation* valueProvided = NULL;
                    if ((*toBeReplaced)->kind == DATA &&
                        (*toBeReplaced)->info.data.kind == REGISTER) {

                        if ((*toBeReplaced)->info.data.info.reg == X86_REG_RDI) {
                            // Argc
                            uint8_t param = 0;
                            valueProvided = createDataOperation(PARAM, &param);
                        } else if ((*toBeReplaced)->info.data.info.reg == X86_REG_RSI) {
                            // Argv
                            uint8_t param = 1;
                            valueProvided = createDataOperation(PARAM, &param);
                        }
                    }

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

void readPostorder(StructuredCodeTree* tree, uint id, bool visited[], uint dfo[], uint* postorderCount);

void rebuildStructure(StructuredCodeTree* tree) {
    printf("Rebuilding!");

    bool visited[MAX_CFG_NODES_DIV_64 * 64] = {0};
    uint postorder[MAX_CFG_NODES_DIV_64 * 64] = {0};
    uint postorderCount = 0;
    readPostorder(tree, tree->rootNode, visited, postorder, &postorderCount);

    printf("Postorder:");
    for (int i=0; i<postorderCount; i++) {
        printf(" %d", postorder[i]);
    }
    printf("\n");

    int ind = 0;
    while (ind < postorderCount) {
        uint checkingId = postorder[ind];
        StructuredCfgNode checking = tree->cfgNodes[checkingId-1];

        StructuredCfgNode* after1 = checking.after1 ? &tree->cfgNodes[checking.after1-1] : NULL;
        StructuredCfgNode* after2 = checking.after2 ? &tree->cfgNodes[checking.after2-1] : NULL;

        StructuredCfgNode* newNode = &tree->cfgNodes[tree->numCfgNodes];
        newNode->id = tree->numCfgNodes + 1;
        newNode->kind = BASE;

        printNode(tree, checking);

        // BLOCK
        if (after1 &&
            !after2 &&
            countMembers(after1->possiblePredecessors) == 1) {
            // BLOCK
            printf("Found block with %d & %d", checkingId, after1->id);

            if (after1->kind == BLOCK) {
                printf(", Prepending %d to %d\n", checkingId, after1->id);

                // Evil hack to get them to check us again
                postorder[ind] = after1->id;
                ind--;

                // Add our node to the start of their block
                after1->info.block.nodeCount++;
                after1->info.block.nodes = realloc(after1->info.block.nodes, sizeof(uint) * after1->info.block.nodeCount);
                // Existing blocks always have size of at least 2. Count++ implies i >= 2 for first iter
                for (int i=after1->info.block.nodeCount-1; i>=1; i--) {
                    after1->info.block.nodes[i] = after1->info.block.nodes[i-1];
                }
                after1->info.block.nodes[0] = checkingId;

                after1->possiblePredecessors = checking.possiblePredecessors;
                updateAftersOfPredecessors(tree, checking.possiblePredecessors, checking.id, after1->id);

                if (checkingId == tree->rootNode)
                    tree->rootNode =  after1->id;

                printf("After %d is %d & %d", after1->id, after1->after1, after1->after2);

            } else {
                printf(", NewID: %d\n", newNode->id);
                // new block time!
                newNode->kind = BLOCK;
                newNode->info.block.nodeCount = 2;
                newNode->info.block.nodes = malloc(sizeof(uint) * 2);
                newNode->info.block.nodes[0] = checkingId;
                newNode->info.block.nodes[1] = after1->id;
                newNode->possiblePredecessors = checking.possiblePredecessors;
                newNode->after1 = after1->after1;
                newNode->after2 = after1->after2;

                StructuredCfgNode* newAfter1 = newNode->after1 ? &tree->cfgNodes[newNode->after1-1] : NULL;
                StructuredCfgNode* newAfter2 = newNode->after2 ? &tree->cfgNodes[newNode->after1-1] : NULL;

                if (newAfter1) {
                    setSub(&newAfter1->possiblePredecessors, after1->id);
                    setAdd(&newAfter1->possiblePredecessors, newNode->id);
                }
                if (newAfter2) {
                    setSub(&newAfter2->possiblePredecessors, after1->id);
                    setAdd(&newAfter2->possiblePredecessors, newNode->id);
                }

                updateAftersOfPredecessors(tree, checking.possiblePredecessors, checking.id, newNode->id);
            }
        // INFINITE_LOOP && DO_WHILE
        } else if (after1 &&
            after1->id == checkingId) {
            if (!after2) {
                // INFINITE_LOOP
                printf("Found infiniteLoop with %d, NewID: %d\n", checkingId, newNode->id);
                newNode->kind = INFINITE_LOOP;
                newNode->info.infiniteLoop.body = checkingId;
            } else {
                // DO_WHILE
                printf("Found doWhile with %d, NewID: %d\n", checkingId, newNode->id);
                newNode->kind = DO_WHILE_LOOP;
                newNode->info.doWhileLoop.body = checkingId;
            }

            newNode->possiblePredecessors = checking.possiblePredecessors;
            setSub(&newNode->possiblePredecessors, checkingId);
            newNode->after1 = checking.after2;
            newNode->after2 = 0;
            updateAftersOfPredecessors(tree, newNode->possiblePredecessors, checking.id, newNode->id);

        } else if (after1 && after2) {
            // IF THEN ELSE
            if (!setContains(&checking.possiblePredecessors, after1->id) &&
                !setContains(&checking.possiblePredecessors, after2->id) &&
                countMembers(after1->possiblePredecessors) == 1 &&
                countMembers(after2->possiblePredecessors) == 1 &&
                countAfter(after1) < 2 &&
                countAfter(after2) < 2 &&
                // match or one is 0. Needed for infinite loop or return
                ((after1->after1 == after2->after1) ||
                 (after1->after1==0 || after2->after1 == 0)) ) {

                printf("Found itE with %d & %d & %d, NewID: %d\n", checkingId, after1->id, after2->id, newNode->id);
                // IF THEN ELSE
                newNode->kind = IF_THEN_ELSE;
                newNode->possiblePredecessors = checking.possiblePredecessors;
                newNode->info.ifThenElse.before = checkingId;
                newNode->info.ifThenElse.trueBody = checking.after1;
                newNode->info.ifThenElse.falseBody = checking.after2;

                // Need to or in case one is 0
                uint trueAfterID = (after1->after1 | after2->after1);

                newNode->after1 = trueAfterID;
                newNode->after2 = 0;

                if (trueAfterID) {
                    StructuredCfgNode* trueAfter = &tree->cfgNodes[trueAfterID-1];
                    trueAfter->possiblePredecessors = blankSet();
                    setAdd(&trueAfter->possiblePredecessors, newNode->id);
                }

                updateAftersOfPredecessors(tree, checking.possiblePredecessors, checking.id, newNode->id);

            // IF THEN
            // after1 = after the statemtn
            // after2 = statement body
            } else if ( !setContains(&checking.possiblePredecessors, after1->id) &&
                        !setContains(&checking.possiblePredecessors, after2->id) &&
                        countMembers(after2->possiblePredecessors) == 1 &&
                        // 2 cases. Body has infinite loop/return or doesnt
                        // No return/loop
                        ((countAfter(after2)==1 &&
                              after2->after1 == after1->id &&
                              countMembers(after1->possiblePredecessors) == 2)
                         ||
                        // return/loop
                         (countAfter(after2)==0 &&
                              countMembers(after1->possiblePredecessors) == 1))) {
                // IF THEN
                printf("Found ifThen with %d & %d. NewID: %d\n", checkingId, after2->id, newNode->id);

                newNode->kind = IF_THEN;
                newNode->possiblePredecessors = checking.possiblePredecessors;
                newNode->info.ifThen.before = checkingId;
                newNode->info.ifThen.body = checking.after2;

                newNode->after1 = checking.after1;
                newNode->after2 = 0;

                after1->possiblePredecessors = blankSet();
                setAdd(&after1->possiblePredecessors, newNode->id);

                updateAftersOfPredecessors(tree, checking.possiblePredecessors, checking.id, newNode->id);
            // WHILE_LOOP
            } else if (checking.kind == BASE &&
                       countAfter(after1) <= 2 &&
                       setContains(&checking.possiblePredecessors, after1->id)) {
                // WHILE_LOOP
                printf("Found whileLoop with %d & %d, NewID: %d\n", checkingId, after1->id, newNode->id);
                newNode->kind = WHILE_LOOP;

                newNode->possiblePredecessors = checking.possiblePredecessors;
                setSub(&newNode->possiblePredecessors, after1->id);

                newNode->info.whileLoop.condition = checkingId;
                newNode->info.whileLoop.body = after1->id;

                newNode->after1 = checking.after2;
                newNode->after2 = 0;

                after2->possiblePredecessors = blankSet();
                setAdd(&after2->possiblePredecessors, newNode->id);

                updateAftersOfPredecessors(tree, checking.possiblePredecessors, checking.id, newNode->id);
            } else {
                printf("Unknown control structure checking: %d\n", checkingId);
            }
        } else {
            printf("Unknown control structure checking: %d\n", checkingId);
        }

        if (newNode->kind != BASE) {
            tree->numCfgNodes++;
            // Now consider the new node next in postorder
            // Don't increment ind and update array;
            if (postorder[ind] == tree->rootNode)
                tree->rootNode = newNode->id;
            postorder[ind] = newNode->id;
        } else {
            printf("inc");
            ind++;
        }
    }
}

void readPostorder(StructuredCodeTree* tree, uint id, bool visited[], uint postorder[], uint* postorderCount) {
    printf("id:%d \n", id);

    if (visited[id-1])
        return;

    visited[id-1] = true;

    StructuredCfgNode* node = &tree->cfgNodes[id-1];

    if (node->after1)
        readPostorder(tree, node->after1, visited, postorder, postorderCount);
    if (node->after2)
        readPostorder(tree, node->after2, visited, postorder, postorderCount);

    postorder[*postorderCount] = id;
    (*postorderCount)++;
}

int printRecursive(StructuredCodeTree* tree, uint id, bool visited[], uint depth){
    if (visited[id])
        return 0;
    visited[id] = true;
    for (int i=0; i<depth; i++)
        printf("  ");
    StructuredCfgNode* node = &tree->cfgNodes[id-1];
    printf("%02d ", id);
    switch (node->kind) {
        case BASE:
            printf("BASE\n");
            break;
        case BLOCK:
            printf("BLOCK\n");
            for (int i=0; i<node->info.block.nodeCount; i++) {
                if (!printRecursive(tree, node->info.block.nodes[i], visited, depth+1)){
                    for (int j=0; j<depth+1; j++) {
                        printf("  ");
                    }
                    printf("%d\n",node->info.block.nodes[i]);
                }
            }
            break;
        case IF_THEN:
            printf("IF_THEN\n");
            // printf("Before:\n");
            printRecursive(tree, node->info.ifThen.before, visited, depth+1);
            // printf("Body:\n");
            printRecursive(tree, node->info.ifThen.body, visited, depth+1);
            break;
        case IF_THEN_ELSE:
            printf("IF_THEN_ELSE\n");
            // printf("Before:\n");
            printRecursive(tree, node->info.ifThenElse.before, visited, depth+1);
            // printf("True:\n");
            printRecursive(tree, node->info.ifThenElse.trueBody, visited, depth+1);
            // printf("False:\n");
            printRecursive(tree, node->info.ifThenElse.falseBody, visited, depth+1);
            break;
        default:
            printf("OTHER\n");
    }
    return 1;
}

void printCfg(StructuredCodeTree* tree) {
    bool visited[MAX_CFG_NODES_DIV_64 * 64] = {0};
    // printRecursive(tree, tree->rootNode, visited, 0);
    for (int i=0; i<tree->numCfgNodes; i++) {
        printNode(tree, tree->cfgNodes[i]);
    }
}

void printNode(StructuredCodeTree* tree, StructuredCfgNode node) {
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


// All the helper junk!!!!


void addPossiblePrevious(StructuredCodeTree *tree, uint idBefore, uint idAfter) {
    
    if (idAfter == 0 || idBefore == 0)
        return;

    printf("Add %d, %d\n", idBefore, idAfter);
    setAdd(&tree->cfgNodes[idAfter-1].possiblePredecessors, idBefore);

}

uint countAfter(StructuredCfgNode* node){
    return node->after1 ? (node->after2?2:1) : 0;
}

void setAdd(CfgNodeSet* set, uint num){
    int ind = 0;
    while (num >= 64) {
        num /= 64;
        ind ++;
    }
    set->backing[ind] |= 1 << (num);
}

void setSub(CfgNodeSet* set, uint num){
    int ind = 0;
    while (num >= 64) {
        num /= 64;
        ind ++;
    }
    uint64_t mask = 1 << (num);
    mask = ~mask;
    set->backing[ind] &= mask;
}

bool setContains(CfgNodeSet* set, uint num){
    int ind = 0;
    while (num >= 64) {
        num /= 64;
        ind ++;
    }
    return (set->backing[ind] & 1 << (num)) > 0;
}

bool setsMatch(CfgNodeSet* set1, CfgNodeSet* set2) {
    for (int i=0; i<MAX_CFG_NODES_DIV_64; i++) {
        if (set1->backing[i] != set2->backing[i])
            return false;
    }
    return true;
}

bool setsMatchOrOneIsEmpty(CfgNodeSet* set1, CfgNodeSet* set2){
    int matchCount = 0;
    int zeroCount1 = 0;
    int zeroCount2 = 0;
    for (int i=0; i<MAX_CFG_NODES_DIV_64; i++) {
        if (set1->backing[i] == set2->backing[i])
            matchCount++;
        if (set1 == 0)
            zeroCount1++;
        if (set2 == 0)
            zeroCount2++;
    }
    if (matchCount == MAX_CFG_NODES_DIV_64)
        return true;
    // They don't match. One must be at least fully 0
    return zeroCount1==MAX_CFG_NODES_DIV_64 || zeroCount2==MAX_CFG_NODES_DIV_64;
}

uint countMembers(CfgNodeSet set){
    uint count = 0;
    for (int i=0; i<MAX_CFG_NODES_DIV_64; i++) {
        while (set.backing[i] > 0) {
            if (set.backing[i] % 2 == 1) {
                count ++;
            }
            set.backing[i] = set.backing[i] >> 1;
        }
    }
    return count;
}

void updateAftersOfPredecessors(StructuredCodeTree* tree, CfgNodeSet possiblePredecessors, uint originalId, uint newId){
    uint id = 0;
    for (int i=0; i<MAX_CFG_NODES_DIV_64; i++) {
        while (possiblePredecessors.backing[i] > 0) {
            if (possiblePredecessors.backing[i] % 2 == 1) {

                StructuredCfgNode* predecessor = &tree->cfgNodes[id-1];

                if (predecessor->after1 == originalId)
                    predecessor->after1 = newId;
                else if (predecessor->after2 == originalId)
                    predecessor->after2 = newId;
                else
                    printf("Incorrect assumption about predecessor. Tree bad.\n");

            }
            possiblePredecessors.backing[i] = possiblePredecessors.backing[i] >> 1;
            id++;
        }
    }
}

CfgNodeSet blankSet(){
    CfgNodeSet out;
    memset(&out, 0, sizeof(CfgNodeSet));
    return out;
}
