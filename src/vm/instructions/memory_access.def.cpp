#include "common.hpp"

// TODO: Replace all instructions in this file with a redesigned slot system.
// Slots should be mappable to buffers:
//  A. Stream buffers that support sequential access, skip and end operations
//  B. Block buffers that support bounds checked random access
// Slots can be read only, write only, or read-write.

////////// New:

// Stream API

INSTRUCTION(stream_load_a, {
    READ(U8, slot);
    NEXT();
})
INSTRUCTION_OPERANDS(stream_load_a, SlotId, 1)

INSTRUCTION(stream_store_a, {
    READ(U8, slot);
    NEXT();
})
INSTRUCTION_OPERANDS(stream_store_a, SlotId, 1)

INSTRUCTION(stream_skip_a, {
    READ(U8, slot);
    NEXT();
})
INSTRUCTION_OPERANDS(stream_skip_a, SlotId, 1)

INSTRUCTION(stream_end_a, {
    READ(U8, slot);
    NEXT();
})
INSTRUCTION_OPERANDS(stream_end_a, SlotId, 1)

// Block API

INSTRUCTION(buffer_size_a, {
    READ(U8, slot);
    /*
    auto index = stack_top(ra);
    auto& slot_entry = slots[slot];
    stack_set(ra, slot_entry.size);
    */
    NEXT();
})
INSTRUCTION_OPERANDS(buffer_size_a, SlotId, 1)

INSTRUCTION(buffer_load_a, {
    READ(U8, slot);
    /*
    auto index = stack_top(ra);
    auto& slot_entry = slots[slot];
    if (index < 0 || index >= slot_entry.size) {
        VM_END_ERROR(Errors::BufferOutOfBounds);
    }
    stack_set(ra, std::bit_cast<float>(slot_entry.buffer[U32(index)]));
    */
    NEXT();
})
INSTRUCTION_OPERANDS(buffer_load_a, SlotId, 1)

INSTRUCTION(buffer_store_a, {
    READ(U8, slot);
    /*
    auto index = stack_top(ra);
    auto& slot_entry = slots[slot];
    stack_pop(ra);
    if (index < 0 || index >= slot_entry.size) {
        VM_END_ERROR(Errors::BufferOutOfBounds);
    }
    slot_entry.buffer[U32(index)] = std::bit_cast<U32>(stack_top(ra));
    stack_pop(ra);
    */
    NEXT();
})
INSTRUCTION_OPERANDS(buffer_store_a, SlotId, 1)

// Streaming read
INSTRUCTION(buffer_streamr_a, {
    READ(U8, slot);
    /*
    auto& slot_entry = slots[slot];
    if (counter < 0 || counter >= slot_entry.size) {
        VM_END_ERROR(Errors::BufferOutOfBounds);
    }
    stack_set(ra, std::bit_cast<float>(slot_entry.buffer[counter]));
    */
    NEXT();
})
INSTRUCTION_OPERANDS(buffer_streamr_a, SlotId, 1)

// Streaming write
INSTRUCTION(buffer_streamw_a, {
    READ(U8, slot);
    /*
    auto& slot_entry = slots[slot];
    if (counter < 0 || counter >= slot_entry.size) {
        VM_END_ERROR(Errors::BufferOutOfBounds);
    }
    slot_entry.buffer[counter] = std::bit_cast<U32>(stack_top(ra));
    stack_pop(ra);
    */
    NEXT();
})
INSTRUCTION_OPERANDS(buffer_streamw_a, SlotId, 1)

////////// Old:

// INSTRUCTION(load_a, {
//     READ(U8, slot);
//     stack_push(ra, std::bit_cast<float>(slots[slot]));
//     NEXT();
// })

// INSTRUCTION(load_ptr_a, {
//     U8 slot = stack_top(ra);
//     stack_set(ra, std::bit_cast<float>(slots[slot]));
//     NEXT();
// })

// INSTRUCTION(store_a, {
//     READ(U8, slot);
//     slots[slot] = std::bit_cast<U32>(stack_top(ra));
//     stack_pop(ra);
//     NEXT();
// })

INSTRUCTION(store_ptr_a, {
    const auto [slot, ptr] = a.pop<2>();
    slots[U8(slot)] = std::bit_cast<U32>(ptr);
    NEXT();
})


// //////////////////////////////////////////////////
// // push_const_X <index>
// //////////////////////////////////////////////////
// INSTRUCTION(push_const_a, {
//     READ(U16, index);
//     stack_push(ra, ctx->constants.data[index]);
//     NEXT();
// })

// INSTRUCTION(push_const_b, {
//     READ(U16, index);
//     stack_push(rb, ctx->constants.data[index]);
//     NEXT();
// })

// INSTRUCTION(push_const_c, {
//     READ(U16, index);
//     stack_push(rc, ctx->constants.data[index]);
//     NEXT();
// })

// //////////////////////////////////////////////////
// // load_const_X
// //////////////////////////////////////////////////
// INSTRUCTION(load_const_a, {
//     U16 index = stack_top(ra);
//     stack_set(ra, ctx->constants.data[index]);
//     NEXT();
// })

// INSTRUCTION(load_const_b, {
//     U16 index = stack_top(ra);
//     stack_set(rb, ctx->constants.data[index]);
//     NEXT();
// })

// INSTRUCTION(load_const_c, {
//     U16 index = stack_top(ra);
//     stack_set(rc, ctx->constants.data[index]);
//     NEXT();
// })

// //////////////////////////////////////////////////
// // load_next_const <base>
// //////////////////////////////////////////////////
INSTRUCTION(load_next_const_a, {
    READ(U16, base);
    // a.push(ctx->constants.data[base + counter]);
    a.push(1);
    NEXT();
})
INSTRUCTION_OPERANDS(load_next_const_a, IValue16, 1)

// INSTRUCTION(load_next_const_b, {
//     READ(U16, base);
//     stack_set(rb, ctx->constants.data[base + counter]);
//     NEXT();
// })

// INSTRUCTION(load_next_const_c, {
//     READ(U16, base);
//     stack_set(rc, ctx->constants.data[base + counter]);
//     NEXT();
// })
