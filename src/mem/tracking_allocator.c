#include <bedrock/mem/tracking_allocator.h>

static br_alloc_result br__tracking_alloc_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static br_allocator br__tracking_internals_allocator(const br_tracking_allocator *tracking) {
  if (tracking != NULL && tracking->internals.fn != NULL) {
    return tracking->internals;
  }

  return br_allocator_heap();
}

static bool br__tracking_reserve_entries(br_tracking_allocator *tracking, usize min_cap) {
  br_allocator allocator;
  br_alloc_result resized;
  usize new_cap;

  if (tracking == NULL) {
    return false;
  }
  if (tracking->entry_cap >= min_cap) {
    return true;
  }

  allocator = br__tracking_internals_allocator(tracking);
  new_cap = tracking->entry_cap != 0u ? tracking->entry_cap : 16u;
  while (new_cap < min_cap) {
    if (new_cap > SIZE_MAX / 2u) {
      new_cap = min_cap;
      break;
    }
    new_cap *= 2u;
  }

  resized = br_allocator_resize_uninit(allocator,
                                       tracking->entries,
                                       tracking->entry_cap * sizeof(*tracking->entries),
                                       new_cap * sizeof(*tracking->entries),
                                       (usize) _Alignof(br_tracking_allocator_entry));
  if (resized.status != BR_STATUS_OK) {
    return false;
  }

  tracking->entries = (br_tracking_allocator_entry *)resized.ptr;
  tracking->entry_cap = new_cap;
  return true;
}

static bool br__tracking_reserve_bad_frees(br_tracking_allocator *tracking, usize min_cap) {
  br_allocator allocator;
  br_alloc_result resized;
  usize new_cap;

  if (tracking == NULL) {
    return false;
  }
  if (tracking->bad_free_cap >= min_cap) {
    return true;
  }

  allocator = br__tracking_internals_allocator(tracking);
  new_cap = tracking->bad_free_cap != 0u ? tracking->bad_free_cap : 8u;
  while (new_cap < min_cap) {
    if (new_cap > SIZE_MAX / 2u) {
      new_cap = min_cap;
      break;
    }
    new_cap *= 2u;
  }

  resized = br_allocator_resize_uninit(allocator,
                                       tracking->bad_frees,
                                       tracking->bad_free_cap * sizeof(*tracking->bad_frees),
                                       new_cap * sizeof(*tracking->bad_frees),
                                       (usize) _Alignof(br_tracking_allocator_bad_free));
  if (resized.status != BR_STATUS_OK) {
    return false;
  }

  tracking->bad_frees = (br_tracking_allocator_bad_free *)resized.ptr;
  tracking->bad_free_cap = new_cap;
  return true;
}

static usize br__tracking_find_entry(const br_tracking_allocator *tracking, const void *memory) {
  usize i;

  if (tracking == NULL || memory == NULL) {
    return SIZE_MAX;
  }

  for (i = 0u; i < tracking->entry_count; ++i) {
    if (tracking->entries[i].memory == memory) {
      return i;
    }
  }

  return SIZE_MAX;
}

static void br__tracking_note_bad_free(br_tracking_allocator *tracking, void *memory, usize size) {
  br_tracking_allocator_bad_free bad_free;

  if (tracking == NULL || memory == NULL) {
    return;
  }

  bad_free.memory = memory;
  bad_free.size = size;

  if (tracking->bad_free_fn != NULL) {
    tracking->bad_free_fn(tracking->bad_free_ctx, &bad_free);
    return;
  }

  if (!br__tracking_reserve_bad_frees(tracking, tracking->bad_free_count + 1u)) {
    return;
  }

  tracking->bad_frees[tracking->bad_free_count] = bad_free;
  tracking->bad_free_count += 1u;
}

static void br__tracking_remove_entry(br_tracking_allocator *tracking, usize index) {
  br_tracking_allocator_entry entry;

  if (tracking == NULL || index >= tracking->entry_count) {
    return;
  }

  entry = tracking->entries[index];
  tracking->stats.total_memory_freed += entry.size;
  tracking->stats.total_free_count += 1u;
  tracking->stats.current_memory_allocated -= entry.size;
  tracking->stats.live_allocation_count -= 1u;

  if (index + 1u < tracking->entry_count) {
    memmove(&tracking->entries[index],
            &tracking->entries[index + 1u],
            (tracking->entry_count - index - 1u) * sizeof(*tracking->entries));
  }
  tracking->entry_count -= 1u;
}

static bool br__tracking_add_entry(br_tracking_allocator *tracking,
                                   void *memory,
                                   usize size,
                                   usize alignment,
                                   br_alloc_op op,
                                   br_status status) {
  br_tracking_allocator_entry *entry;

  if (tracking == NULL || memory == NULL || size == 0u) {
    return true;
  }
  if (!br__tracking_reserve_entries(tracking, tracking->entry_count + 1u)) {
    return false;
  }

  entry = &tracking->entries[tracking->entry_count];
  entry->memory = memory;
  entry->size = size;
  entry->alignment = alignment;
  entry->op = op;
  entry->status = status;
  tracking->entry_count += 1u;

  tracking->stats.total_memory_allocated += size;
  tracking->stats.total_allocation_count += 1u;
  tracking->stats.current_memory_allocated += size;
  tracking->stats.live_allocation_count += 1u;
  if (tracking->stats.current_memory_allocated > tracking->stats.peak_memory_allocated) {
    tracking->stats.peak_memory_allocated = tracking->stats.current_memory_allocated;
  }

  return true;
}

