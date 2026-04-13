#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <bedrock.h>

#define BR_VEC_T int
#define BR_VEC_NAME br_test_int_vec
#include <bedrock/template/vec.h>

static void test_heap_allocator(void) {
    br_alloc_result alloc;
    br_alloc_result resized;
    int *values;

    alloc = br_allocator_alloc(br_allocator_heap(), sizeof(int) * 4u, (usize) _Alignof(int));
    assert(alloc.status == BR_STATUS_OK);
    assert(alloc.ptr != NULL);

    values = (int *)alloc.ptr;
    for (usize i = 0; i < 4u; ++i) {
        assert(values[i] == 0);
        values[i] = (int)(i + 1u);
    }

    resized = br_allocator_resize(
        br_allocator_heap(), values, sizeof(int) * 4u, sizeof(int) * 8u, (usize) _Alignof(int));
    assert(resized.status == BR_STATUS_OK);
    assert(resized.ptr != NULL);

    values = (int *)resized.ptr;
    for (usize i = 0; i < 4u; ++i) {
        assert(values[i] == (int)(i + 1u));
    }
    for (usize i = 4u; i < 8u; ++i) {
        assert(values[i] == 0);
    }

    assert(br_allocator_free(br_allocator_heap(), values, sizeof(int) * 8u) == BR_STATUS_OK);
}

static void test_arena(void) {
    u8 storage[128];
    br_arena arena;
    br_arena_mark mark;
    br_alloc_result first;
    br_alloc_result second;
    br_alloc_result third;
    u32 *value;

    br_arena_init(&arena, storage, sizeof(storage));

    first = br_arena_alloc(&arena, 16u, 16u);
    assert(first.status == BR_STATUS_OK);
    assert(first.ptr != NULL);
    assert(((uptr)first.ptr & 15u) == 0u);

    mark = br_arena_mark_save(&arena);
    second = br_arena_alloc_uninit(&arena, 32u, 8u);
    assert(second.status == BR_STATUS_OK);
    assert(second.ptr != NULL);

    assert(br_arena_rewind(&arena, mark) == BR_STATUS_OK);

    third = br_allocator_alloc(br_arena_allocator(&arena), sizeof(u32), (usize) _Alignof(u32));
    assert(third.status == BR_STATUS_OK);
    assert(third.ptr != NULL);

    value = (u32 *)third.ptr;
    assert(*value == 0u);
    *value = 0xdeadbeefu;
    assert(*value == 0xdeadbeefu);
}

static void test_vec(void) {
    br_test_int_vec vec;
    int popped;

    br_test_int_vec_init(&vec, br_allocator_heap());
    for (int i = 0; i < 32; ++i) {
        assert(br_test_int_vec_push(&vec, i * 3) == BR_STATUS_OK);
    }

    assert(vec.len == 32u);
    assert(vec.cap >= vec.len);
    assert(vec.data[0] == 0);
    assert(vec.data[31] == 93);

    assert(br_test_int_vec_pop(&vec, &popped) == BR_STATUS_OK);
    assert(popped == 93);
    assert(vec.len == 31u);

    br_test_int_vec_destroy(&vec);
}

static void test_fail_allocator(void) {
    br_test_int_vec vec;

    br_test_int_vec_init(&vec, br_allocator_fail());
    assert(br_test_int_vec_push(&vec, 1) == BR_STATUS_OUT_OF_MEMORY);
    br_test_int_vec_destroy(&vec);
}

int main(void) {
    test_heap_allocator();
    test_arena();
    test_vec();
    test_fail_allocator();
    return 0;
}
