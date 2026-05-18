#include "vm/vm.hpp"
#include "vm/instructions_list.hpp"

constexpr std::array<uint8_t, 4> float_to_bytes(float f) {
    return std::bit_cast<std::array<uint8_t, 4>>(f);
}

#define CONST(f) float_to_bytes(f)[0], float_to_bytes(f)[1], float_to_bytes(f)[2], float_to_bytes(f)[3]
#define CONST16(u16) U8(u16 & 0xff), U8((u16>>8) & 0xff)
#define FUNC(f) U8(f & 0xff), U8((f >> 8) & 0xff)

void execute(Environment* env, const std::vector<OpCode>& code, Array<U8> constants) {
    Context* ctx = ctx::create(env, code.data(), 32, {nullptr, 0});
    auto result = ctx::execute(ctx);
    ctx::debug(ctx, result.SP);
    ctx::output_slots(ctx);
    ctx::destroy(ctx);
    if (result.return_code == ReturnValue::Ok) {
        output("Ok.\n");
    } else {
        output("Error {:x}.\n", (int)result.return_code);
    }
}


#include <chrono>
struct TestCase {
    const char* label;
    const std::vector<OpCode>& code;
};
void benchmark(std::size_t count, Environment* env, const std::vector<OpCode>& setup, const std::vector<TestCase>& versions, Array<U8> constants) {
    using namespace std::chrono;
    volatile Result result;

    output("{} iterations.\n", count);
    for (const auto& version : versions) {
        Context* ctx = ctx::create(env, setup.data(), 32, {nullptr, 0});
        output("Version {}:\n", version.label);
        result = ctx::execute(ctx);
        ctx::debug(ctx, result.SP);
        ctx::set_bytecode(ctx, version.code.data());

        auto start = high_resolution_clock::now();
        for (std::size_t idx=0; idx<count;++idx) {
            result = ctx::execute(ctx);
        }
        auto end = high_resolution_clock::now();
        auto duration_ns = duration_cast<nanoseconds>(end - start).count();
        auto duration_us = duration_cast<microseconds>(end - start).count();
        auto duration_ms = duration_cast<milliseconds>(end - start).count();
        output("Duration: {} nanos, {} micros, {} millis\n\n", duration_ns, duration_us, duration_ms);
        ctx::destroy(ctx);
    }
}

int main(int argc, char *argv[]) {
    output("Total number of instructions: {} ({} remaining)\n", U16(Instructions::COUNT), U16(256 - Instructions::COUNT));
    auto env = env::create();
    corelib_install(env);

    auto from_minutes = env::get_function(env, "Math.from_minutes");
    auto txt_create = env::get_function(env, "Text.create");
    auto txt_putch = env::get_function(env, "Text.putch");
    auto txt_print = env::get_function(env, "Text.print");

    std::vector<U8> constants{
        'H', 'e', 'l', 'l', 'o', ',', ' ', 'W', 'o', 'r', 'l', 'd', '!',
    };

    using namespace Instructions;

#if 0
for (int i=0; i<1;i++){
    benchmark(1000,
        env, {
            push2_a, CONST(8), CONST(7),
            push2_a, CONST(6), CONST(5),
            s0_store_a,
            push2_a, CONST(5), CONST(3),
            push2_a, CONST(2), CONST(1),
            end,
    },
    {{
        "old swap repeat",
        {
            set_mark,
                s0_swap_a,
                s0_swap_b,
                s1_swap_b,
                s1_swap_a,
            repeat, CONST16(10000),
            end,
        }
    }, {
        "new swap repeat",
        {
            set_mark,
                shadow_swap_s0, 0,
                shadow_swap_s0, 1,
                shadow_swap_s1, 1,
                shadow_swap_s1, 0,
            repeat, CONST16(10000),
            end,
        }
    }}, {constants.data(), constants.size()});
}
#endif

#if 1
    execute(env, {
        // push2_a, CONST(8), CONST(7),
        // push2_a, CONST(6), CONST(5),
        // shadow_store_a,
        // push2_a, CONST(5), CONST(3),
        // push2_a, CONST(2), CONST(1),
        // debug,
        native, FUNC(txt_create),
        set_loop, 10, 0,
            dup_a, // Make copy of text handle
            push_a, CONST('x'),
            // push_counter_a, store_ptr_a,
            native, FUNC(txt_putch), // Putch character into text
        end_loop,
        native, FUNC(txt_print), // Print result
        // push2_a, CONST(4), CONST(3),
        // push2_a, CONST(2), CONST(1),
        // shadow_store_a, debug,
        // shadow_copy_s0_s1, debug,
        // push2_b, CONST(5), CONST(6), debug,
        // shadow_swap_b, debug,
        // push_c, CONST(7), shadow_push_c, debug,
        // shadow_swap,
        // debug,
        // add_a,
        // shadow_push_a,
        // debug,
        // shadow_push_s0_s1,
        // debug,
        // shadow_pop_a,
        end,
    }, {constants.data(), constants.size()});
#endif

#if 0
    execute(env, {
        push_a, CONST(0),
        end,
    }, {constants.data(), constants.size()});
#endif

    env::destroy(env);
    return 0;
}
