#include "common.hpp"

INSTRUCTION(end, VM_END(ReturnValue::Ok);)

INSTRUCTION(error, {
    READ(U8, error_code);
    VM_END(error_code);
})
INSTRUCTION_OPERANDS(error, IValue8, 1)

INSTRUCTION(debug, {
    ctx::debug(instructions, ctx, callstack, counter, ra, rb, rc, rs);
    NEXT();
})
