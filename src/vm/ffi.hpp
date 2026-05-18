#pragma once

#include <cstdint>

namespace ffi {
    struct FFI;

    union Vec2 {
        float f[2];
        struct {
          float x, y;
        };
    };

    // Scalars
    float pop(struct FFI* ffi);
    void push(struct FFI* ffi, float value);
    std::int32_t popi(struct FFI* ffi);
    void pushi(struct FFI* ffi, std::int32_t value);

    // Vectors
    Vec2 popv(struct FFI* ffi);
    void pushv(struct FFI* ffi, Vec2 value);
}
