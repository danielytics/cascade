#ifdef INSTRUCTION_FUNCPTRS
#undef INSTRUCTION_FUNCPTRS
#define INSTRUCTION(name, ...)  InstructionsImpls::name,
#define INSTRUCTION_OPERANDS(...)
#endif

#ifdef INSTRUCTION_ENUMS
#undef INSTRUCTION_ENUMS
#define INSTRUCTION(name, ...) name,
#define INSTRUCTION_OPERANDS(...)
#endif

#ifdef INSTRUCTION_DECLS
#undef INSTRUCTION_DECLS
#define INSTRUCTION(name, ...) Result name (INSTRUCTION_ARGS);
#define INSTRUCTION_OPERANDS(...)
#endif

#ifdef INSTRUCTION_NAMES
#undef INSTRUCTION_NAMES
#define INSTRUCTION(iname, ...) InstructionInfo{.name=AS_STRING(iname), .opcode=OpCode(Instructions::iname)},
#define INSTRUCTION_OPERANDS(...)
#endif

#ifdef INSTRUCTION_OPERAND_INFO
#undef INSTRUCTION_OPERAND_INFO
#define INSTRUCTION(...)
#define INSTRUCTION_OPERANDS(name, type, count) {Instructions::name, {OperandType::type, count}},
#endif

#ifdef INSTRUCTION_IMPLS
#include "../types.hpp"
#define INSTRUCTION_OPERANDS(...)
#endif

#ifndef INSTRUCTION
#include <cmath>
#define OMIT_MACROS
#include "../instructions_impl.hpp"
#undef OMIT_MACROS
#define INSTRUCTION(name, ...) namespace dummy { inline const Result name (INSTRUCTION_ARGS) { STACK_DECLS __VA_ARGS__ } }
#define INSTRUCTION_OPERANDS(...)
#define NEXT() return {ReturnValue::Ok, 0}
#define CALL_CALL_NEXT_RAW(cs,cnt,a,b,c,s) NEXT()
#define CALL_NEXT(cnt,a,b,c) NEXT()
#define SKIP(num)
#define READ(type, var) type var
#define VM_SAVE_STATE(SaveRegA)
#define VM_RESTORE_STATE(RestoreRegA)
#define VM_END(status) return {status, 0};
#endif
