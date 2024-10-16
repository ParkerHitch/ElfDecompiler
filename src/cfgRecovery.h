#include "datastructs.h"
#include "stdint.h"
#include <stdint.h>

// 128 nodes
#define MAX_CFG_NODES_DIV_64 2

typedef struct _CfgNode {

    uint numUnits; //Must be at least 1
    ExecutableUnit* startUnit;
    ExecutableUnit* lastUnit;

    // 0 for no follower
    uint follow1;
    uint follow2;

} CfgNode;

typedef struct _Cfg {
    ParsedProgram* baseProgram;

    uint numCfgNodes;
    CfgNode cfgNodes[MAX_CFG_NODES_DIV_64 * 64];

} Cfg;

Cfg* makeCfgAndResolveDependencies(ParsedProgram* program);

void printCfg(Cfg* cfg);

bool setContains(uint64_t set[], uint num);
void setAdd(uint64_t set[], uint num);
void setSub(uint64_t set[], uint num);
void setMerge(uint64_t set[], uint64_t set2[]);
uint64_t* duplicateSet(uint64_t set[]);

