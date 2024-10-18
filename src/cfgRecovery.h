#ifndef __CFG_RECOVERY
#define __CFG_RECOVERY

#include "datastructs.h"
#include "stdint.h"
#include <stdint.h>

// 128 nodes
#define MAX_CFG_NODES_DIV_64 1

typedef struct _CfgNodeSet {
    uint64_t backing[MAX_CFG_NODES_DIV_64];
} CfgNodeSet;

typedef enum _StructureKind {
    BASE,
    BLOCK,
    IF_THEN,
    IF_THEN_ELSE,
    WHILE_LOOP,
    DO_WHILE_LOOP,
    INFINITE_LOOP
} StructureKind;

typedef struct _BaseCfgNode {
    uint numUnits; //Must be at least 1
    ExecutableUnit* startUnit;
    ExecutableUnit* lastUnit;
} BaseCfgNode;

typedef struct _StructuredCfgNode {

    CfgNodeSet possiblePredecessors;

    uint id;

    uint after1;
    uint after2;

    StructureKind kind;
    union {
        BaseCfgNode base;

        struct {
            uint nodeCount;
            uint* nodes;
        } block;

        struct {
            uint before;
            uint body;
        } ifThen;

        struct {
            uint before;
            uint trueBody;
            uint falseBody;
        } ifThenElse;

        struct {
            uint condition;
            uint body;
        } whileLoop;

        struct {
            uint body;
        } doWhileLoop;
    } info;

} StructuredCfgNode;

// Closest thing to an AST we're gonna get
typedef struct _StructuredCodeTree {
    ParsedProgram* baseProgram;

    uint numCfgNodes;
    StructuredCfgNode cfgNodes[MAX_CFG_NODES_DIV_64 * 64];

    uint rootNode;
} StructuredCodeTree;

StructuredCodeTree* initBaseAndResolveDependencies(ParsedProgram* program);

void rebuildStructure(StructuredCodeTree* tree);

void printCfg(StructuredCodeTree* tree);

void addPossiblePrevious(StructuredCodeTree* tree, uint idBefore, uint idAfter);

void updateAftersOfPredecessors(StructuredCodeTree* tree, CfgNodeSet possiblePredecessors, uint originalId, uint newId);

uint countAfter(StructuredCfgNode* node);

bool setContains(CfgNodeSet* set, uint num);
bool setsMatch(CfgNodeSet* set1, CfgNodeSet* set2);
bool setsMatchOrOneIsEmpty(CfgNodeSet* set1, CfgNodeSet* set2);
uint countMembers(CfgNodeSet set);
void setAdd(CfgNodeSet* set, uint num);
void setSub(CfgNodeSet* set, uint num);
CfgNodeSet setUnion(CfgNodeSet set1, CfgNodeSet set2);
CfgNodeSet blankSet();


#endif
