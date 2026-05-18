#include "common.hpp"

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//                                                                            ////////////
// Move data on and off a shadow stack                                        ////////////
//                                                                            ////////////
//////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// shadow_store_X
// Store stack X onto shadow stack s0, replacing it
////////////////////////////////////////////////////////////////////////////////
#define IMPL(reg) { \
    shadow.store<0>(reg); \
}
INSTRUCTION_ABC(shadow_store, IMPL)
#undef IMPL

////////////////////////////////////////////////////////////////////////////////
// shadow_load_X
// Load stack X from shadow stack s0, replacing previos X
////////////////////////////////////////////////////////////////////////////////
#define IMPL(reg) { \
    shadow.load<0>(reg); \
}
INSTRUCTION_ABC(shadow_load, IMPL)
#undef IMPL

////////////////////////////////////////////////////////////////////////////////
// shadow_swap_X
// Swap stack X with shadow stack s0
////////////////////////////////////////////////////////////////////////////////
#define IMPL(reg) { \
    shadow.swap_with<0>(reg); \
}
INSTRUCTION_ABC(shadow_swap, IMPL)
#undef IMPL

////////////////////////////////////////////////////////////////////////////////
// shadow_push_X
// Pop value from X and push shadow stack s0
////////////////////////////////////////////////////////////////////////////////
#define IMPL(reg) { \
    auto [value] = reg.pop<1>(); \
    shadow.push<0>(value); \
}
INSTRUCTION_ABC(shadow_push, IMPL)
#undef IMPL

////////////////////////////////////////////////////////////////////////////////
// shadow_pop_X
// Pop value from shadow stack s0 and push onto stack X
////////////////////////////////////////////////////////////////////////////////
#define IMPL(reg) { \
    reg.push(shadow.pop<0>()); \
}
INSTRUCTION_ABC(shadow_pop, IMPL)
#undef IMPL

////////////////////////////////////////////////////////////////////////////////
// shadow_pull_X
// Pop the last value from shadow stack s0 and push onto stack X
////////////////////////////////////////////////////////////////////////////////
#define IMPL(reg) { \
    reg.push(shadow.pull<0>()); \
}
INSTRUCTION_ABC(shadow_pull, IMPL)
#undef IMPL

////////////////////////////////////////////////////////////////////////////////
// shadow_set
// Set the top value of shadow stack s0 to the top value of a
////////////////////////////////////////////////////////////////////////////////
INSTRUCTION(shadow_set, {
    auto s0 = shadow.get<0>();
    Stack{s0}.set<0>(a.get<0>());
    shadow.set<0>(s0);
    NEXT();
})

////////////////////////////////////////////////////////////////////////////////
// shadow_blend <control>
// Replace stack a with a blend of shadow stacks s0 and s1
// control (2 bytes):
// 0b-XXX-YYY-ZZZ-WWW  XXX = selection for destination element 0 (Top)
//                     YYY = selection for destination element 1 (M1/M)
//                     ZZZ = selection for destination element 2 (M2/W)
//                     WWW = selection for destination element 3 (Bottom)
// Bits marked with - are ignored.
// Selection format:
//  SNN: S = 0 (s0), 1 (s1). NN = 00 = source element 0 of shadow stack S
//                                01 = source element 1 of shadow stack S
//                                10 = source element 2 of shadow stack S
//                                11 = source element 3 of shadow stack S
////////////////////////////////////////////////////////////////////////////////
INSTRUCTION(shadow_blend, {
    READ(U16, control);
    __m128i permute_mask = _mm_set_epi32(
        (control >> 0) & 0x3,   // Control for the 1st element
        (control >> 4) & 0x3,   // Control for the 2nd element
        (control >> 8) & 0x3,   // Control for the 3rd element
        (control >> 12) & 0x3   // Control for the 4th element
    );

    __m128 s0 = shadow.get<0>();
    __m128 s1 = shadow.get<1>();

    // Use permutevar to rearrange elements in a and b
    __m128 perm_a = _mm_permutevar_ps(s0, permute_mask);
    __m128 perm_b = _mm_permutevar_ps(s1, permute_mask);

    // Create a blend mask based on the upper 2 bits of the control
    __m128 blend_mask = _mm_castsi128_ps(_mm_set_epi32(
        (control & 0x0004) ? -1 : 0,   // Upper 2 bits of the control for the 1st element
        (control & 0x0040) ? -1 : 0,   // Upper 2 bits of the control for the 2nd element
        (control & 0x0400) ? -1 : 0,   // Upper 2 bits of the control for the 3rd element
        (control & 0x4000) ? -1 : 0    // Upper 2 bits of the control for the 4th element
    ));

    // Blend permuted a and b based on the blend mask (using the sign bit of blend_mask)
    ra = _mm_blendv_ps(perm_a, perm_b, blend_mask);
    NEXT();
})
INSTRUCTION_OPERANDS(shadow_blend, ShadowBlendMask, 1)


//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//                                                                            ////////////
// Move data between shadow stacks                                            ////////////
//                                                                            ////////////
//////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// shadow_swap
// Swap the contents of shadow stack s0 with that of shadow stack s1
////////////////////////////////////////////////////////////////////////////////
INSTRUCTION(shadow_swap, {
    shadow.swap();
    NEXT();
})

////////////////////////////////////////////////////////////////////////////////
// shadow_copy_X_Y
// Copy the contents of shadow stack X onto shadow stack Y
////////////////////////////////////////////////////////////////////////////////
INSTRUCTION(shadow_copy_s0_s1, {
    shadow.set<1>(shadow.get<0>());
    NEXT();
})

INSTRUCTION(shadow_copy_s1_s0, {
    shadow.set<0>(shadow.get<1>());
    NEXT();
})

////////////////////////////////////////////////////////////////////////////////
// shadow_push_X_Y
// Copy the contents of shadow stack X onto shadow stack Y
////////////////////////////////////////////////////////////////////////////////
INSTRUCTION(shadow_push_s0_s1, {
    shadow.push<1>(shadow.pop<0>());
    NEXT();
})

INSTRUCTION(shadow_push_s1_s0, {
    shadow.push<0>(shadow.pop<1>());
    NEXT();
})


//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//                                                                            ////////////
// Shuffle data on shadow stacks                                              ////////////
//                                                                            ////////////
//////////////////////////////////////////////////////////////////////////////////////////

INSTRUCTION(shadow_shuf, {
    READ(U8, selector);
    __m128i control = _mm_set_epi32(
        selector & 0x3,
        (selector >> 2) & 0x3,
        (selector >> 4) & 0x3,
        (selector >> 6) & 0x3
    );
    rb = _mm_permutevar_ps(rb, control);
    NEXT();
})
INSTRUCTION_OPERANDS(shadow_shuf, StackSelector, 1)
