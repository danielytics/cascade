#include "common.hpp"

////////////////////////////////////////////////////////////////////////////////
// select_min
// Set top of a to min(top(a), top(b)) and pops b
////////////////////////////////////////////////////////////////////////////////
INSTRUCTION(select_min, {
    ra = _mm_min_ss(ra, rb);
    b.drop<1>();
    NEXT();
})

////////////////////////////////////////////////////////////////////////////////
// select_max
// Set top of a to max(top(a), top(b)) and pops b
////////////////////////////////////////////////////////////////////////////////
INSTRUCTION(select_max_a, {
    ra = _mm_max_ss(ra, rb);
    b.drop<1>();
    NEXT();
})

////////////////////////////////////////////////////////////////////////////////
// ifz
// If top of a is zero, replace with top of b, otherwise leave intact, pop b
////////////////////////////////////////////////////////////////////////////////
INSTRUCTION(ifz, {
    __m128 mask = _mm_cmpeq_ss(ra, _mm_setzero_ps());
    ra = _mm_blendv_ps(ra, rb, mask);
    b.drop<1>();
    NEXT();
})

////////////////////////////////////////////////////////////////////////////////
// ifelse
// If top of a is true, replace with top of b, else replace with second of b
// Drop top two values of b
////////////////////////////////////////////////////////////////////////////////
INSTRUCTION(ifelse, {
    auto [if_true, if_false] = b.pop<2>();
    __m128 true_value = _mm_set_ss(if_true);
    __m128 false_value = _mm_set_ss(if_false);
    __m128 mask = _mm_cmpeq_ss(ra, _mm_setzero_ps());
    __m128 result = _mm_blendv_ps(true_value, false_value, mask);
    ra = _mm_move_ss(ra, result);
    NEXT();
})
