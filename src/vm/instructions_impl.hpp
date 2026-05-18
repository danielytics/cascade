#pragma once
#include <unordered_map>
#include <vector>
#include <type_traits>
#include <utility>

#include "vm.hpp"
#include "ffi.hpp"
#include "types.hpp"

#ifndef OMIT_MACROS
    #define CALL_NEXT_RAW(stack, cnt,a,b,c,s) MUSTTAIL return instructions_table[*instructions](++instructions, ctx, stack, slots, cnt, a, b, c, s);
    #define CALL_NEXT(cnt,a,b,c) CALL_NEXT_RAW(callstack, cnt,a,b,c,rs)
    #define NEXT() CALL_NEXT(counter, ra, rb, rc)
    #define SKIP(num) instructions += num
    #define READ(type, var) type var; std::memcpy(&var, instructions, sizeof(type)); instructions += sizeof(type)
    #define VM_SAVE_STATE(SaveRegA) vm_save_state<SaveRegA>(instructions, ctx, callstack, counter, ra, rb, rc, rs);
    #define VM_RESTORE_STATE(RestoreRegA) vm_restore_state<RestoreRegA>(instructions, callstack, counter, ra, rb, rc, rs);
    #define VM_END(status) return vm_return(instructions, ctx, callstack, slots, counter, ra, rb, rc, rs, status);

    #ifdef ENABLE_TRACING
    #define DO_TRACE(name) { vm_trace(AS_STRING(name), instructions, ctx, callstack, counter, ra, rb, rc, rs); }
    #else
        #define DO_TRACE(name)
    #endif

    #define INSTRUCTION(name, ...) Result InstructionsImpls::name (INSTRUCTION_ARGS) { DO_TRACE(name) STACK_DECLS __VA_ARGS__ }
#endif


union StackEntryRegisters {
    R256 S;
    struct {
        R128 B;
        R128 C;
    };
};
static_assert(sizeof(StackEntryRegisters) == 32, "StackEntryRegisters is 32 + 2x16 bytes");

struct StackEntryCallFrame {
    R128 A;
    U32 counter;
    U32 IP;
    const OpCode* bytecode;
};
static_assert(sizeof(StackEntryCallFrame) == 32, "StackEntryCallFrame is 16 + 2x4 + 8 bytes");

struct StackEntryDataFrame {
    float data[8];
};
static_assert(sizeof(StackEntryDataFrame) == 32, "StackEntryDataFrame is 4x8 bytes");

struct StackEntryLoopFrame {
    const OpCode* loop_start;
    U32 max_count;
    U32 padding; // Not necessary, but we like being explicit
};
static_assert(sizeof(StackEntryLoopFrame) == 16, "StackEntryLoopFrame is 8 + 4 + padding bytes");

union StackEntry {
    StackEntryRegisters registers;
    StackEntryCallFrame call_frame;
    StackEntryDataFrame data_frame;
    StackEntryLoopFrame loop_frame;
};
static_assert(sizeof(StackEntry) % sizeof(R256) == 0, "StackEntry must be multiple of 32 bytes, for 256bit alignment");
static_assert(sizeof(StackEntry) == 32, "StackEntry size is the largest of its constituents");


struct NativeFunctionEntry {
    NativeFunction<void>* ptr;
    void* userdata
;
};

struct Environment {
    std::vector<NativeFunctionEntry> native_functions;
    std::unordered_map<const char*, std::uint16_t> native_functions_table;
    Array<OpCode> bytecode;
};


struct VMState {
    // Each function call is 3 entries, space for depth 10 calls + 2 nested loops
    alignas(R256) StackEntry stackspace[32];
};
static_assert(sizeof(VMState) <= 1024, "VMState should ideally fit into 1KB");
static_assert(sizeof(VMState) == 1024, "For now, its exactly 1KB. This assertion is executable documentation. Change it if necessary.");


struct Context {
    const OpCode* bytecode;
    VMState vm_state;
    Array<Slot> slots;
    const Array<const OpCode* const> functions;
    NativeFunctionEntry* native_functions;
    Environment* env;
};

namespace ffi {
    struct FFI {
        R128& a;
        Context* ctx;
    };
}


namespace ctx {
    extern void debug(const OpCode* instructions, const struct Context* ctx, const StackEntry* callstack, I32 counter, R128 ra, R128 rb, R128 rc, R256 rs);
}

#ifdef ENABLE_TRACING
void vm_trace(const char* name, const OpCode* instructions, const struct Context* ctx, const StackEntry* callstack, I32 counter, R128 ra, R128 rb, R128 rc, R256 rs);
#endif

