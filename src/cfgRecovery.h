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
            struct _StructuredCfgNode** nodes;
        } block;

        struct {
            struct _StructuredCfgNode* before;
            struct _StructuredCfgNode* body;
        } ifThen;

        struct {
            struct _StructuredCfgNode* before;
            struct _StructuredCfgNode* trueBody;
            struct _StructuredCfgNode* falseBody;
        } ifThenElse;

        struct {
            struct _StructuredCfgNode* condition;
            struct _StructuredCfgNode* body;
        } whileLoop;

        struct {
            struct _StructuredCfgNode* body;
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

void discoverStructure(StructuredCodeTree* tree);

void printCfg(StructuredCodeTree* tree);

void addPossiblePrevious(StructuredCodeTree* tree, uint idBefore, uint idAfter);

bool setContains(CfgNodeSet* set, uint num);
void setAdd(CfgNodeSet* set, uint num);
void setSub(CfgNodeSet* set, uint num);
CfgNodeSet setUnion(CfgNodeSet set1, CfgNodeSet set2);

#endif
