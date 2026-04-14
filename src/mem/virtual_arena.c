#include <bedrock/mem/virtual_arena.h>

struct br_virtual_arena_block {
  struct br_virtual_arena_block *prev;
  u8 *base;
  usize used;
  usize committed;
  usize reserved;
};

typedef struct br__virtual_platform_block {
  br_virtual_arena_block block;
  usize committed_total;
  usize reserved_total;
} br__virtual_platform_block;

static br_alloc_result br__virtual_arena_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static bool br__safe_add_size(usize a, usize b, usize *result) {
  if (a > SIZE_MAX - b) {
    return false;
  }

  *result = a + b;
  return true;
}

static br_status br__align_up_size(usize value, usize alignment, usize *result) {
  usize mask;

  if (!br_is_power_of_two_size(alignment)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  mask = alignment - 1u;
  if (value > SIZE_MAX - mask) {
    return BR_STATUS_OUT_OF_MEMORY;
  }

  *result = (value + mask) & ~mask;
  return BR_STATUS_OK;
}

static usize br__max_size(usize a, usize b) {
  return a > b ? a : b;
}

static usize br__min_size(usize a, usize b) {
  return a < b ? a : b;
}

static br_status br__normalize_alignment(usize alignment, usize minimum, usize *result) {
  if (alignment == 0u) {
    alignment = BR_DEFAULT_ALIGNMENT;
  }
  if (alignment < minimum) {
    alignment = minimum;
  }
  if (!br_is_power_of_two_size(alignment)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  *result = alignment;
  return BR_STATUS_OK;
}

static br_status br__align_to_pages(usize value, usize *result) {
  usize page_size = br_vm_page_size();

  if (page_size == 0u) {
    return BR_STATUS_NOT_SUPPORTED;
  }

  return br__align_up_size(value, page_size, result);
}

static br__virtual_platform_block *br__virtual_platform_from_block(br_virtual_arena_block *block) {
  return (br__virtual_platform_block *)(void *)block;
}

static br_status br__virtual_block_create(usize committed,
                                          usize reserved,
                                          usize alignment,
                                          br_virtual_arena_block **out_block) {
  br_vm_region_result region;
  br__virtual_platform_block *platform_block;
  br_status status;
  usize base_offset;
  usize total_commit;
  usize total_size;

  if (out_block == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (reserved == 0u) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  status =
    br__normalize_alignment(alignment, (usize) _Alignof(br__virtual_platform_block), &alignment);
  if (status != BR_STATUS_OK) {
    return status;
  }

  status = br__align_to_pages(committed, &committed);
  if (status != BR_STATUS_OK) {
    return status;
  }
  status = br__align_to_pages(reserved, &reserved);
  if (status != BR_STATUS_OK) {
    return status;
  }
  if (committed > reserved) {
    committed = reserved;
  }

  if (!br__safe_add_size(sizeof(br__virtual_platform_block), alignment, &total_size) ||
      !br__safe_add_size(total_size, reserved, &total_size)) {
    return BR_STATUS_OUT_OF_MEMORY;
  }

  status = br__align_up_size(sizeof(br__virtual_platform_block), alignment, &base_offset);
  if (status != BR_STATUS_OK) {
    return status;
  }
  if (!br__safe_add_size(base_offset, committed, &total_commit)) {
    return BR_STATUS_OUT_OF_MEMORY;
  }

  region = br_vm_reserve(total_size);
  if (region.status != BR_STATUS_OK) {
    return region.status;
  }

  status = br_vm_commit(region.value.data, total_commit);
  if (status != BR_STATUS_OK) {
    br_vm_release(region.value.data, region.value.size);
    return status;
  }

  platform_block = (br__virtual_platform_block *)(void *)region.value.data;
  memset(platform_block, 0, sizeof(*platform_block));
  platform_block->committed_total = total_commit;
  platform_block->reserved_total = region.value.size;
  platform_block->block.base = region.value.data + base_offset;
  platform_block->block.committed = committed;
  platform_block->block.reserved = region.value.size - base_offset;

  *out_block = &platform_block->block;
  return BR_STATUS_OK;
}

static void br__virtual_block_destroy(br_virtual_arena_block *block) {
  br__virtual_platform_block *platform_block;

  if (block == NULL) {
    return;
  }

  platform_block = br__virtual_platform_from_block(block);
  br_vm_release(platform_block, platform_block->reserved_total);
}

static br_status br__virtual_block_commit_if_needed(br_virtual_arena_block *block,
                                                    usize size,
                                                    usize default_commit_size) {
  br__virtual_platform_block *platform_block;
  br_status status;
  usize base_offset;
  usize extra_size;
  usize total_commit;

  if (block == NULL) {
    return BR_STATUS_OUT_OF_MEMORY;
  }
  if (block->committed - block->used >= size) {
    return BR_STATUS_OK;
  }

  platform_block = br__virtual_platform_from_block(block);
  base_offset = (usize)(block->base - (u8 *)(void *)platform_block);

  extra_size = br__max_size(size, block->committed >> 1u);
  if (!br__safe_add_size(base_offset, block->used, &total_commit) ||
      !br__safe_add_size(total_commit, extra_size, &total_commit)) {
    return BR_STATUS_OUT_OF_MEMORY;
  }

  status = br__align_to_pages(total_commit, &total_commit);
  if (status != BR_STATUS_OK) {
    return status;
  }

  /*
  Odin compares the platform total commit directly against the arena-level
  default commit size. Bedrock keeps that shape for compatibility even though
  the value is nominally expressed in payload bytes rather than header-inclusive
  bytes.
  */
  total_commit = br__max_size(total_commit, default_commit_size);
  total_commit = br__min_size(total_commit, platform_block->reserved_total);

  if (total_commit <= platform_block->committed_total) {
    return BR_STATUS_OK;
  }

  status = br_vm_commit(platform_block, total_commit);
  if (status != BR_STATUS_OK) {
    return status;
  }

  platform_block->committed_total = total_commit;
  block->committed = total_commit - base_offset;
  return BR_STATUS_OK;
}

static br_alloc_result br__virtual_block_alloc(br_virtual_arena_block *block,
                                               usize size,
                                               usize alignment,
                                               usize default_commit_size,
                                               bool zeroed) {
  br_status status;
  usize alignment_offset;
  usize aligned;
  usize total_size;
  uptr current;
  u8 *ptr;

  if (block == NULL) {
    return br__virtual_arena_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  status = br__normalize_alignment(alignment, 1u, &alignment);
  if (status != BR_STATUS_OK) {
    return br__virtual_arena_result(NULL, 0u, status);
  }
  if (size == 0u) {
    return br__virtual_arena_result(NULL, 0u, BR_STATUS_OK);
  }

  current = (uptr)(void *)(block->base + block->used);
  status = br__align_up_size((usize)current, alignment, &aligned);
  if (status != BR_STATUS_OK) {
    return br__virtual_arena_result(NULL, 0u, status);
  }
  alignment_offset = aligned - (usize)current;

  if (!br__safe_add_size(size, alignment_offset, &total_size) ||
      total_size > (block->reserved - block->used)) {
    return br__virtual_arena_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  status = br__virtual_block_commit_if_needed(block, total_size, default_commit_size);
  if (status != BR_STATUS_OK) {
    return br__virtual_arena_result(NULL, 0u, status);
  }

  ptr = block->base + block->used + alignment_offset;
  block->used += total_size;
  if (zeroed) {
    memset(ptr, 0, size);
  }

  return br__virtual_arena_result(ptr, size, BR_STATUS_OK);
}

static void br__virtual_arena_free_last_block(br_virtual_arena *arena) {
  br_virtual_arena_block *free_block;

  if (arena == NULL || arena->curr_block == NULL) {
    return;
  }

  free_block = arena->curr_block;
  arena->total_used -= free_block->used;
  arena->total_reserved -= free_block->reserved;
  arena->curr_block = free_block->prev;
  br__virtual_block_destroy(free_block);
}

static br_alloc_result br__virtual_arena_alloc_internal(br_virtual_arena *arena,
                                                        usize size,
                                                        usize alignment,
                                                        bool zeroed) {
  br_alloc_result result;
  br_status status;
  usize prev_used;
  usize default_commit_size;
  usize minimum_block_size;
  usize needed;
  usize block_size;
  br_virtual_arena_block *new_block;

  if (arena == NULL) {
    return br__virtual_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (size == 0u) {
    return br__virtual_arena_result(NULL, 0u, BR_STATUS_OK);
  }

  switch (arena->kind) {
    case BR_VIRTUAL_ARENA_KIND_GROWING:
      prev_used = arena->curr_block != NULL ? arena->curr_block->used : 0u;
      result = br__virtual_block_alloc(
        arena->curr_block, size, alignment, arena->default_commit_size, zeroed);
      if (result.status == BR_STATUS_OUT_OF_MEMORY) {
        minimum_block_size = arena->minimum_block_size;
        if (minimum_block_size == 0u) {
          minimum_block_size = BR_VIRTUAL_ARENA_DEFAULT_GROWING_MINIMUM_BLOCK_SIZE;
          status = br__align_to_pages(minimum_block_size, &minimum_block_size);
          if (status != BR_STATUS_OK) {
            return br__virtual_arena_result(NULL, 0u, status);
          }
          arena->minimum_block_size = minimum_block_size;
        }

        default_commit_size = arena->default_commit_size;
        if (default_commit_size == 0u) {
          default_commit_size =
            br__min_size(BR_VIRTUAL_ARENA_DEFAULT_GROWING_COMMIT_SIZE, minimum_block_size);
          status = br__align_to_pages(default_commit_size, &default_commit_size);
          if (status != BR_STATUS_OK) {
            return br__virtual_arena_result(NULL, 0u, status);
          }
          arena->default_commit_size = default_commit_size;
        }

        default_commit_size = br__min_size(arena->default_commit_size, arena->minimum_block_size);
        minimum_block_size = br__max_size(arena->default_commit_size, arena->minimum_block_size);
        arena->default_commit_size = default_commit_size;
        arena->minimum_block_size = minimum_block_size;

        status = br__normalize_alignment(alignment, 1u, &alignment);
        if (status != BR_STATUS_OK) {
          return br__virtual_arena_result(NULL, 0u, status);
        }
        status = br__align_up_size(size, alignment, &needed);
        if (status != BR_STATUS_OK) {
          return br__virtual_arena_result(NULL, 0u, status);
        }

        needed = br__max_size(needed, default_commit_size);
        block_size = br__max_size(needed, minimum_block_size);

        status = br__virtual_block_create(needed, block_size, alignment, &new_block);
        if (status != BR_STATUS_OK) {
          return br__virtual_arena_result(NULL, 0u, status);
        }

        new_block->prev = arena->curr_block;
        arena->curr_block = new_block;
        arena->total_reserved += new_block->reserved;
        prev_used = 0u;
        result = br__virtual_block_alloc(
          arena->curr_block, size, alignment, arena->default_commit_size, zeroed);
      }

      if (result.status == BR_STATUS_OK && arena->curr_block != NULL) {
        arena->total_used += arena->curr_block->used - prev_used;
      }
      return result;

    case BR_VIRTUAL_ARENA_KIND_STATIC:
      result = br__virtual_block_alloc(
        arena->curr_block, size, alignment, arena->default_commit_size, zeroed);
      if (result.status == BR_STATUS_OK && arena->curr_block != NULL) {
        arena->total_used = arena->curr_block->used;
      }
      return result;

    case BR_VIRTUAL_ARENA_KIND_NONE:
      return br__virtual_arena_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }

  return br__virtual_arena_result(NULL, 0u, BR_STATUS_INVALID_STATE);
}

static br_alloc_result br__virtual_arena_allocator_fn(void *ctx, const br_alloc_request *req) {
  br_alloc_result result;
  br_virtual_arena *arena = (br_virtual_arena *)ctx;

  if (req == NULL) {
    return br__virtual_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
      return br__virtual_arena_alloc_internal(arena, req->size, req->alignment, true);
    case BR_ALLOC_OP_ALLOC_UNINIT:
      return br__virtual_arena_alloc_internal(arena, req->size, req->alignment, false);
    case BR_ALLOC_OP_RESIZE:
    case BR_ALLOC_OP_RESIZE_UNINIT:
      if (req->ptr == NULL) {
        return br__virtual_arena_alloc_internal(
          arena, req->size, req->alignment, req->op == BR_ALLOC_OP_RESIZE);
      }
      if (req->old_size == 0u) {
        return br__virtual_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
      }
      if (req->size == req->old_size) {
        return br__virtual_arena_result(req->ptr, req->size, BR_STATUS_OK);
      }
      if (req->size < req->old_size) {
        return br__virtual_arena_result(req->ptr, req->size, BR_STATUS_OK);
      }

      /*
      Bedrock mirrors Odin's current-block tail growth where possible, but we
      still fall back to allocate-and-copy because the C allocator ABI does not
      track ownership metadata for arbitrary old pointers.
      */
      if (arena != NULL && arena->curr_block != NULL) {
        br_virtual_arena_block *block = arena->curr_block;
        u8 *ptr = (u8 *)req->ptr;
        usize start;
        usize old_end;
        usize extra;
        br_status status;

        if (ptr >= block->base && ptr <= block->base + block->used) {
          start = (usize)(ptr - block->base);
          if (req->old_size <= block->reserved - start) {
            old_end = start + req->old_size;
            if (old_end == block->used) {
              extra = req->size - req->old_size;
              status = br__virtual_block_commit_if_needed(block, extra, arena->default_commit_size);
              if (status == BR_STATUS_OK && extra <= block->reserved - block->used) {
                block->used += extra;
                arena->total_used += extra;
                if (req->op == BR_ALLOC_OP_RESIZE) {
                  memset(ptr + req->old_size, 0, extra);
                }
                return br__virtual_arena_result(req->ptr, req->size, BR_STATUS_OK);
              }
            }
          }
        }
      }

      result = br__virtual_arena_alloc_internal(
        arena, req->size, req->alignment, req->op == BR_ALLOC_OP_RESIZE);
      if (result.status != BR_STATUS_OK) {
        return result;
      }

      memcpy(result.ptr, req->ptr, br_min_size(req->old_size, req->size));
      return result;
    case BR_ALLOC_OP_FREE:
      return br__virtual_arena_result(NULL, 0u, BR_STATUS_OK);
    case BR_ALLOC_OP_RESET:
      br_virtual_arena_reset(arena);
      return br__virtual_arena_result(NULL, 0u, BR_STATUS_OK);
  }

  return br__virtual_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

void br_virtual_arena_init(br_virtual_arena *arena) {
  if (arena == NULL) {
    return;
  }

  memset(arena, 0, sizeof(*arena));
}

br_status br_virtual_arena_init_growing(br_virtual_arena *arena, usize reserved) {
  br_status status;

  if (arena == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (arena->kind != BR_VIRTUAL_ARENA_KIND_NONE || arena->curr_block != NULL) {
    return BR_STATUS_INVALID_STATE;
  }

  if (reserved == 0u) {
    reserved = arena->minimum_block_size != 0u
                 ? arena->minimum_block_size
                 : BR_VIRTUAL_ARENA_DEFAULT_GROWING_MINIMUM_BLOCK_SIZE;
  }

  if (arena->minimum_block_size != 0u) {
    status = br__align_to_pages(arena->minimum_block_size, &arena->minimum_block_size);
    if (status != BR_STATUS_OK) {
      return status;
    }
  }
  if (arena->default_commit_size != 0u) {
    status = br__align_to_pages(arena->default_commit_size, &arena->default_commit_size);
    if (status != BR_STATUS_OK) {
      return status;
    }
  }

  status = br__virtual_block_create(0u, reserved, BR_DEFAULT_ALIGNMENT, &arena->curr_block);
  if (status != BR_STATUS_OK) {
    return status;
  }

  arena->kind = BR_VIRTUAL_ARENA_KIND_GROWING;
  arena->total_used = 0u;
  arena->total_reserved = arena->curr_block->reserved;
  if (arena->minimum_block_size == 0u) {
    arena->minimum_block_size = arena->curr_block->reserved;
  }

  return BR_STATUS_OK;
}

br_status br_virtual_arena_init_static(br_virtual_arena *arena, usize reserved, usize commit_size) {
  br_status status;

  if (arena == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (arena->kind != BR_VIRTUAL_ARENA_KIND_NONE || arena->curr_block != NULL) {
    return BR_STATUS_INVALID_STATE;
  }

  if (reserved == 0u) {
    reserved = arena->minimum_block_size != 0u ? arena->minimum_block_size
                                               : BR_VIRTUAL_ARENA_DEFAULT_STATIC_RESERVE_SIZE;
  }
  if (commit_size == 0u) {
    commit_size = BR_VIRTUAL_ARENA_DEFAULT_STATIC_COMMIT_SIZE;
  }

  if (arena->default_commit_size != 0u) {
    status = br__align_to_pages(arena->default_commit_size, &arena->default_commit_size);
    if (status != BR_STATUS_OK) {
      return status;
    }
  }
  if (arena->minimum_block_size != 0u) {
    status = br__align_to_pages(arena->minimum_block_size, &arena->minimum_block_size);
    if (status != BR_STATUS_OK) {
      return status;
    }
  }

  status =
    br__virtual_block_create(commit_size, reserved, BR_DEFAULT_ALIGNMENT, &arena->curr_block);
  if (status != BR_STATUS_OK) {
    return status;
  }

  arena->kind = BR_VIRTUAL_ARENA_KIND_STATIC;
  arena->total_used = 0u;
  arena->total_reserved = arena->curr_block->reserved;
  if (arena->minimum_block_size == 0u) {
    arena->minimum_block_size = arena->curr_block->reserved;
  }

  return BR_STATUS_OK;
}

void br_virtual_arena_reset(br_virtual_arena *arena) {
  if (arena == NULL) {
    return;
  }

  switch (arena->kind) {
    case BR_VIRTUAL_ARENA_KIND_GROWING:
      while (arena->curr_block != NULL && arena->curr_block->prev != NULL) {
        br__virtual_arena_free_last_block(arena);
      }
      if (arena->curr_block != NULL && arena->curr_block->used != 0u) {
        memset(arena->curr_block->base, 0, arena->curr_block->used);
        arena->curr_block->used = 0u;
      }
      arena->total_used = 0u;
      if (arena->curr_block != NULL) {
        arena->total_reserved = arena->curr_block->reserved;
      }
      break;

    case BR_VIRTUAL_ARENA_KIND_STATIC:
      if (arena->curr_block != NULL && arena->curr_block->used != 0u) {
        memset(arena->curr_block->base, 0, arena->curr_block->used);
        arena->curr_block->used = 0u;
      }
      arena->total_used = 0u;
      break;

    case BR_VIRTUAL_ARENA_KIND_NONE:
      break;
  }
}

void br_virtual_arena_destroy(br_virtual_arena *arena) {
  br_virtual_arena_block *block;
  br_virtual_arena_block *prev;

  if (arena == NULL) {
    return;
  }

  block = arena->curr_block;
  while (block != NULL) {
    prev = block->prev;
    br__virtual_block_destroy(block);
    block = prev;
  }

  memset(arena, 0, sizeof(*arena));
}

br_virtual_arena_mark br_virtual_arena_mark_save(const br_virtual_arena *arena) {
  br_virtual_arena_mark mark;

  mark.block = NULL;
  mark.used = 0u;
  if (arena != NULL && arena->curr_block != NULL) {
    mark.block = arena->curr_block;
    mark.used = arena->curr_block->used;
  }
  return mark;
}

br_status br_virtual_arena_rewind(br_virtual_arena *arena, br_virtual_arena_mark mark) {
  br_virtual_arena_block *block;
  usize old_used;

  if (arena == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  switch (arena->kind) {
    case BR_VIRTUAL_ARENA_KIND_GROWING:
      if (mark.block == NULL) {
        return BR_STATUS_INVALID_ARGUMENT;
      }

      for (block = arena->curr_block; block != NULL; block = block->prev) {
        if (block == mark.block) {
          break;
        }
      }
      if (block == NULL) {
        return BR_STATUS_INVALID_ARGUMENT;
      }

      while (arena->curr_block != mark.block) {
        br__virtual_arena_free_last_block(arena);
      }

      if (arena->curr_block == NULL || mark.used > arena->curr_block->used) {
        return BR_STATUS_INVALID_ARGUMENT;
      }

      old_used = arena->curr_block->used;
      if (old_used > mark.used) {
        memset(arena->curr_block->base + mark.used, 0, old_used - mark.used);
        arena->curr_block->used = mark.used;
        arena->total_used -= old_used - mark.used;
      }
      return BR_STATUS_OK;

    case BR_VIRTUAL_ARENA_KIND_STATIC:
      if (arena->curr_block == NULL || mark.block != arena->curr_block ||
          mark.used > arena->curr_block->used) {
        return BR_STATUS_INVALID_ARGUMENT;
      }

      old_used = arena->curr_block->used;
      if (old_used > mark.used) {
        memset(arena->curr_block->base + mark.used, 0, old_used - mark.used);
        arena->curr_block->used = mark.used;
        arena->total_used -= old_used - mark.used;
      }
      return BR_STATUS_OK;

    case BR_VIRTUAL_ARENA_KIND_NONE:
      return mark.block == NULL ? BR_STATUS_OK : BR_STATUS_INVALID_STATE;
  }

  return BR_STATUS_INVALID_STATE;
}

br_alloc_result br_virtual_arena_alloc(br_virtual_arena *arena, usize size, usize alignment) {
  return br__virtual_arena_alloc_internal(arena, size, alignment, true);
}

br_alloc_result
br_virtual_arena_alloc_uninit(br_virtual_arena *arena, usize size, usize alignment) {
  return br__virtual_arena_alloc_internal(arena, size, alignment, false);
}

br_allocator br_virtual_arena_allocator(br_virtual_arena *arena) {
  br_allocator allocator;

  allocator.fn = br__virtual_arena_allocator_fn;
  allocator.ctx = arena;
  return allocator;
}
