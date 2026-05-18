
#include <cassert>
#include <cstdlib>
#include <limits>

#include "memory.hpp"

pool::Pool pool::create (std::size_t item_size, std::size_t num_items) {
    assert(num_items < std::numeric_limits<std::uint32_t>::max());
    auto buffer_size = item_size * num_items;
    return Pool {
        .buffer = {
            static_cast<std::byte*>(std::aligned_alloc(32, buffer_size * sizeof(std::byte))),
            buffer_size,
        },
        .available_items = std::uint32_t(num_items),
        .item_size = std::uint16_t(item_size)
    };
}

void pool::destroy (Pool& pool) {
    std::free(pool.buffer.data);
    pool.buffer.data = nullptr;
    pool.buffer.length = 0;
    pool.item_size = 0;
}

pool::Buffer pool::alloc (Pool& pool) {
    if (pool.available_items > 0) {
        pool.available_items--;
        return Buffer{
            0,
            pool.item_size
        };
    } else {
        return Buffer{nullptr, 0};
    }
}
