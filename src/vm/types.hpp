#pragma once

#include <cstddef>
#include <cstdint>
#include <smmintrin.h>
#include <immintrin.h>

#define UNUSED __attribute__ ((unused))
#define MUSTTAIL __attribute__((musttail))
#define INLINE __attribute__((always_inline)) inline
#define LIKELY(expr)   __builtin_expect(!!(expr), 1)
#define UNLIKELY(expr)   __builtin_expect(!!(expr), 0)
#define EXPECT_TAKEN(cond) (LIKELY(cond))
#define EXPECT_NOT_TAKEN(cond) (UNLIKELY(cond))

#define AS_STRING(a) #a
#define JOIN_SYMBOLS(a,b) a ## b
#define CONCATENATE(a, b) JOIN_SYMBOLS(a, b)
#define INSTRUCTION_ARGS_DECL const OpCode*, struct Context*, StackEntry*, Slot*, I32, R128, R128, R128, R256
#define INSTRUCTION_ARGS UNUSED const OpCode* instructions, UNUSED struct Context* ctx, UNUSED StackEntry* callstack, UNUSED Slot* slots, UNUSED I32 counter, UNUSED R128 ra, UNUSED R128 rb, UNUSED R128 rc, UNUSED R256 rs
#define STACK_DECLS \
    Stack a{ra}; \
    Stack b{rb}; \
    Stack c{rc}; \
    ShadowStacks shadow{rs};

#define INSTRUCTION_ABC(name, CODE, ...) \
    INSTRUCTION(CONCATENATE(name, _a), {CODE(a __VA_OPT__(,) __VA_ARGS__) NEXT();} ) \
    INSTRUCTION(CONCATENATE(name, _b), {CODE(b __VA_OPT__(,) __VA_ARGS__) NEXT();} ) \
    INSTRUCTION(CONCATENATE(name, _c), {CODE(c __VA_OPT__(,) __VA_ARGS__) NEXT();} )
#define INSTRUCTION_OPERANDS_ABC(name, ...) \
    INSTRUCTION_OPERANDS(CONCATENATE(name, _a), __VA_ARGS__) \
    INSTRUCTION_OPERANDS(CONCATENATE(name, _b), __VA_ARGS__) \
    INSTRUCTION_OPERANDS(CONCATENATE(name, _c), __VA_ARGS__)
#define INSTRUCTION_X(name, code) INSTRUCTION(name, {code} NEXT();)

using R128 = __m128;
using R256 = __m256;
using OpCode = std::uint8_t;
using U8 = std::uint8_t;
using U16 = std::uint16_t;
using I16 = std::int16_t;
using U32 = std::uint32_t;
using I32 = std::int32_t;
using U64 = std::uint64_t;
using Scalar = float;
using Slot = U32;

union StackEntry;

namespace ReturnValue {
    using Type = U8;
    enum {
        Ok=0,
        DivideByZero,
        BufferOutOfBounds,
        USER,
    };
}

struct Result {
    ReturnValue::Type return_code;
    U8 SP;
    INLINE void operator=(const Result& other) volatile {
        return_code = other.return_code;
        SP = other.SP;
    }
};

template <typename T>
struct Array {
    T* const data;
    std::size_t length;
};

enum class OperandType {
    Value, // Scalar Value, 32 bit float
    ByteOffset, // U16
    SlotId, // Slot index, U8
    Function, // Funciton index, U8
    NativeFunction, // Native function index, U16
    StackSelector, // 4x2bit stack indices, U8
    ShadowBlendMask, // 4x4 shadow stack selectors, U16
    IValue8, // Integer value, U8
    IValue16, // Integer value, U16
};
struct OperandInfo {
    OperandType type;
    U16 count;
};
struct InstructionInfo {
    const char* name;
    OpCode opcode;
};

using InstructionFunction = Result(INSTRUCTION_ARGS_DECL);
