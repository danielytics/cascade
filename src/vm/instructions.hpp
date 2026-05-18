

#ifdef INSTRUCTION_IMPLS
#include "instructions_impl.hpp"
#include "instructions_list.hpp"
#include <array>
extern std::array<InstructionFunction*, Instructions::COUNT> instructions_table;
#endif

/*
 * No operation. Do nothing.
 */
#include "instructions/common.hpp"
INSTRUCTION(nop, NEXT();)

/*
 * Stack Manipulation Instructions
 *
 * Push, pop, shuffle, copy between stacks, etc
 */
#include "instructions/stack.def.cpp"

/*
 * Arithmetic and Logic Instructions
 *
 * Perform calculations with values on the stacks
 */
#include "instructions/arithmetic.def.cpp"

/*
 * Comparison Instrucions
 *
 * Compare values on the stacks against each other
 */
#include "instructions/comparison.def.cpp"

/*
 * Shadow Stack Manipulation Instructions
 *
 * Move data to and from shadow stacks, shuffle shadow stacks
 */
#include "instructions/shadow_stack.def.cpp"

/*
 * Control Flow Instructions
 *
 * Conditional branching of instructions
 */
#include "instructions/control_flow.def.cpp"

/*
 * Memory Access Instructions
 *
 * Load data from memory, store data to memory
 */
#include "instructions/memory_access.def.cpp"

/*
 * Conditional Instructions
 *
 * Branchless conditional operations on stack values
 */
#include "instructions/conditional.def.cpp"

/*
 * Function Instructions
 *
 * Call/return bytecode and native functions, create function objects
 */
#include "instructions/functions.def.cpp"

/*
 * Collection Manipulation Instructions
 *
 * Work with collection and iterable types
 */
#include "instructions/collections.def.cpp"

/*
 * 2D Vector Type Instructions
 *
 * Manipulate 2D Vector data types
 */
#include "instructions/vec2d.def.cpp"

/*
 * SIMD Vector Arithmetic Instructions
 *
 * Arithemetic on multiple values at once
 */
#include "instructions/vector.def.cpp"

/*
 * Management Instructions
 *
 * Control the VM's internal state
 */
#include "instructions/management.def.cpp"

#undef INSTRUCTION
#undef INSTRUCTION_OPERANDS

#ifdef INSTRUCTION_IMPLS
#undef INSTRUCTION_IMPLS
#undef CALL_NEXT
#undef NEXT
#undef SKIP
#undef READ
#endif
