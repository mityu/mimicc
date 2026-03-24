#include "mimicc.h"

_Noreturn void todo() { error("not yet implemented"); }

static void genCodeOne(const AsmInst *inst) {
    switch (inst->kind) {
    case AsmAnyText:
        dumps(inst->text);
        break;
    case AsmPush:
        todo();
        break;
    case AsmPop:
        todo();
        break;
    case AsmLabel:
        todo();
        break;
    }
}

void genCode(const AsmInst *inst) {
    for (; inst; inst = inst->next) {
        genCodeOne(inst);
    }
}
