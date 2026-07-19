#include <assert.h>
#include <string.h>

#include <bedrock.h>

/*
A block allocator that hands out blocks at an address congruent to 8 (mod 64):
8-aligned (the arena's minimum in these tests) but deliberately never
64-aligned. When such a block is later reused for a 64-aligned request, the
bump pointer sits 56 bytes below a 64 boundary, forcing the reused-block
re-alignment path to compute a real, nonzero margin. That makes the tests below
discriminate Bedrock's re-align-after-cycle fix from Odin's assume-margin-zero
behavior, which the heap allocator's incidental over-alignment could otherwise
mask. Blocks are bump-allocated from a caller-owned pool; free is a no-op.
*/
typedef struct misaligning_block_pool {
  uint8_t *base;
  size_t len;
  size_t used;
} misaligning_block_pool;

static br_alloc_result misaligning_block_fn(void *ctx, const br_alloc_request *req) {
  misaligning_block_pool *pool = (misaligning_block_pool *)ctx;
  br_alloc_result result;
  uptr cursor;
  uptr block;
  size_t offset;

  result.ptr = NULL;
  result.size = 0u;
  result.status = BR_STATUS_OK;

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
    case BR_ALLOC_OP_ALLOC_UNINIT:
      cursor = (uptr)(void *)(pool->base + pool->used);
      block = ((cursor + 63u) & ~(uptr)63u) + 8u; /* congruent to 8 (mod 64) */
      offset = (size_t)(block - (uptr)(void *)pool->base);
      if (offset + req->size > pool->len) {
        result.status = BR_STATUS_OUT_OF_MEMORY;
        return result;
      }
      pool->used = offset + req->size;
      result.ptr = (void *)block;
      result.size = req->size;
      if (req->op == BR_ALLOC_OP_ALLOC) {
        memset(result.ptr, 0, req->size);
      }
      return result;
    case BR_ALLOC_OP_FREE:
      return result;
    default:
      result.status = BR_STATUS_NOT_SUPPORTED;
      return result;
  }
}

static void test_dynamic_arena_basic_resize(void) {
  br_dynamic_arena arena;
  br_alloc_result a;
  br_alloc_result grown;

  memset(&arena, 0, sizeof(arena));
  assert(br_dynamic_arena_init(&arena, br_allocator_heap(), br_allocator_heap(), 64u, 48u, 8u) ==
         BR_STATUS_OK);

  a = br_dynamic_arena_alloc(&arena, 16u);
  assert(a.status == BR_STATUS_OK);
  assert(a.ptr != NULL);
  memset(a.ptr, 0x37, a.size);

  grown = br_dynamic_arena_resize(&arena, a.ptr, a.size, 32u);
  assert(grown.status == BR_STATUS_OK);
  assert(grown.ptr != NULL);
  assert(memcmp(grown.ptr, a.ptr, a.size) == 0);
  assert(((u8 *)grown.ptr)[16] == 0u);

  br_dynamic_arena_destroy(&arena);
}

static void test_dynamic_arena_reset_reuses_blocks(void) {
  br_tracking_allocator block_tracking;
  br_tracking_allocator array_tracking;
  br_dynamic_arena arena;
  br_alloc_result a;
  br_alloc_result b;
  br_alloc_result c;

  memset(&block_tracking, 0, sizeof(block_tracking));
  memset(&array_tracking, 0, sizeof(array_tracking));
  memset(&arena, 0, sizeof(arena));

  br_tracking_allocator_init(&block_tracking, br_allocator_heap(), br_allocator_heap());
  br_tracking_allocator_init(&array_tracking, br_allocator_heap(), br_allocator_heap());

  assert(br_dynamic_arena_init(&arena,
                               br_tracking_allocator_allocator(&block_tracking),
                               br_tracking_allocator_allocator(&array_tracking),
                               64u,
                               128u,
                               8u) == BR_STATUS_OK);

  a = br_dynamic_arena_alloc_uninit(&arena, 48u);
  b = br_dynamic_arena_alloc_uninit(&arena, 24u);
  assert(a.status == BR_STATUS_OK);
  assert(b.status == BR_STATUS_OK);
  assert(block_tracking.stats.live_allocation_count == 2u);

  br_dynamic_arena_reset(&arena);
  assert(arena.current_block == NULL);
  assert(arena.used_count == 0u);
  assert(arena.unused_count == 2u);
  assert(block_tracking.stats.live_allocation_count == 2u);

  c = br_dynamic_arena_alloc_uninit(&arena, 24u);
  assert(c.status == BR_STATUS_OK);
  assert(block_tracking.stats.total_allocation_count == 2u);

  br_dynamic_arena_destroy(&arena);
  assert(block_tracking.stats.live_allocation_count == 0u);
  assert(array_tracking.stats.live_allocation_count == 0u);
  br_tracking_allocator_destroy(&block_tracking);
  br_tracking_allocator_destroy(&array_tracking);
}

