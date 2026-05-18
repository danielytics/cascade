#include <array>
#include <immintrin.h>
#include <vector>
#include <cstring>

#include "vm.hpp"
#include "instructions_list.hpp"
#include "instructions_impl.hpp"

extern std::array<InstructionFunction*, Instructions::COUNT> instructions_table;


template <int N, bool Lower> float part(const __m128 s) {
    return std::bit_cast<float>(_mm_extract_epi32(s, N));
}

void stack_output(const char* label, const __m128 reg) {
  output("{}: {} {} {} {}", label, part<0, true>(reg), part<1, true>(reg), part<2, true>(reg), part<3, true>(reg));
}

template <int N, typename T, typename ValueT = T>
void arr_output(const char* label, const Array<T>& arr, bool numbers=false) {
    const T* data = arr.data;
    output("{}:", label);
    int num=0;
    for (auto i=0; i<arr.length; ++i) {
        if (i % N == 0) {
            output("\n");
            if (numbers) {
                output("{}: ", num++);
            }
        }
        T value = *data++;
        if constexpr (std::is_same_v<T, float> || sizeof(T) == sizeof(float)) {
            output("{:08x} ({})  ", value, std::bit_cast<float>(value));
        } else if constexpr (sizeof(T) == sizeof(U8)) {
            output("{:02x} ", U8(value));
        }
    }
    output("\n");
}


void ctx::debug(const OpCode* instructions, const struct Context* ctx, const StackEntry* callstack, I32 counter, R128 ra, R128 rb, R128 rc, R256 rs) {
    auto IP = instructions - ctx->bytecode;
    auto SP = callstack - ctx->vm_state.stackspace;
    output("{{\n");
    output("  [IP: {}, SP: {}, Counter: {}]\n", IP, SP, counter);
    output("  [ ");
    stack_output("a", ra);
    stack_output(" | b", rb);
    stack_output(" | c", rc);
    output("]\n  [ ");
    stack_output("s0",  ShadowStacks::raw<0>(rs));
    stack_output(" | s1", ShadowStacks::raw<1>(rs));
    output(" ]\n}}\n");
}

void ctx::debug(const struct Context* ctx, std::uint8_t SP) {
    StackEntry* callstack = const_cast<StackEntry*>(ctx->vm_state.stackspace);
    I32 counter;
    R128 ra, rb, rc;
    R256 rs;
    callstack += SP;
    const OpCode* instructions = VM_RESTORE_STATE(true);
    ctx::debug(instructions, ctx, callstack, counter, ra, rb, rc, rs);
    // arr_output<8>("Callstack", Array<U32>{(U32*)(ctx->vm_state.stackspace), 256});
}

void ctx::output_slots(const Context* ctx) {
    arr_output<8>("Slots", ctx->slots);
}

#ifdef ENABLE_TRACING
void vm_trace(const char* name, const OpCode* instructions, const struct Context* ctx, const StackEntry* callstack, I32 counter, R128 ra, R128 rb, R128 rc, R256 rs) {
    auto IP = instructions - ctx->bytecode;
    auto SP = callstack - ctx->vm_state.stackspace;
    output("{:05} {:18} ", IP, name);
    stack_output("[ a", ra);
    stack_output(" | b", rb);
    stack_output(" | c", rc);
    stack_output(" | s0",  ShadowStacks::raw<0>(rs));
    stack_output(" | s1", ShadowStacks::raw<1>(rs));
    output(" | counter: {} | sp: {} ]\n", counter, SP);
}
#endif

Context* ctx::create (Environment* env, const OpCode* bytecode, U16 numSlots, const Array<const OpCode* const> functions) {
    auto ctx = new Context{
        .bytecode = bytecode,
        .vm_state = VMState{
            .stackspace = {0}
        },
        .slots = {
            new Slot[numSlots],
            numSlots,
        },
        .functions = functions,
        .native_functions = env->native_functions.data(),
        .env = env,
    };
    std::memset(ctx->slots.data, 0, ctx->slots.length * sizeof(Slot));
    output("Size of stack: {} (entry: {}), size of vm_state: {} ({})\n", sizeof(ctx->vm_state.stackspace), sizeof(StackEntry), sizeof(VMState), sizeof(VMState) - sizeof(ctx->vm_state.stackspace));
    std::memset(ctx->vm_state.stackspace, 0, sizeof(ctx->vm_state.stackspace));
    return ctx;
}

void ctx::destroy (Context* ctx) {
    delete [] ctx->slots.data;
    delete ctx;
}

Result ctx::execute (Context* ctx, std::uint8_t SP) {
    auto instructions = ctx->bytecode;
    StackEntry* callstack = ctx->vm_state.stackspace;
    I32 counter;
    R128 ra, rb, rc;
    R256 rs;
    if (SP == 0) {
        // Start executing from blank slate
        counter = 0;
        ra = _mm_setzero_ps();
        rb = _mm_setzero_ps();
        rc = _mm_setzero_ps();
        rs = _mm256_setzero_ps();
    } else {
        // Continue executing from saved state
        callstack += SP;
        VM_RESTORE_STATE(true);
    }
    auto instruction = instructions_table[*instructions];
    return instruction(
        instructions + 1, ctx,
        ctx->vm_state.stackspace, ctx->slots.data,
        counter, ra, rb, rc, rs
    );
}

void ctx::set_bytecode(Context* ctx, const OpCode* bytecode) {
    ctx->bytecode = bytecode;
}

//////////////////////////////////////////////////
// FFI API Implementation                       //
//////////////////////////////////////////////////

float ffi::pop(ffi::FFI* ffi) {
    const auto [top] = Stack{ffi->a}.pop<1>();
    return top;
}

void ffi::push(FFI* ffi, float value) {
    Stack{ffi->a}.push(value);
}

ffi::Vec2 ffi::popv(FFI* ffi) {
    const auto [x, y] = Stack{ffi->a}.pop<2>();
    return {x, y};
}

void ffi::pushv(FFI* ffi, Vec2 value) {
    Stack{ffi->a}.push(value.y, value.x);
}
//////////////////////////////////////////////////


Environment* env::create () {
    auto env =  new Environment{
        .native_functions = {},
        .native_functions_table = {},
        .bytecode = {

        },
    };
    // Clear memory for easier debugging and safety.
    std::memset(env->bytecode.data, 0, env->bytecode.length);
    return env;
}

void env::destroy (Environment* env) {
    std::free(env->bytecode.data);
    delete env;
}

void env::register_native (Environment* env, const char* name, NativeFunction<void>* func, void* userdata) {
    env->native_functions_table[name] = env->native_functions.size();
    env->native_functions.push_back({
        func, userdata
    });
}

U16 env::get_function (Environment* env, const char* name) {
    auto it = env->native_functions_table.find(name);
    if (it == env->native_functions_table.end()) {
        return U16(-1);
    }
    return it->second;
}

void wrapped_function(ffi::FFI* ffi, void* userdata) {
    reinterpret_cast<void(*)(ffi::FFI*)>(userdata)(ffi);
}
