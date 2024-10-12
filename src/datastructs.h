#include <capstone/capstone.h>
#include <capstone/x86.h>
#include <elf.h>
#include <stdint.h>


// ### Operations ###
// These are an AST, they do NOT correspond exactly to asm.
// So like an add op doesn't add to 1st opperand like is would in asm.

typedef enum _OperationKind {
    DATA,
    DEREF,

    ADD,
    SUB,
    MUL,
    DIV,

    EQUAL,
    NOTEQAL,
    GREATER,
    LESS,
    GREATER_OR_EQ,
    LESS_OR_EQ,

    FNCALL,
    FNRETURN,
    // NOTE: IF YOU EVER UPDATE THIS KNOW THAT WE ARE ASSUMING THAT
    // ONLY FNCALL, FNRETURN, & DEREF HAVE unaryOperand set. Everything else is binaryOperands
    // DATA is obviously data tho
} OperationKind;

// Tag for union ProgramData representing data that can be loaded/modified
typedef enum _ProgramDataKind {
    LITERAL,
    REGISTER,
    ADDRESS, // Into program memory
} ProgramDataKind;

// Tagged union representing data that can be loaded/modified during ops
typedef struct _ProgramData {
    ProgramDataKind kind;
    union {
        uint64_t lit;
        x86_reg reg;
        uint64_t adr; // Into program memory
    } info;
} ProgramData;

typedef struct _Operation {
    OperationKind kind;
    uint8_t width; // How many bytes this operation results in
    union {
        struct {
            struct _Operation* op1;
            struct _Operation* op2;
        } binaryOperands;

        struct _Operation* unaryOperand;
        ProgramData data;
    } info;

    // If not NULL, the bottom "overwrittenBy->width" bits of this operation
    // get overwritten by the the result of overwrittenBy;
    // NOTE: NOT ACTUALLY USED.
    // IF YOU START USING ACTUALLY ADD THIS TO LIKE DELETE & COPY FN
    struct _Operation* overwrittenBy;

} Operation;


// ### Ovseravable Impacts of Code ###
// Aka lines that appear in the C program + register modification

typedef struct _CodeImpact {

    // The location that gets modified by this code
    // Literals cannot be impacted by code, so this is either REGISTER or ADDRESS
    Operation* impactedLocation;
    // Segment this impact is located in if the location is in memory
    x86_reg segment;
    // An ast of operations whose result gets stored in the impacted location
    Operation* impact;

} CodeImpact;


// ### Code blocks ###

// After a code block executes, what happens?
typedef enum _AfterActionKind {
    RETURN,
    JUMP,
    CONDITIONAL_JUMP,
} AfterActionKind;

typedef struct _AfterAction {
    AfterActionKind kind;
    // If we're jumping, even conditionally, where would we go?
    uint64_t jumpAddr;
    // If we're jumpting conditionally, what's the condition?
    // This must be a comparason operation, obviously
    Operation* condition;
} AfterAction;

// Represents a bit of code that always gets executed together
typedef struct _CodeBlock {

    // I just really don't trust type sizes in C idk
    Elf64_Addr firstInstAddr;

    // Array of the instructions that get executed for this snippet
    uint instructionCount;
    uint instructionCapacity;
    cs_insn** instructions;

    // What happens after this block executes?
    AfterAction after;

    // Arry of observable effects of this block executing.
    uint impactCount;
    CodeImpact* impacts;
    uint impactCapacity;

    // Sections of memory or registers that are dependencies for this block
    uint dependencyCount;
    uint dependencyCapacity;
    ProgramData* dependencies;

} CodeBlock;

// ### Functions ###

// TODO: Functions
// hahahaha no. Maybe next time
typedef struct _FunctionBlock {

} FunctionBlock;



// ##### Helper functions #####

// Makes a blank code block
CodeBlock* initCodeBlock();

// Append an uninitialized instruction block's array
void appendBlankInsn(CodeBlock* block, csh* csHandle);

void deleteOperation(Operation* op);

Operation* deepCopyOperation(Operation* op);

Operation* createDataOperation(ProgramDataKind kind, void* data);

bool operationsEquivalent(Operation* op1, Operation* op2);

void printImpacts(CodeBlock* block, csh handle);

void operationToStr(Operation* op, char* outBuff, csh handle);