static void test_dynamic_arena_out_band_and_allocator_adapter(void) {
  br_tracking_allocator block_tracking;
  br_tracking_allocator array_tracking;
  br_dynamic_arena arena;
  br_allocator allocator;
  br_alloc_result out_band;
  br_alloc_result via_allocator;

  memset(&block_tracking, 0, sizeof(block_tracking));
  memset(&array_tracking, 0, sizeof(array_tracking));
  memset(&arena, 0, sizeof(arena));

  br_tracking_allocator_init(&block_tracking, br_allocator_heap(), br_allocator_heap());
  br_tracking_allocator_init(&array_tracking, br_allocator_heap(), br_allocator_heap());

  assert(br_dynamic_arena_init(&arena,
                               br_tracking_allocator_allocator(&block_tracking),
                               br_tracking_allocator_allocator(&array_tracking),
                               64u,
                               48u,
                               8u) == BR_STATUS_OK);

  out_band = br_dynamic_arena_alloc_uninit(&arena, 64u);
  assert(out_band.status == BR_STATUS_OK);
  assert(out_band.ptr != NULL);
  assert(arena.out_band_count == 1u);

  allocator = br_dynamic_arena_allocator(&arena);
  via_allocator = br_allocator_alloc(allocator, 16u, 32u);
  assert(via_allocator.status == BR_STATUS_OK);
  assert(via_allocator.ptr != NULL);
  assert(br_allocator_free(allocator, via_allocator.ptr, via_allocator.size) ==
         BR_STATUS_NOT_SUPPORTED);
  assert(br_allocator_reset(allocator) == BR_STATUS_OK);

  assert(block_tracking.stats.live_allocation_count == 0u);
  assert(arena.current_block == NULL);
  assert(arena.out_band_count == 0u);

  br_dynamic_arena_destroy(&arena);
  assert(array_tracking.stats.live_allocation_count == 0u);
  br_tracking_allocator_destroy(&block_tracking);
  br_tracking_allocator_destroy(&array_tracking);
}

static void test_dynamic_arena_request_alignment(void) {
  br_dynamic_arena arena;
  br_allocator allocator;
  br_alloc_result first;
  br_alloc_result inblock;
  br_alloc_result seq;
  br_alloc_result outband;
  br_alloc_result base;
  br_alloc_result grown;
  br_alloc_result direct;
  usize i;

  memset(&arena, 0, sizeof(arena));
  assert(br_dynamic_arena_init(&arena, br_allocator_heap(), br_allocator_heap(), 4096u, 512u, 8u) ==
         BR_STATUS_OK);
  allocator = br_dynamic_arena_allocator(&arena);

  /*
  Advance the bump pointer by a non-alignment-multiple amount so the following
  over-aligned request must skip a real margin inside the block.
  */
  first = br_allocator_alloc(allocator, 24u, 8u);
  assert(first.status == BR_STATUS_OK);
  assert(((uptr)first.ptr % 8u) == 0u);

  /* In-block over-aligned request routed through the generic allocator ABI. */
  inblock = br_allocator_alloc(allocator, 16u, 64u);
  assert(inblock.status == BR_STATUS_OK);
  assert(inblock.ptr != NULL);
  assert(((uptr)inblock.ptr % 64u) == 0u);

  /* Several sequential over-aligned requests each come back aligned. */
  for (i = 0u; i < 4u; ++i) {
    seq = br_allocator_alloc(allocator, 8u, 32u);
    assert(seq.status == BR_STATUS_OK);
    assert(((uptr)seq.ptr % 32u) == 0u);
  }

  /* Out-band over-aligned request (size >= out_band_size) is floored too. */
  outband = br_allocator_alloc(allocator, 512u, 64u);
  assert(outband.status == BR_STATUS_OK);
  assert(outband.ptr != NULL);
  assert(((uptr)outband.ptr % 64u) == 0u);
  assert(arena.out_band_count == 1u);

  /* Resize-grow through the ABI keeps the alignment and copies the old bytes. */
  base = br_allocator_alloc_uninit(allocator, 16u, 64u);
  assert(base.status == BR_STATUS_OK);
  memset(base.ptr, 0x5a, base.size);
  grown = br_allocator_resize(allocator, base.ptr, 16u, 48u, 64u);
  assert(grown.status == BR_STATUS_OK);
  assert(((uptr)grown.ptr % 64u) == 0u);
  assert(memcmp(grown.ptr, base.ptr, 16u) == 0);

  /* The direct entry points allocate at the arena's minimum alignment. */
  direct = br_dynamic_arena_alloc(&arena, 8u);
  assert(direct.status == BR_STATUS_OK);
  assert(((uptr)direct.ptr % 8u) == 0u);

  br_dynamic_arena_destroy(&arena);
}

