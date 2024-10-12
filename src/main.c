#include <stdio.h>
#include <string.h>

#include <capstone/capstone.h>

#include "elfParser.h"
#include "asmParser.h"

int main(int argc, char** argv) {

    if (argc != 2){
        printf("Please provide exactly 1 arg: the .elf you'd like to decompile.\n");
        return -1;
    }

    char* elfName = argv[1];

    ParsedElf* parsedElf = readElf(elfName);

    if (!parsedElf){
        return -2;
    }

    printf("Hello, Decomp!\n");

    csh handle;
    cs_insn *insn;
    size_t count;

    if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK)
        return -1;

    cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);

    parseAsmFromInstruction(NULL, 0x401156, parsedElf, &handle);

    // count = cs_disasm(handle, parsedElf->textSection, parsedElf->textSectionSize, parsedElf->textSectionVAddr, 0, &insn);
    //
    // if (count > 0) {
    //     size_t j;
    //     for (j = 0; j < count; j++) {
    //         printf("0x%"PRIx64":\t%s\t\t%s\n", insn[j].address, insn[j].mnemonic,
    //                insn[j].op_str);
    //     }
    //
    //     cs_free(insn, count);
    // } else
    //     printf("ERROR: Failed to disassemble given code!\n");


    cs_close(&handle);

    return 0;
}