template <bool RegA> INLINE void vm_save_state  (const OpCode* instructions, struct Context* ctx, StackEntry*& callstack, I32 counter, R128 ra, R128 rb, R128 rc, R256 rs) {
    StackEntryRegisters& shadow = callstack->registers;
    shadow.S = rs;
    ++callstack;
    StackEntryRegisters& reg = callstack->registers;
    reg.B = rb;
    reg.C = rc;
    ++callstack;
    StackEntryCallFrame& frame = callstack->call_frame;
    if constexpr (RegA) {
        frame.A = ra;
    }
    frame.counter = counter;
    frame.bytecode = ctx->bytecode;
    frame.IP = instructions - ctx->bytecode;
    ++callstack;
}

template <bool RegA> INLINE const OpCode* vm_restore_state (const OpCode*& instructions, StackEntry*& callstack, I32& counter, R128& ra, R128& rb, R128& rc, R256& rs) {
    StackEntryCallFrame& frame = (--callstack)->call_frame;
    if constexpr (RegA) {
        ra = frame.A;
    }
    counter = frame.counter;
    instructions = frame.bytecode + frame.IP;
    StackEntryRegisters& reg = (--callstack)->registers;
    rc = reg.C;
    rb = reg.B;
    StackEntryRegisters& shadow = (--callstack)->registers;
    rs = shadow.S;
    return frame.bytecode;
}

INLINE Result vm_return (INSTRUCTION_ARGS, ReturnValue::Type status) {
    VM_SAVE_STATE(true);
    return {
        status,
        U8(callstack - ctx->vm_state.stackspace)
    };
}

class Stack {
public:
    Stack(__m128& reg) : reg{reg} {}

    INLINE void operator=(Stack& other) {
        reg = other.reg;
    }

    template <int N>
    INLINE float get() {
        static_assert(N >= 0 && N < 4, "N must be between 0 and 3 inclusive.");
        if constexpr (N == 0) {
            return _mm_cvtss_f32(reg);
        } else {
            return _mm_cvtss_f32(_mm_shuffle_ps(reg, reg, _MM_SHUFFLE(N, N, N, N)));
        }
    }

    template <int N>
    INLINE Stack set(float value) {
        if constexpr (N == 0) {
            // Use higher throughput instruction
            reg = _mm_move_ss(reg, _mm_set_ss(value));
        } else {
            reg = _mm_insert_ps(reg, _mm_set_ss(value), 0x10 * N);
        }
        return *this;
    }

    template <typename... Args>
    INLINE Stack push(Args... values) {
        static_assert(sizeof...(values) >= 1 && sizeof...(values) <= 4,
                      "push requires between 1 and 4 values.");
        static_assert((std::is_convertible_v<Args, float> && ...),
                      "All arguments to push must be convertible to float.");

        reg = _mm_castsi128_ps(_mm_slli_si128(_mm_castps_si128(reg), 4 * sizeof...(values)));
        push_impl(std::index_sequence_for<Args...>{}, static_cast<float>(values)...);
        return *this;
    }

    template <typename... Args>
    INLINE Stack push(const std::tuple<Args...>& values) {
        constexpr std::size_t N = sizeof...(Args);
        static_assert(N >= 1 && N <= 4, "push requires between 1 and 4 values.");

        reg = _mm_castsi128_ps(_mm_slli_si128(_mm_castps_si128(reg), 4 * N));
        push_tuple_impl(values, std::make_index_sequence<N>{});
        return *this;
    }

    template <int N> INLINE Stack drop() {
        reg = _mm_castsi128_ps(_mm_srli_si128(_mm_castps_si128(reg), 4 * N));
        return *this;
    }

    template <int N>
    INLINE auto pop() {
        static_assert(N >= 1 && N <= 4, "N must be between 1 and 4.");

        auto values = extract_top(std::make_index_sequence<N>{});
        reg = _mm_castsi128_ps(_mm_srli_si128(_mm_castps_si128(reg), 4 * N));
        return values;
    }

    INLINE float pull () {
        R128 temp = _mm_shuffle_ps(reg, reg, _MM_SHUFFLE(2, 1, 0, 3));
        float result = _mm_cvtss_f32(temp);
        reg = _mm_move_ss(temp, _mm_setzero_ps());
        return result;
    }

    template <int Top, int Middle1, int Middle2, int Bottom>
    INLINE Stack shuffle() {
        reg = _mm_shuffle_ps(reg, reg, _MM_SHUFFLE(Bottom, Middle2, Middle1, Top));
        return *this;
    }

    template <typename Func>
    INLINE Stack exec (Func func) {
        __m128& ref = reg;
        static_assert(std::is_invocable_r_v<void, Func, __m128&>, "Func must be void(__m128&)");
        func(ref);
        return *this;
    }

