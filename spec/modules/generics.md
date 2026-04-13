# Generics

## Problem Statement

Odin uses parametric polymorphism, procedure overloading, slices, maps, and
`any` in places where C has no direct equivalent.

If Bedrock copies those APIs literally, the result will be either:

- a macro swamp
- a weakly typed `void *` swamp
- or both

So Bedrock needs a clear split by use-case.

## Rule 1: Algorithms Use Type Erasure

When an operation naturally works on raw memory, use an erased ABI.

Examples:

```c
void br_sort(void *base, size_t count, size_t elem_size,
             int (*cmp)(const void *, const void *, void *),
             void *ctx);

void *br_bsearch(const void *key, const void *base, size_t count,
                 size_t elem_size,
                 int (*cmp)(const void *, const void *, void *),
                 void *ctx);
```

This is the right tool for:

- sort/search
- slice algorithms
- byte-wise transforms
- hash-table probing internals

The caller keeps type knowledge. The library stays honest.

## Rule 2: Containers Use Macro-Generated Types

Containers should be generated as real typed structs and functions, not stored as
`void *` plus runtime element size.

Example direction:

```c
#define BR_VEC_T Foo
#define BR_VEC_NAME foo_vec
#include <bedrock/template/vec.h>
```

This should generate things like:

- `foo_vec`
- `foo_vec_init`
- `foo_vec_push`
- `foo_vec_pop`
- `foo_vec_reserve`

Why this approach:

- good type safety
- predictable debugger output
- no per-element casts at call sites
- no hidden boxing

This is the correct approach for:

- vectors
- deques
- pools
- typed hash maps and sets

## Rule 3: `_Generic` Is Sugar, Not The Foundation

On compilers with `_Generic`, we may add convenience wrappers such as:

```c
#define br_vec_push(v, x) _Generic((v), \
    foo_vec *: foo_vec_push, \
    bar_vec *: bar_vec_push \
)((v), (x))
```

But this must never be the only API. `_Generic` is opt-in sugar. The generated
typed functions remain the real interface.

## Rule 4: No Universal `any` In V1

Bedrock should not attempt an Odin-style or C++-style universal runtime `any`
for the core library.

That avoids dragging in:

- RTTI-like metadata
- formatter registries
- dynamic ownership questions
- difficult ABI commitments

This is the main reason `fmt`, `reflect`, `flags`, and most of `json` should not
be part of the first wave.

## Practical Mapping

Use this policy when porting Odin packages:

- `bytes`, `strings`, `io`, `bufio`: concrete APIs, mostly no generic issue
- `sort`, `slice`, low-level helpers: erased algorithms
- `container/*`: macro-generated typed containers
- `fmt`, `reflect`, `flags`, RTTI-heavy `json`: redesign later, not v1

## Why Not Just Use `void *` Everywhere

Because it gives the worst combination:

- weak type safety
- unclear ownership
- awkward element construction and destruction
- poor autocomplete and debugger ergonomics

Use erased APIs where they are naturally erased, and generated typed APIs where
the data structure itself is generic.
