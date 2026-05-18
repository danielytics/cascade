#include "common.hpp"

INSTRUCTION(cmp_eq, {
    ra = _mm_cmpeq_ss(ra, rb);
    b.drop<1>();
    NEXT();
})

INSTRUCTION(cmp_neq, {
    ra = _mm_cmpneq_ss(ra, rb);
    b.drop<1>();
    NEXT();
})

INSTRUCTION(cmp_gt, {
    ra = _mm_cmpgt_ss(ra, rb);
    b.drop<1>();
    NEXT();
})

INSTRUCTION(cmp_lt, {
    ra = _mm_cmplt_ss(ra, rb);
    b.drop<1>();
    NEXT();
})

INSTRUCTION(cmp_gte, {
    ra = _mm_cmpge_ss(ra, rb);
    b.drop<1>();
    NEXT();
})

INSTRUCTION(cmp_lte, {
    ra = _mm_cmple_ss(ra, rb);
    b.drop<1>();
    NEXT();
})

INSTRUCTION(cmp_z, {
    ra = _mm_cmpeq_ss(ra, _mm_setzero_ps());
    NEXT();
})

INSTRUCTION(cmp_nz, {
    ra = _mm_cmpneq_ss(ra, _mm_setzero_ps());
    NEXT();
})