    INLINE std::byte* write(std::byte* destination) {
        _mm_store_ps(reinterpret_cast<float*>(destination), reg);
        return destination + sizeof(reg);
    }

    INLINE const std::byte* read(const std::byte* source) {
        reg = _mm_load_ps(reinterpret_cast<const float*>(source));
        return source + sizeof(reg);
    }

private:
    __m128& reg;

    template <std::size_t... Indices, typename... Args>
    INLINE void push_impl(std::index_sequence<Indices...>, Args... values) {
        (set<sizeof...(values) - 1 - Indices>(values), ...);
    }

    template <typename Tuple, std::size_t... Indices>
    INLINE void push_tuple_impl(const Tuple& values, std::index_sequence<Indices...>) {
        (set<std::tuple_size<Tuple>::value - 1 - Indices>(static_cast<float>(std::get<Indices>(values))), ...);
    }

    template <std::size_t... Is>
    INLINE auto extract_top(std::index_sequence<Is...>) {
        return std::make_tuple(get<Is>()...);
    }

    friend class ShadowStacks;
};

class ShadowStacks {
public:
    ShadowStacks(__m256& reg) : reg{reg} {}

    INLINE void operator=(ShadowStacks& other) {
        reg = other.reg;
    }

    template <int N>
    INLINE ShadowStacks store (const Stack& stack) {
        static_assert(N >= 0 && N <= 1, "N must be 0 or 1.");
        set<N>(stack.reg);
        return *this;
    }

    template <int N>
    INLINE ShadowStacks load (Stack& stack) {
        static_assert(N >= 0 && N <= 1, "N must be 0 or 1.");
        stack.reg = get<N>();
        return *this;
    }

    template <int N>
    INLINE const __m128 get () const {
        static_assert(N >= 0 && N <= 1, "N must be 0 or 1.");
        return raw<N>(reg);
    }

    template <int N>
    INLINE void set (__m128 value) {
        static_assert(N >= 0 && N <= 1, "N must be 0 or 1.");
        if constexpr (N == 0) {
            reg = _mm256_insertf128_ps(reg, value, 0);
        } else {
            reg = _mm256_insertf128_ps(reg, value, 1);
        }
    }

    template <int N>
    INLINE ShadowStacks swap_with (Stack& stack) {
        static_assert(N >= 0 && N <= 1, "N must be 0 or 1.");
        __m128 temp = stack.reg;
        stack.reg = get<N>();
        set<N>(temp);
        return *this;
    }

    INLINE ShadowStacks swap  () {
        reg = _mm256_permutex_pd(reg, 0x4e);
        return *this;
    }

    template <int N>
    INLINE ShadowStacks push (float value) {
        static_assert(N >= 0 && N <= 1, "N must be 0 or 1.");
        __m128 temp = get<N>();
        temp = _mm_castsi128_ps(_mm_slli_si128(_mm_castps_si128(temp), 4));
        temp = _mm_move_ss(temp, _mm_set_ss(value));
        set<N>(temp);
        return *this;
    }

    template <int N>
    INLINE float pop () {
        static_assert(N >= 0 && N <= 1, "N must be 0 or 1.");
        __m128 temp = get<N>();
        float value = _mm_cvtss_f32(temp);
        temp = _mm_castsi128_ps(_mm_srli_si128(_mm_castps_si128(temp), 4));
        set<N>(temp);
        return value;
    }

    template <int N>
    INLINE float pull () {
        static_assert(N >= 0 && N <= 1, "N must be 0 or 1.");
        __m128 temp = get<N>();
        temp = _mm_shuffle_ps(temp, temp, _MM_SHUFFLE(2, 1, 0, 3));
        float result = _mm_cvtss_f32(temp);
        temp = _mm_move_ss(temp, _mm_setzero_ps());
        set<N>(temp);
        return result;
    }

    INLINE std::byte* write(std::byte* destination) {
        _mm256_store_ps(reinterpret_cast<float*>(destination), reg);
        return destination + sizeof(reg);
    }

    INLINE const std::byte* read(const std::byte* source) {
        reg = _mm256_load_ps(reinterpret_cast<const float*>(source));
        return source + sizeof(reg);
    }

    template <int N>
    static INLINE const __m128 raw (const __m256& reg) {
        static_assert(N >= 0 && N <= 1, "N must be 0 or 1.");
        if constexpr (N == 0) {
            return _mm256_castps256_ps128(reg);
        } else {
            return _mm256_extractf128_ps(reg, 1);
        }
    }

private:
    __m256& reg;
};