static br_alloc_result br__tracking_allocator_fn(void *ctx, const br_alloc_request *req) {
  br_tracking_allocator *tracking = (br_tracking_allocator *)ctx;
  br_alloc_result result;
  usize index;

  if (req == NULL) {
    return br__tracking_alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (tracking == NULL) {
    return br__tracking_alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
    case BR_ALLOC_OP_ALLOC_UNINIT:
      result = br_allocator_call(tracking->backing, req);
      if (result.status != BR_STATUS_OK) {
        return result;
      }
      if (!br__tracking_add_entry(
            tracking, result.ptr, result.size, req->alignment, req->op, result.status)) {
        if (result.ptr != NULL) {
          (void)br_allocator_free(tracking->backing, result.ptr, result.size);
        }
        return br__tracking_alloc_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
      }
      return result;

    case BR_ALLOC_OP_FREE:
      if (req->ptr == NULL) {
        return br__tracking_alloc_result(NULL, 0u, BR_STATUS_OK);
      }

      index = br__tracking_find_entry(tracking, req->ptr);
      if (index == SIZE_MAX) {
        br__tracking_note_bad_free(tracking, req->ptr, req->old_size);
        return br__tracking_alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
      }

      result = br_allocator_call(tracking->backing, req);
      if (result.status != BR_STATUS_OK) {
        return result;
      }

      br__tracking_remove_entry(tracking, index);
      return result;

    case BR_ALLOC_OP_RESET:
      result = br_allocator_call(tracking->backing, req);
      if (result.status != BR_STATUS_OK) {
        return result;
      }
      if (tracking->clear_on_reset) {
        tracking->entry_count = 0u;
        tracking->stats.current_memory_allocated = 0u;
        tracking->stats.live_allocation_count = 0u;
      }
      return result;

    case BR_ALLOC_OP_RESIZE:
    case BR_ALLOC_OP_RESIZE_UNINIT:
      if (req->ptr != NULL) {
        index = br__tracking_find_entry(tracking, req->ptr);
        if (index == SIZE_MAX) {
          br__tracking_note_bad_free(tracking, req->ptr, req->old_size);
          return br__tracking_alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
        }

        if (req->size == 0u) {
          result = br_allocator_call(tracking->backing, req);
          if (result.status == BR_STATUS_OK) {
            br__tracking_remove_entry(tracking, index);
          }
          return result;
        }

        result = br_allocator_call(tracking->backing, req);
        if (result.status != BR_STATUS_OK) {
          return result;
        }

        br__tracking_remove_entry(tracking, index);
        if (!br__tracking_add_entry(
              tracking, result.ptr, result.size, req->alignment, req->op, result.status)) {
          return br__tracking_alloc_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
        }
        return result;
      }

      result = br_allocator_call(tracking->backing, req);
      if (result.status != BR_STATUS_OK) {
        return result;
      }
      if (!br__tracking_add_entry(
            tracking, result.ptr, result.size, req->alignment, req->op, result.status)) {
        if (result.ptr != NULL) {
          (void)br_allocator_free(tracking->backing, result.ptr, result.size);
        }
        return br__tracking_alloc_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
      }
      return result;
  }

  return br__tracking_alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

void br_tracking_allocator_init(br_tracking_allocator *tracking,
                                br_allocator backing,
                                br_allocator internals) {
  if (tracking == NULL) {
    return;
  }

  memset(tracking, 0, sizeof(*tracking));
  tracking->backing = backing;
  tracking->internals = internals.fn != NULL ? internals : br_allocator_heap();
}

void br_tracking_allocator_destroy(br_tracking_allocator *tracking) {
  br_allocator allocator;

  if (tracking == NULL) {
    return;
  }

  allocator = br__tracking_internals_allocator(tracking);
  if (tracking->entries != NULL) {
    (void)br_allocator_free(
      allocator, tracking->entries, tracking->entry_cap * sizeof(*tracking->entries));
  }
  if (tracking->bad_frees != NULL) {
    (void)br_allocator_free(
      allocator, tracking->bad_frees, tracking->bad_free_cap * sizeof(*tracking->bad_frees));
  }

  memset(tracking, 0, sizeof(*tracking));
}

void br_tracking_allocator_clear(br_tracking_allocator *tracking) {
  if (tracking == NULL) {
    return;
  }

  tracking->entry_count = 0u;
  tracking->bad_free_count = 0u;
  tracking->stats.current_memory_allocated = 0u;
  tracking->stats.live_allocation_count = 0u;
}

void br_tracking_allocator_reset(br_tracking_allocator *tracking) {
  if (tracking == NULL) {
    return;
  }

  tracking->entry_count = 0u;
  tracking->bad_free_count = 0u;
  memset(&tracking->stats, 0, sizeof(tracking->stats));
}

br_allocator br_tracking_allocator_allocator(br_tracking_allocator *tracking) {
  br_allocator allocator;

  allocator.fn = br__tracking_allocator_fn;
  allocator.ctx = tracking;
  return allocator;
}
