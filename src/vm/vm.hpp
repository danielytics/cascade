#pragma once

#include <vector>
#include <cstdio>
#include <format>

#include "types.hpp"
#include "ffi.hpp"

template <typename... Args>
void __attribute__((noinline)) output(std::format_string<Args...> format, Args &&...args) {
  std::fputs(std::format(format, std::forward<Args>(args)...).c_str(), stdout);
}

template <typename Userdata> using NativeFunction = void(ffi::FFI*, Userdata*);
void wrapped_function(ffi::FFI* ffi, void* userdata);

struct Environment;
struct Context;

namespace ffi {
    struct FFI;
}

//////////////////////////////////////////////////
// Context API                                  //
//////////////////////////////////////////////////
namespace ctx {
    Context* create (Environment* env, const OpCode* bytecode, U16 numSlots, const Array<const OpCode* const> functions);
    void destroy (Context* ctx);
    Result execute (Context* ctx, std::uint8_t SP=0);

    void debug(const struct Context* ctx, std::uint8_t SP);
    void output_slots(const struct Context* ctx);
    void set_bytecode(Context* ctx, const OpCode* bytecode);
}

//////////////////////////////////////////////////
// Environment API                              //
//////////////////////////////////////////////////
namespace env {
    Environment* create ();
    void destroy (Environment* env);
    void register_native (Environment* env, const char* name, NativeFunction<void>* func, void* userdata);
    U16 get_function (Environment* env, const char* name);


    template <typename Func> std::enable_if_t<std::is_invocable_v<Func, ffi::FFI*>>
    register_function(Environment* env, const char* name, Func func) {
        register_native(env, name, wrapped_function, reinterpret_cast<void*>(func));
    }

    template <typename Userdata, typename Func=void(ffi::FFI*, Userdata*)>
    void register_function(Environment* env, const char* name, Func func, Userdata* userdata) {
        register_native(
            env,
            name,
            reinterpret_cast<NativeFunction<void>*>(func),
            const_cast<void*>(reinterpret_cast<const void*>(userdata))
        );
    }
}

void corelib_install (Environment* env);
