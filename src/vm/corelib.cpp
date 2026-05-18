#include "vm.hpp"
#include "ffi.hpp"

#include <cmath>

#include <cstdio>
#include <vector>
#include <string>

//////////////////////////////////////////////////
// Math API                                     //
//////////////////////////////////////////////////
namespace Math {
    void from_degrees (ffi::FFI* ffi) {
        ffi::push(ffi, ffi::pop(ffi) * 0.017453292519943295f);
    }

    void to_degrees (ffi::FFI* ffi) {
        ffi::push(ffi, ffi::pop(ffi) * 57.29577951308232f);
    }

    void from_minutes (ffi::FFI* ffi) {
        ffi::push(ffi, ffi::pop(ffi) * 0.10471975511965978f);
    }

    // Trgonometric Functions

    void sin (ffi::FFI* ffi) {
        ffi::push(ffi, sinf(ffi::pop(ffi)));
    }

    void cos (ffi::FFI* ffi) {
        ffi::push(ffi, cosf(ffi::pop(ffi)));
    }

    void tan (ffi::FFI* ffi) {
        ffi::push(ffi, tanf(ffi::pop(ffi)));
    }

    void asin (ffi::FFI* ffi) {
        ffi::push(ffi, asinf(ffi::pop(ffi)));
    }

    void acos (ffi::FFI* ffi) {
        ffi::push(ffi, acosf(ffi::pop(ffi)));
    }

    void atan (ffi::FFI* ffi) {
        ffi::push(ffi, atanf(ffi::pop(ffi)));
    }

    void atan2 (ffi::FFI* ffi) {
        ffi::push(ffi, atan2f(ffi::pop(ffi), ffi::pop(ffi)));
    }

    void sinh (ffi::FFI* ffi) {
        ffi::push(ffi, sinhf(ffi::pop(ffi)));
    }

    void cosh (ffi::FFI* ffi) {
        ffi::push(ffi, coshf(ffi::pop(ffi)));
    }

    void tanh (ffi::FFI* ffi) {
        ffi::push(ffi, tanhf(ffi::pop(ffi)));
    }

    void asinh (ffi::FFI* ffi) {
        ffi::push(ffi, asinhf(ffi::pop(ffi)));
    }

    void acosh (ffi::FFI* ffi) {
        ffi::push(ffi, acoshf(ffi::pop(ffi)));
    }

    void atanh (ffi::FFI* ffi) {
        ffi::push(ffi, atanhf(ffi::pop(ffi)));
    }

    void hypot (ffi::FFI* ffi) {
        ffi::push(ffi, hypotf(ffi::pop(ffi), ffi::pop(ffi)));
    }


    // Exponential and Logarithmic Functions

    void log (ffi::FFI* ffi) {
        ffi::push(ffi, logf(ffi::pop(ffi)));
    }

    void log10 (ffi::FFI* ffi) {
        ffi::push(ffi, log10f(ffi::pop(ffi)));
    }

    void log2 (ffi::FFI* ffi) {
        ffi::push(ffi, log2f(ffi::pop(ffi)));
    }

    void log1p (ffi::FFI* ffi) {
        ffi::push(ffi, log1pf(ffi::pop(ffi)));
    }

    void exp (ffi::FFI* ffi) {
        ffi::push(ffi, expf(ffi::pop(ffi)));
    }

    void exp2 (ffi::FFI* ffi) {
        ffi::push(ffi, exp2f(ffi::pop(ffi)));
    }

    void expm1 (ffi::FFI* ffi) {
        ffi::push(ffi, expm1f(ffi::pop(ffi)));
    }

}

//////////////////////////////////////////////////
// Text API                                     //
//////////////////////////////////////////////////
namespace Text {
    std::vector<std::string> strings;

    void create (ffi::FFI* ffi) {
        auto id = U32(strings.size());
        strings.push_back("");
        ffi::push(ffi, std::bit_cast<float>(id));
    }

    void concat (ffi::FFI* ffi) {
        auto b = ffi::pop(ffi);
        auto a = ffi::pop(ffi);
        strings[a] += strings[b];
        ffi::push(ffi, a);
    }

    void putch (ffi::FFI* ffi) {
        auto b = ffi::pop(ffi);
        auto a = ffi::pop(ffi);
        strings[a] += char(b);
        ffi::push(ffi, a);
    }

    void print (ffi::FFI* ffi) {
        auto a = ffi::pop(ffi);
        std::puts(strings[a].c_str());
    }
}

//////////////////////////////////////////////////
// Random Number API                            //
//////////////////////////////////////////////////
namespace Random {
    void rand (ffi::FFI* ) {

    }

    void randRange (ffi::FFI*) {

    }

    void randInt (ffi::FFI*) {

    }

    void perlin1d (ffi::FFI*) {

    }

    void perlin2d (ffi::FFI*) {

    }

    void perlin3d (ffi::FFI*) {

    }

    void simplex1d (ffi::FFI*) {

    }

    void simplex2d (ffi::FFI*) {

    }

    void simplex3d (ffi::FFI*) {

    }
}


//////////////////////////////////////////////////
// Install into Environment                     //
//////////////////////////////////////////////////
void corelib_install (Environment* env) {
    // Math library
    env::register_function(env, "Math.from_degrees", Math::from_degrees);
    env::register_function(env, "Math.to_degrees", Math::to_degrees);
    env::register_function(env, "Math.from_minutes", Math::from_minutes);
    env::register_function(env, "Math.log", Math::log);
    env::register_function(env, "Math.log10", Math::log10);
    env::register_function(env, "Math.exp", Math::exp);
    env::register_function(env, "Math.sin", Math::sin);
    env::register_function(env, "Math.cos", Math::cos);
    env::register_function(env, "Math.tan", Math::tan);

    // Text library
    env::register_function(env, "Text.create", Text::create);
    env::register_function(env, "Text.concat", Text::concat);
    env::register_function(env, "Text.putch", Text::putch);
    env::register_function(env, "Text.print", Text::print);
}
