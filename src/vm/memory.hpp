#pragma once

#include <cstdint>
#include <cstddef>

namespace pool {
    struct Buffer {
        std::byte* data;
        std::size_t length;
    };

    struct Pool {
        Buffer buffer;
        std::uint32_t available_items;
        std::uint16_t item_size;
    };

    Pool create (std::size_t item_size, std::size_t num_items);
    void destroy (Pool& pool);
    Buffer alloc (Pool& pool);
}