static void test_dynamic_arena_large_alignment(void) {
  br_dynamic_arena arena;
  br_allocator allocator;
  br_alloc_result first;
  br_alloc_result page;

  memset(&arena, 0, sizeof(arena));
  /*
  A 4096-aligned, 4096-byte request stays in-band (below the 8192 out-band
  threshold) but, after a small leading allocation, its alignment margin can
  push it past the 8192-byte block capacity. Whether it fits the first block or
  forces a fresh (4096-aligned) block, the returned pointer must be 4096-aligned
  -- so the assertions stay agnostic to which block satisfies it.
  */
  assert(br_dynamic_arena_init(
           &arena, br_allocator_heap(), br_allocator_heap(), 8192u, 8192u, 16u) == BR_STATUS_OK);
  allocator = br_dynamic_arena_allocator(&arena);

  first = br_allocator_alloc(allocator, 40u, 16u);
  assert(first.status == BR_STATUS_OK);
  assert(((uptr)first.ptr % 16u) == 0u);

  page = br_allocator_alloc(allocator, 4096u, 4096u);
  assert(page.status == BR_STATUS_OK);
  assert(page.ptr != NULL);
  assert(((uptr)page.ptr % 4096u) == 0u);

  br_dynamic_arena_destroy(&arena);
}

/*
A block freed back to `unused_blocks` and later reused for a larger-alignment
request must be re-aligned against the block it actually got, not assumed to
start on the requested boundary. The misaligning allocator hands the reused
block back at an address 56 bytes below a 64 boundary, so a correct
implementation applies a 56-byte margin and still returns a 64-aligned pointer;
Odin's assume-margin-zero behavior would return the raw (8-aligned) block base.
*/
static void test_dynamic_arena_reused_block_realigns(void) {
  uint8_t pool_mem[1024];
  misaligning_block_pool pool;
  br_allocator block_allocator;
  br_dynamic_arena arena;
  br_allocator allocator;
  br_alloc_result seed;
  void *reused_block;
  br_alloc_result over_aligned;

  memset(pool_mem, 0, sizeof(pool_mem));
  pool.base = pool_mem;
  pool.len = sizeof(pool_mem);
  pool.used = 0u;
  block_allocator.fn = misaligning_block_fn;
  block_allocator.ctx = &pool;

  memset(&arena, 0, sizeof(arena));
  assert(br_dynamic_arena_init(&arena, block_allocator, br_allocator_heap(), 256u, 512u, 8u) ==
         BR_STATUS_OK);
  allocator = br_dynamic_arena_allocator(&arena);

  /* Create a block at minimum (8) alignment, then release it to unused_blocks. */
  seed = br_allocator_alloc(allocator, 16u, 8u);
  assert(seed.status == BR_STATUS_OK);
  reused_block = arena.current_block;
  assert(reused_block != NULL);
  assert(((uptr)reused_block % 64u) != 0u); /* the allocator's whole point */

  br_dynamic_arena_reset(&arena);
  assert(arena.current_block == NULL);
  assert(arena.unused_count == 1u);

  /* Reuse that block for a 64-aligned request through the generic ABI. */
  over_aligned = br_allocator_alloc(allocator, 16u, 64u);
  assert(over_aligned.status == BR_STATUS_OK);
  assert(arena.current_block == reused_block); /* the same block was reused */
  assert(over_aligned.ptr != NULL);
  assert(((uptr)over_aligned.ptr % 64u) == 0u); /* re-aligned despite the block base */

  br_dynamic_arena_destroy(&arena);
}

/*
When a reused block's alignment margin no longer leaves room for the request,
the arena must report OUT_OF_MEMORY rather than hand back an under-aligned
pointer. Here the 64-byte request needs a 56-byte margin in a 96-byte block:
the payload alone fits, but payload + margin does not.
*/
static void test_dynamic_arena_reused_block_margin_overflow(void) {
  uint8_t pool_mem[1024];
  misaligning_block_pool pool;
  br_allocator block_allocator;
  br_dynamic_arena arena;
  br_allocator allocator;
  br_alloc_result seed;
  br_alloc_result over_aligned;

  memset(pool_mem, 0, sizeof(pool_mem));
  pool.base = pool_mem;
  pool.len = sizeof(pool_mem);
  pool.used = 0u;
  block_allocator.fn = misaligning_block_fn;
  block_allocator.ctx = &pool;

  memset(&arena, 0, sizeof(arena));
  assert(br_dynamic_arena_init(&arena, block_allocator, br_allocator_heap(), 96u, 512u, 8u) ==
         BR_STATUS_OK);
  allocator = br_dynamic_arena_allocator(&arena);

  seed = br_allocator_alloc(allocator, 8u, 8u);
  assert(seed.status == BR_STATUS_OK);
  br_dynamic_arena_reset(&arena);
  assert(arena.unused_count == 1u);

  /* 64-byte payload fits the 96-byte block, but not after the 56-byte margin. */
  over_aligned = br_allocator_alloc(allocator, 64u, 64u);
  assert(over_aligned.status == BR_STATUS_OUT_OF_MEMORY);
  assert(over_aligned.ptr == NULL);

  br_dynamic_arena_destroy(&arena);
}

int main(void) {
  test_dynamic_arena_basic_resize();
  test_dynamic_arena_reset_reuses_blocks();
  test_dynamic_arena_out_band_and_allocator_adapter();
  test_dynamic_arena_request_alignment();
  test_dynamic_arena_large_alignment();
  test_dynamic_arena_reused_block_realigns();
  test_dynamic_arena_reused_block_margin_overflow();
  return 0;
}
