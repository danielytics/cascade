# Cascade

Cascade is an experimental bytecode virtual machine and fast single-pass assembler written in C++, originally written in 2024.

It is based on an idea that I believe to be pretty novel:
1. The insturction set is stack based
2. There are multiple small stacks
3. The stacks are kept entirely in machine registers using SSE and AVX
4. A rich instruction set means that instructions are very close to native (very little overhaed, no instruction decoding, few conditional branches)

The hypothesis was that by using SIMD registers to hold the stacks, we can achieve high performance as no actual memory is accessed for stack operations.

## Run

Install [Tup](https://gittup.org/tup/), Clang, `libc++`

```bash
tup variant builds/*.config
tup
build-dev/casm example.cb
```

## Virtual Machine notes

The design uses four small stacks:

* The primary stacks: `a` and `b`.
  
  Most operations operate directly on these two stacks, eg `add` will effectively run `a.push(a.pop() + b.pop())`
* The secondary stack: `c`

  This stack works the exact same as the primary stacks and shares many of the same instructions, however is slightly limited in that not all instructions can operate on it.
  Some operations are defined for `a` and `c` but not for `b`, eg `add_a` and `add_c` but no `add_b` (`add_c` => `c.push(c.pop() + c.pop())`).
* The shadow stacks: `s0`, `s1`

  These stacks are used to store data temporarily. Instructions exist to move data from the main stacks to and from the shadow stacks, including swapping. There are operations to move data between `s0` and `s1`.
  The purpose is to spill data to the shadow stacks to free up space on the main stacks, that you will need again soon.

All stacks are 4 items deep and each item is 32 bits (either an int32 or a float, the VM is typeless and the type depends on the instructions used, no type validation is done).
The 3 main stacks `a`, `b`, and `c` are implemented as individual 128bit SSE registers.
The shadow stacks `s0` and `s1` are implemented as a single 256bit AVX register (`s0` is the low lane and `s1` is the high lane).

Instruction dispatch is done using a direct threaded tail call approach using Clang's `__attribute__(musttail)` attribute.

This design means that instructions are quite small, generally only a few machine cycles each. The C++ code is heavily macro based to reduce boilerplate and the assembly generated tends to be quite tight.

Below are some instructions, showing their implementations and the generated assembly:

1. `add_a`

Semantics: `a.push(a.pop() + a.pop())`
```c++
INSTRUCTION(add_a, {
    const auto [x, y] = a.pop<2>();
    a.push(x + y);
    NEXT();
})
```
This is compiled to:
```asm
; a[0] + a[1]; a is xmm0
    dc40:	vmovshdup xmm4,xmm0             ; xmm4[0] will contain xmm0[1], ie a[1]
    dc44:	vaddps xmm4,xmm0,xmm4           ; xmm4[0] = xmm4[0] + xmm0[0], essentially xmm4[0] = a[0] + a[1]
; xmm5 = 0
    dc48:	vxorps xmm5,xmm5,xmm5
; Pop a[1]: shift a[2..3] up by one, leaving 0 in a[3] 
    dc4c:	vblendps xmm0,xmm0,xmm5,0x3     ; set the first two elements of xmm0 to 0 (xmm5): [0, 0, c, d]
    dc52:	vshufps xmm0,xmm0,xmm0,0x38     ; shift the bottom two values up and set lowest to 0: [0, c, d, 0] 
    dc57:	vblendps xmm0,xmm0,xmm4,0x1     ; set the first element to the result: [result, c, d, 0]
; Dispatch logic (read next instruction addr, increment instruction pointer, jump to addr)
    dc5d:	movzx  eax,BYTE PTR [rdi]       ; Read the next instruction, eax contains the instruction bytecode (an offset into the instruction_table)
    dc60:	lea    r9,[rip+0x1a1c1]         ; Get the base address of the instructions_table
    dc67:	mov    rax,QWORD PTR [r9+rax*8] ; Read the address of the next instruction from the instruction_table (base + bytecode/offset)
    dc6b:	inc    rdi                      ; Increment instruction pointer, advancing to next instruction
    dc6e:	jmp    rax                      ; Jump to the next instructions code
```
This ipmlements the add operation directly, then using SSE shuffle and blend instructions to arrange the stack into the shape that the `pop, pop, push` would have left it in.


2. `add`

Semantics: `a.push(a.pop() + b.pop())`
```c++
INSTRUCTION(add, {
    a.set<0>(a.get<0>() + b.get<0>());
    b.drop<1>();
    NEXT();
})
```
This is compiled to:
```asm
; Add a[0] (xmm0) and b[0] (xmm1)
    d730:	vaddss xmm0,xmm0,xmm1
; xmm4 = 0
    d734:	vxorps xmm4,xmm4,xmm4
; b.pop() ie shift b by one element and zero the last
    d738:	vblendps xmm1,xmm1,xmm4,0x1 ; Set top to 0: [a, b, c, d] => [0, b, c, d]
    d73e:	vshufps xmm1,xmm1,xmm1,0x39 ; Rotate to shift everything up: [b, c, d, 0]
; Dispatch logic, same as before
    d743:	movzx  eax,BYTE PTR [rdi]
    d746:	lea    r9,[rip+0x1a6db]
    d74d:	mov    rax,QWORD PTR [r9+rax*8]
    d751:	inc    rdi
    d754:	jmp    rax
```
3. `shadow_blend`

This is a complex instruction that replaces `a` with a blend of shadow stacks `s0` and `s1`.
```c++
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

    // Use permutevar to rearrange elements in s0 and s1
    __m128 perm_a = _mm_permutevar_ps(s0, permute_mask);
    __m128 perm_b = _mm_permutevar_ps(s1, permute_mask);

    // Create a blend mask based on the upper 2 bits of the control
    __m128 blend_mask = _mm_castsi128_ps(_mm_set_epi32(
        (control & 0x0004) ? -1 : 0,   // Upper 2 bits of the control for the 1st element
        (control & 0x0040) ? -1 : 0,   // Upper 2 bits of the control for the 2nd element
        (control & 0x0400) ? -1 : 0,   // Upper 2 bits of the control for the 3rd element
        (control & 0x4000) ? -1 : 0    // Upper 2 bits of the control for the 4th element
    ));

    // Blend permuted s0 and s1 based on the blend mask (using the sign bit of blend_mask)
    ra = _mm_blendv_ps(perm_a, perm_b, blend_mask);
    NEXT();
})
```
This is compiled to:
```asm
    e850:	movzx  eax,WORD PTR [rdi]
    e853:	vmovd  xmm0,eax
    e857:	vpbroadcastd xmm0,xmm0
    e85c:	vpsrlvd xmm4,xmm0,XMMWORD PTR [rip+0xf7eb]
    e865:	vextractf128 xmm5,ymm3,0x1
    e86b:	vpermilps xmm6,xmm3,xmm4
    e870:	vpsllvd xmm0,xmm0,XMMWORD PTR [rip+0xf7e7]
    e879:	vpermilps xmm4,xmm5,xmm4
    e87e:	vblendvps xmm0,xmm6,xmm4,xmm0
    e884:	movzx  eax,BYTE PTR [rdi+0x2]
    e888:	lea    r9,[rip+0x19599]
    e88f:	mov    rax,QWORD PTR [r9+rax*8]
    e893:	add    rdi,0x3
    e897:	jmp    rax
```

Most operations are similar: direct SSE operations for arithmetic and logic, shuffles and blends for stack operations, broadcast, extract, permutations for operating on the AVX lanes. Only loops and function call/return use actual memory access (outside of actual memory operations and FFI).

### Runtime Behavior

The hypothesis was that by keeping all stack operations in machine registers by leveraging wide SIMD registers to keep multiple values of a stack in registers, that performance would be very good.
There were a number of hurdles with this approach:
1. Implementing this in C++ meant relying on musttail and calling conventions. That meant that in order to keep data in registers without spilling, insturctions are limited to ~6 scalar values and ~6 vector values.
2. The original plan was to use AVX instructions for the primary stacks, allowing 8 32bit values per stack. The original design fused two together for a 16 deep stack. However, due to how x86_64 implements 256bit registers as two indedependent lanes, this apporach turned out to be a dead end: you cannot easily cross the lane boundaries between the two 128bit halves. For example, shifting would shift each lane separately without crossing.
Given these restrictions, using SSE for main operations and AVX only for additional temp space was the simpler and more efficient approach.

The rich instruction set means that most insturcitons are very efficient, however a few things became clear on profiling and testing:
1. Instructions being so efficient meant they are dominated by the dispatch overhead
2. A rich instruction set meant few conditional branches (good!) but many more instructions leading to more icache misses (bad!)

Ultimately, the VM performance is limited by icache misses and dispatch overhead. Its fast, but not as fast as I'd hoped.

## Assembler

The assembler was designed to be high level enough to be easy to use (ie branches and loops have a concept of blocks), yet directly map to instructions and be extremely fast. The performance was achieved by being single pass: the assembly text is read linearly once from start to finish, generating the bytecode directly in one pass. This is achieved by by inserting padding bytes when values are not yet known, remembering what needs to be updated, and patching it when the value is known.
For example, a conditional branch in bytecode stores an immediate offset to jump to if the branch is not taken. The assembler will fill this with 0 and remember that it needs to be patched. When it encounters the closing block token, it patches the current address offset to the remembered location.
Name lookups (instructions, functions) are also done one character at a time as the input is read, by storing all symbols in a trie. This means that symbol and keyword lookups are performed as the input text is being read.

Example code:
```
RUN: main
FUNC: main {
    push2_a 12 10
    push2_b 10 10
    ; Push 1 to c if a.pop() > b.pop() [10 > 10, false]
    cmp_gt
    when {
        push_c 1
    }
    ; Push 1 to c if a.pop() > b.pop() [12 > 10, true]
    cmp_gt
    when {
        push_c 2
    }
    end
}
```

The result is:

```
build-dev/casm example.cb
Size of bytecode: 39 bytes.
Executing:
Size of stack: 1024 (entry: 32), size of vm_state: 1024 (0)
{
  [IP: 0, SP: 0, Counter: 0]
  [ a: 0 0 0 0 | b: 0 0 0 0 | c: 2 0 0 0]
  [ s0: 0 0 0 0 | s1: 0 0 0 0 ]
}
Slots:
00000000 (0)  00000000 (0)  00000000 (0)  00000000 (0)  00000000 (0)  00000000 (0)  00000000 (0)  00000000 (0)
00000000 (0)  00000000 (0)  00000000 (0)  00000000 (0)  00000000 (0)  00000000 (0)  00000000 (0)  00000000 (0)
00000000 (0)  00000000 (0)  00000000 (0)  00000000 (0)  00000000 (0)  00000000 (0)  00000000 (0)  00000000 (0)
00000000 (0)  00000000 (0)  00000000 (0)  00000000 (0)  00000000 (0)  00000000 (0)  00000000 (0)  00000000 (0)
Ok.
```
Stack `c` contains: `2 0 0 0`

It is also possible to assemble instructions directly in C++:

```c++
execute(env, {
    native, FUNC(txt_create),
    set_loop, 10, 0,
        dup_a, // Make copy of text handle
        push_a, CONST('x'),
        native, FUNC(txt_putch), // Putch character into text
    end_loop,
    native, FUNC(txt_print), // Print result
    end,
  }, {constants.data(), constants.size()});
```

# Current Status

Most insturcitons are implemented: stack, arithmetic, shadow stack, control flow, FFI.

Math and Text FFI functions are implemented

Memory functions, collections, and vector instructions were never implemented.

The assembler mostly works, but is missing some features: currently it can output the bytecode to stdio for debugging, but not save it to file, only assemble-and-execute directly. Also, as far as I can remember, while directives exist to insert data into the bytecode, there's no way to actually access it.
