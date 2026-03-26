#include "mimicc.h"

_Noreturn void todo() { error("not yet implemented"); }

// Get string representation of given register.  Every returned string is
// stored in static area, so don't modify or free the returned ones.
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
    case OpSize8:
        index = 3;
        break;
    case OpSize16:
        index = 2;
        break;
    case OpSize32:
        index = 1;
        break;
    case OpSize64:
        index = 0;
        break;
    default:
        errorUnreachable();
    }

    return regTable[r->kind][index];
}

// Return a string representation of given operand.  The returned string is
// always on newly allocated memory, therefore you can/should free it after
// using it.
static char *stringifyOperand(const AsmInstOperand *operand) {
    switch (operand->mode) {
    case AsmAddressingModeRegister:
        return format("%s", getRegName(&operand->src.reg));
    case AsmAddressingModeImm:
        if (operand->src.imm.isLabel) {
            return format(".%s", operand->src.imm.label);
        } else {
            return format("%d", operand->src.imm.value);
        }
    case AsmAddressingModeMemory: {
        char *base = NULL, *offset = NULL, *retval = NULL;
        const AsmInstOperandMem *mem = &operand->src.mem;
        AsmInstOperand opoffset;

        opoffset.mode = AsmAddressingModeImm;
        opoffset.src.imm = mem->offset;
        offset = stringifyOperand(&opoffset);

        if (mem->isRelative) {
            AsmInstOperand opbase;
            char *size;

            opbase.mode = AsmAddressingModeRegister;
            opbase.src.reg = mem->base;
            base = stringifyOperand(&opbase);

            size = NULL;
            switch (mem->size) {
            case OpSize8:
                size = "BYTE";
                break;
            case OpSize16:
                size = "WORD";
                break;
            case OpSize32:
                size = "DWORD";
                break;
            case OpSize64:
                size = "QWORD";
                break;
            }
            if (size == NULL)
                errorUnreachable();

            retval = format("%s PTR %s[%s]", size, offset, base);
        } else {
            retval = format("%s", offset);
        }

        safeFree(base);
        safeFree(offset);
        return retval;
    }
    }
    errorUnreachable();
}

static void genCodeOne(const AsmInst *inst) {
    switch (inst->kind) {
    case AsmAnyText:
        dumps(inst->text);
        break;
    case AsmPush: {
        char *op = stringifyOperand(&inst->data.push);
        dumpf("  push %s\n", op);
        safeFree(op);
        break;
    }
    case AsmPop:
        dumpf("  pop %s\n", getRegName(&inst->data.pop));
        break;
    case AsmLabel:
        todo();
        break;
    case AsmMov: {
        char *src, *dst;
        src = stringifyOperand(&inst->data.mov.src);
        dst = stringifyOperand(&inst->data.mov.dst);
        dumpf("  mov %s, %s\n", dst, src);
        safeFree(src);
        safeFree(dst);
        break;
    }
    }
}

void genCode(const AsmInst *inst) {
    for (; inst; inst = inst->next) {
        genCodeOne(inst);
    }
}
