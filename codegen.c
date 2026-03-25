#include "mimicc.h"

_Noreturn void todo() { error("not yet implemented"); }

static const char *getRegName(const Register *r) {
    // clang-format off
    static const char *regTable[RegCount][4] = {
        {"rax", "eax",  "ax",   "al"},
        {"rdi", "edi",  "di",   "dil"},
        {"rsi", "esi",  "si",   "sil"},
        {"rdx", "edx",  "dx",   "dl"},
        {"rcx", "ecx",  "cx",   "cl"},
        {"rbp", "ebp",  "bp",   "bpl"},
        {"rsp", "esp",  "sp",   "spl"},
        {"rbx", "ebx",  "bx",   "bl"},
        {"r8",  "r8d",  "r8w",  "r8b"},
        {"r9",  "r9d",  "r9w",  "r9b"},
        {"r10", "r10d", "r10w", "r10b"},
        {"r11", "r11d", "r11w", "r11b"},
        {"r12", "r12d", "r12w", "r12b"},
        {"r13", "r13d", "r13w", "r13b"},
        {"r14", "r14d", "r14w", "r14b"},
        {"r15", "r15d", "r15w", "r15b"},
    };
    // clang-format on

    int index = 0;
    switch (r->size) {
    case Reg8:
        index = 3;
        break;
    case Reg16:
        index = 2;
        break;
    case Reg32:
        index = 1;
        break;
    case Reg64:
        index = 0;
        break;
    default:
        errorUnreachable();
    }

    return regTable[r->kind][index];
}

static void genCodeOne(const AsmInst *inst) {
    switch (inst->kind) {
    case AsmAnyText:
        dumps(inst->text);
        break;
    case AsmPush:
        dumpf("  push %s\n", getRegName(&inst->reg));
        break;
    case AsmPop:
        dumpf("  pop %s\n", getRegName(&inst->reg));
        break;
    case AsmLabel:
        todo();
        break;
    case AsmMov:
        dumpf("  mov %s, %s\n", getRegName(&inst->data.mov.dst),
                getRegName(&inst->data.mov.src));
        break;
    }
}

void genCode(const AsmInst *inst) {
    for (; inst; inst = inst->next) {
        genCodeOne(inst);
    }
}
