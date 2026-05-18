
#include <vector>
#include <cstdio>
#include <format>
#include <cassert>

#define UNUSED __attribute__ ((unused))
#define MUSTTAIL __attribute__((musttail))
#define INLINE __attribute__((always_inline)) inline
#define LIKELY(expr)   __builtin_expect(!!(expr), 1)
#define UNLIKELY(expr)   __builtin_expect(!!(expr), 0)
#define EXPECT_TAKEN(cond) (LIKELY(cond))
#define EXPECT_NOT_TAKEN(cond) (UNLIKELY(cond))
#define AS_STRING(a) #a
#define JOIN_SYMBOLS(a,b) a ## b
#define CONCAT(a, b) JOIN_SYMBOLS(a, b)

template <typename... Args>
void __attribute__((noinline)) output(std::format_string<Args...> format, Args &&...args) {
  std::fputs(std::format(format, std::forward<Args>(args)...).c_str(), stdout);
}

#include <cstdint>
#include <cstdlib>

using IP = std::byte*;

// INLINE void next(Context& ctx) {
//     ctx.ip++;
//     goto
// }

using Instruction = void(*)(IP, float, float);
extern Instruction ops[];

#define call(a, b) reinterpret_cast<Instruction>(reinterpret_cast<std::uintptr_t>(ops[0]) + *reinterpret_cast<std::uint16_t*>(ip))(ip + 2, a, b)
#define next_args(a, b) MUSTTAIL return call(a, b)
#define next() next_args(a, b)

#define INST(name, data, code) \
    struct CONCAT(base_, name) \
    data; \
    struct CONCAT(i_, name) : CONCAT(base_, name) \
    { static constexpr std::uint8_t OpCode = __COUNTER__; }; \
    void CONCAT(f_, name) (IP ip, float a, float b) \
    code

INST(nop, {}, {
    next();
})

INST(add, {}, {
    a += b;
    next();
})

INST(addi, {
    float im;
}, {
    a += reinterpret_cast<i_addi*>(ip)->im;
    ip += sizeof(i_addi);
    next();
})

INST(inc, {}, {
    a++;
    next();
})

INST(swap, {}, {
    next_args(b, a);
})

INST(put, {}, {
    output("{}\n", a);
    next();
})

INST(end, {}, {
    return;
})


Instruction ops[] = {
    f_nop, f_add, f_addi, f_inc, f_swap, f_put, f_end
};

template <typename T>
void push(std::vector<std::byte>& instruction_stream, const T& instruction) {
    std::uintptr_t base = reinterpret_cast<std::uintptr_t>(ops[0]);
    std::uintptr_t current = reinterpret_cast<std::uintptr_t>(ops[T::OpCode]);
    std::uint16_t offset = static_cast<std::uint16_t>(current - base);

    instruction_stream.push_back(std::byte(offset & 0xFF));
    instruction_stream.push_back(std::byte((offset >> 8) & 0xFF));

    if constexpr (! std::is_empty_v<T>) {
        const std::byte* data = reinterpret_cast<const std::byte*>(&instruction);
        instruction_stream.insert(instruction_stream.end(), data, data + sizeof(T));
    }
}

template <typename... Args>
void pack(std::vector<std::byte>& instruction_stream, const Args&... args) {
    (push(instruction_stream, args), ...);
}

int main (int argc, char* argv[]) {

    std::vector<std::byte> instruction_stream;
    pack(instruction_stream,
        i_inc{},
        i_inc{},
        i_inc{},
        i_swap{},
        i_addi{ 4 },
        i_add{},
        i_put{},
        i_end{}
    );
    for (const auto item : instruction_stream) {
        output("{:x} ", std::uint8_t(item));
    }
    output("\n");
    IP ip = instruction_stream.data();
    call(0, 0);

    return 0;
}
