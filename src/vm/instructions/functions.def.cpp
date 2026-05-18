#include "common.hpp"

INSTRUCTION(jump, {
    READ(U8, func_offset);
    ctx->bytecode = ctx->functions.data[func_offset];
    instructions = ctx->bytecode;
    NEXT();
})
INSTRUCTION_OPERANDS(jump, Function, 1)

INSTRUCTION(call, {
    READ(U8, func_offset);
    VM_SAVE_STATE(false);
    ctx->bytecode = ctx->functions.data[func_offset];
    instructions = ctx->bytecode;
    NEXT();
})
INSTRUCTION_OPERANDS(call, Function, 1)

INSTRUCTION(ret, {
    ctx->bytecode = VM_RESTORE_STATE(false);
    NEXT();
})

INSTRUCTION(native, {
    READ(U16, func_index); // Native function index (assembler should have looked it up by name)
    auto entry = ctx->native_functions[func_index];
    ffi::FFI ffi(ra, ctx);
    entry.ptr(&ffi, entry.userdata);
    NEXT();
})
INSTRUCTION_OPERANDS(native, NativeFunction, 1)
