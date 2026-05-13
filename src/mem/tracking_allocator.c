#include <bedrock/mem/tracking_allocator.h>

typedef enum br__tracking_slot_state {
  BR__TRACKING_SLOT_EMPTY = 0,
  BR__TRACKING_SLOT_FULL,
  BR__TRACKING_SLOT_TOMBSTONE
} br__tracking_slot_state;

typedef struct br__tracking_index_slot {
  void *memory;
  usize entry_index;
  br__tracking_slot_state state;
} br__tracking_index_slot;

struct br_tracking_allocator_internal {
  br__tracking_index_slot *slots;
  usize slot_cap;
  usize slot_count;
  usize tombstone_count;
};

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

static bool br__tracking_ensure_internal(br_tracking_allocator *tracking) {
  br_allocator allocator;
  br_alloc_result allocated;

  if (tracking == NULL) {
    return false;
  }
  if (tracking->internal != NULL) {
    return true;
  }

  allocator = br__tracking_internals_allocator(tracking);
  allocated = br_allocator_alloc(allocator,
                                 sizeof(br_tracking_allocator_internal),
                                 (usize) _Alignof(br_tracking_allocator_internal));
  if (allocated.status != BR_STATUS_OK || allocated.ptr == NULL) {
    return false;
  }

  memset(allocated.ptr, 0, sizeof(br_tracking_allocator_internal));
  tracking->internal = (br_tracking_allocator_internal *)allocated.ptr;
  return true;
}

static usize br__tracking_hash_ptr(const void *memory) {
  uptr x = (uptr)memory;

  x ^= x >> 20u;
  x ^= x >> 12u;
  x ^= x >> 7u;
  x ^= x >> 4u;
  return (usize)x;
}

static usize br__tracking_index_target_cap(usize min_live) {
  usize cap = 32u;

  while (cap / 2u < min_live) {
    if (cap > SIZE_MAX / 2u) {
      return cap;
    }
    cap *= 2u;
  }

  return cap;
}

static usize br__tracking_index_find_slot_in_array(const br__tracking_index_slot *slots,
                                                   usize cap,
                                                   const void *memory) {
  usize mask;
  usize slot_index;

  if (slots == NULL || cap == 0u || memory == NULL) {
    return SIZE_MAX;
  }

  mask = cap - 1u;
  slot_index = br__tracking_hash_ptr(memory) & mask;

  for (;;) {
    const br__tracking_index_slot *slot = &slots[slot_index];

    if (slot->state == BR__TRACKING_SLOT_EMPTY) {
      return SIZE_MAX;
    }
    if (slot->state == BR__TRACKING_SLOT_FULL && slot->memory == memory) {
      return slot_index;
    }

    slot_index = (slot_index + 1u) & mask;
  }
}

static void br__tracking_index_insert_into_array(br__tracking_index_slot *slots,
                                                 usize cap,
                                                 void *memory,
                                                 usize entry_index) {
  usize mask;
  usize slot_index;

  mask = cap - 1u;
  slot_index = br__tracking_hash_ptr(memory) & mask;

  while (slots[slot_index].state == BR__TRACKING_SLOT_FULL) {
    slot_index = (slot_index + 1u) & mask;
  }

  slots[slot_index].memory = memory;
  slots[slot_index].entry_index = entry_index;
  slots[slot_index].state = BR__TRACKING_SLOT_FULL;
}

static bool br__tracking_index_rehash(br_tracking_allocator *tracking, usize min_live) {
  br_tracking_allocator_internal *internal;
  br_allocator allocator;
  br_alloc_result allocated;
  br__tracking_index_slot *new_slots;
  usize old_slot_count;
  usize new_cap;
  usize i;

  if (!br__tracking_ensure_internal(tracking)) {
    return false;
  }

  internal = tracking->internal;
  allocator = br__tracking_internals_allocator(tracking);
  new_cap = br__tracking_index_target_cap(min_live);
  if (internal->slot_cap > new_cap) {
    new_cap = internal->slot_cap;
  }

  allocated = br_allocator_alloc(
    allocator, new_cap * sizeof(*new_slots), (usize) _Alignof(br__tracking_index_slot));
  if (allocated.status != BR_STATUS_OK || allocated.ptr == NULL) {
    return false;
  }

  new_slots = (br__tracking_index_slot *)allocated.ptr;
  old_slot_count = internal->slot_count;
  if (internal->slots != NULL) {
    for (i = 0u; i < internal->slot_cap; ++i) {
      if (internal->slots[i].state == BR__TRACKING_SLOT_FULL) {
        br__tracking_index_insert_into_array(
          new_slots, new_cap, internal->slots[i].memory, internal->slots[i].entry_index);
      }
    }

    (void)br_allocator_free(
      allocator, internal->slots, internal->slot_cap * sizeof(*internal->slots));
  }

  internal->slots = new_slots;
  internal->slot_cap = new_cap;
  internal->slot_count = old_slot_count;
  internal->tombstone_count = 0u;
  return true;
}

static bool br__tracking_index_maybe_grow(br_tracking_allocator *tracking, usize additional_live) {
  br_tracking_allocator_internal *internal;
  usize target_live;

  if (!br__tracking_ensure_internal(tracking)) {
    return false;
  }

  internal = tracking->internal;
  target_live = internal->slot_count + additional_live;
  if (internal->slot_cap == 0u || target_live > internal->slot_cap / 2u ||
      internal->tombstone_count > internal->slot_cap / 4u) {
    return br__tracking_index_rehash(tracking, target_live);
  }

  return true;
}

static bool
br__tracking_index_insert(br_tracking_allocator *tracking, void *memory, usize entry_index) {
  br_tracking_allocator_internal *internal;
  usize mask;
  usize slot_index;
  usize first_tombstone;

  if (tracking == NULL || memory == NULL) {
    return false;
  }
  if (!br__tracking_index_maybe_grow(tracking, 1u)) {
    return false;
  }

  internal = tracking->internal;
  mask = internal->slot_cap - 1u;
  slot_index = br__tracking_hash_ptr(memory) & mask;
  first_tombstone = SIZE_MAX;

  for (;;) {
    br__tracking_index_slot *slot = &internal->slots[slot_index];

    if (slot->state == BR__TRACKING_SLOT_EMPTY) {
      if (first_tombstone != SIZE_MAX) {
        slot = &internal->slots[first_tombstone];
        internal->tombstone_count -= 1u;
      }

      slot->memory = memory;
      slot->entry_index = entry_index;
      slot->state = BR__TRACKING_SLOT_FULL;
      internal->slot_count += 1u;
      return true;
    }
    if (slot->state == BR__TRACKING_SLOT_FULL && slot->memory == memory) {
      slot->entry_index = entry_index;
      return true;
    }
    if (slot->state == BR__TRACKING_SLOT_TOMBSTONE && first_tombstone == SIZE_MAX) {
      first_tombstone = slot_index;
    }

    slot_index = (slot_index + 1u) & mask;
  }
}

static bool br__tracking_index_insert_without_grow(br_tracking_allocator *tracking,
                                                   void *memory,
                                                   usize entry_index) {
  br_tracking_allocator_internal *internal;
  usize mask;
  usize slot_index;
  usize first_tombstone;

  if (tracking == NULL || tracking->internal == NULL || memory == NULL) {
    return false;
  }

  internal = tracking->internal;
  if (internal->slot_cap == 0u) {
    return false;
  }

  mask = internal->slot_cap - 1u;
  slot_index = br__tracking_hash_ptr(memory) & mask;
  first_tombstone = SIZE_MAX;

  for (;;) {
    br__tracking_index_slot *slot = &internal->slots[slot_index];

    if (slot->state == BR__TRACKING_SLOT_EMPTY) {
      if (first_tombstone != SIZE_MAX) {
        slot = &internal->slots[first_tombstone];
        internal->tombstone_count -= 1u;
      }

      slot->memory = memory;
      slot->entry_index = entry_index;
      slot->state = BR__TRACKING_SLOT_FULL;
      internal->slot_count += 1u;
      return true;
    }
    if (slot->state == BR__TRACKING_SLOT_FULL && slot->memory == memory) {
      slot->entry_index = entry_index;
      return true;
    }
    if (slot->state == BR__TRACKING_SLOT_TOMBSTONE && first_tombstone == SIZE_MAX) {
      first_tombstone = slot_index;
    }

    slot_index = (slot_index + 1u) & mask;
  }
}

static bool br__tracking_index_find(const br_tracking_allocator *tracking,
                                    const void *memory,
                                    usize *entry_index_out) {
  const br_tracking_allocator_internal *internal;
  usize slot_index;

  if (tracking == NULL || tracking->internal == NULL || memory == NULL) {
    return false;
  }

  internal = tracking->internal;
  slot_index = br__tracking_index_find_slot_in_array(internal->slots, internal->slot_cap, memory);
  if (slot_index == SIZE_MAX) {
    return false;
  }

  if (entry_index_out != NULL) {
    *entry_index_out = internal->slots[slot_index].entry_index;
  }
  return true;
}

static bool
br__tracking_index_update(br_tracking_allocator *tracking, const void *memory, usize entry_index) {
  br_tracking_allocator_internal *internal;
  usize slot_index;

  if (tracking == NULL || tracking->internal == NULL || memory == NULL) {
    return false;
  }

  internal = tracking->internal;
  slot_index = br__tracking_index_find_slot_in_array(internal->slots, internal->slot_cap, memory);
  if (slot_index == SIZE_MAX) {
    return false;
  }

  internal->slots[slot_index].entry_index = entry_index;
  return true;
}

static bool br__tracking_index_remove(br_tracking_allocator *tracking,
                                      const void *memory,
                                      usize *entry_index_out) {
  br_tracking_allocator_internal *internal;
  usize slot_index;

  if (tracking == NULL || tracking->internal == NULL || memory == NULL) {
    return false;
  }

  internal = tracking->internal;
  slot_index = br__tracking_index_find_slot_in_array(internal->slots, internal->slot_cap, memory);
  if (slot_index == SIZE_MAX) {
    return false;
  }

  if (entry_index_out != NULL) {
    *entry_index_out = internal->slots[slot_index].entry_index;
  }

  internal->slots[slot_index].memory = NULL;
  internal->slots[slot_index].entry_index = 0u;
  internal->slots[slot_index].state = BR__TRACKING_SLOT_TOMBSTONE;
  internal->slot_count -= 1u;
  internal->tombstone_count += 1u;

  if (internal->slot_count == 0u) {
    memset(internal->slots, 0, internal->slot_cap * sizeof(*internal->slots));
    internal->tombstone_count = 0u;
  } else if (internal->tombstone_count > internal->slot_cap / 4u) {
    (void)br__tracking_index_rehash(tracking, internal->slot_count);
  }

  return true;
}

static void br__tracking_index_clear(br_tracking_allocator *tracking) {
  br_tracking_allocator_internal *internal;

  if (tracking == NULL || tracking->internal == NULL) {
    return;
  }

  internal = tracking->internal;
  if (internal->slots != NULL) {
    memset(internal->slots, 0, internal->slot_cap * sizeof(*internal->slots));
  }
  internal->slot_count = 0u;
  internal->tombstone_count = 0u;
}

static void br__tracking_record_alloc(br_tracking_allocator *tracking, usize size) {
  tracking->stats.total_memory_allocated += size;
  tracking->stats.total_allocation_count += 1u;
  tracking->stats.current_memory_allocated += size;
  if (tracking->stats.current_memory_allocated > tracking->stats.peak_memory_allocated) {
    tracking->stats.peak_memory_allocated = tracking->stats.current_memory_allocated;
  }
}

static void br__tracking_record_free(br_tracking_allocator *tracking, usize size) {
  tracking->stats.total_memory_freed += size;
  tracking->stats.total_free_count += 1u;
  tracking->stats.current_memory_allocated -= size;
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

static void br__tracking_clear_unlocked(br_tracking_allocator *tracking) {
  br__tracking_index_clear(tracking);
  tracking->entry_count = 0u;
  tracking->bad_free_count = 0u;
  tracking->stats.current_memory_allocated = 0u;
  tracking->stats.live_allocation_count = 0u;
}

static void br__tracking_reset_unlocked(br_tracking_allocator *tracking) {
  br__tracking_index_clear(tracking);
  tracking->entry_count = 0u;
  tracking->bad_free_count = 0u;
  memset(&tracking->stats, 0, sizeof(tracking->stats));
}

static bool br__tracking_add_entry(br_tracking_allocator *tracking,
                                   void *memory,
                                   usize size,
                                   usize alignment,
                                   br_alloc_op op,
                                   br_status status) {
  br_tracking_allocator_entry *entry;
  usize entry_index;

  if (tracking == NULL || memory == NULL || size == 0u) {
    return true;
  }
  if (!br__tracking_reserve_entries(tracking, tracking->entry_count + 1u)) {
    return false;
  }

  entry_index = tracking->entry_count;
  if (!br__tracking_index_insert(tracking, memory, entry_index)) {
    return false;
  }

  entry = &tracking->entries[entry_index];
  entry->memory = memory;
  entry->size = size;
  entry->alignment = alignment;
  entry->op = op;
  entry->status = status;
  tracking->entry_count += 1u;

  br__tracking_record_alloc(tracking, size);
  tracking->stats.live_allocation_count += 1u;
  return true;
}

static void br__tracking_remove_entry(br_tracking_allocator *tracking, usize entry_index) {
  br_tracking_allocator_entry entry;
  usize last_index;

  if (tracking == NULL || entry_index >= tracking->entry_count) {
    return;
  }

  entry = tracking->entries[entry_index];
  br__tracking_record_free(tracking, entry.size);
  tracking->stats.live_allocation_count -= 1u;

  last_index = tracking->entry_count - 1u;
  if (entry_index != last_index) {
    br_tracking_allocator_entry moved = tracking->entries[last_index];

    tracking->entries[entry_index] = moved;
    /*
    Bedrock keeps the public `entries` array dense for leak inspection, so
    deletions swap-remove here and then retarget the private pointer index.
    */
    (void)br__tracking_index_update(tracking, moved.memory, entry_index);
  }

  tracking->entry_count -= 1u;
}

static void br__tracking_resize_entry(br_tracking_allocator *tracking,
                                      usize entry_index,
                                      void *old_memory,
                                      void *new_memory,
                                      usize new_size,
                                      usize new_alignment,
                                      br_alloc_op op,
                                      br_status status) {
  br_tracking_allocator_entry *entry;
  usize old_size;

  if (tracking == NULL || entry_index >= tracking->entry_count) {
    return;
  }

  entry = &tracking->entries[entry_index];
  old_size = entry->size;

  br__tracking_record_free(tracking, old_size);
  br__tracking_record_alloc(tracking, new_size);

  if (new_memory != old_memory) {
    (void)br__tracking_index_remove(tracking, old_memory, NULL);
    /*
    This is a same-live-count update, so Bedrock reuses the existing table
    capacity instead of risking an allocation failure during reinsertion.
    */
    (void)br__tracking_index_insert_without_grow(tracking, new_memory, entry_index);
  }

  entry->memory = new_memory;
  entry->size = new_size;
  entry->alignment = new_alignment;
  entry->op = op;
  entry->status = status;
}

static br_alloc_result br__tracking_allocator_fn_unlocked(br_tracking_allocator *tracking,
                                                          const br_alloc_request *req) {
  br_alloc_result result;
  usize entry_index;

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
      if (!br__tracking_index_find(tracking, req->ptr, &entry_index)) {
        br__tracking_note_bad_free(tracking, req->ptr, req->old_size);
        return br__tracking_alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
      }

      result = br_allocator_call(tracking->backing, req);
      if (result.status != BR_STATUS_OK) {
        return result;
      }

      (void)br__tracking_index_remove(tracking, req->ptr, NULL);
      br__tracking_remove_entry(tracking, entry_index);
      return result;

    case BR_ALLOC_OP_RESET:
      result = br_allocator_call(tracking->backing, req);
      if (result.status != BR_STATUS_OK) {
        return result;
      }
      if (tracking->clear_on_reset) {
        br__tracking_index_clear(tracking);
        tracking->entry_count = 0u;
        tracking->stats.current_memory_allocated = 0u;
        tracking->stats.live_allocation_count = 0u;
      }
      return result;

    case BR_ALLOC_OP_RESIZE:
    case BR_ALLOC_OP_RESIZE_UNINIT:
      if (req->ptr != NULL) {
        if (!br__tracking_index_find(tracking, req->ptr, &entry_index)) {
          br__tracking_note_bad_free(tracking, req->ptr, req->old_size);
          return br__tracking_alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
        }

        if (req->size == 0u) {
          result = br_allocator_call(tracking->backing, req);
          if (result.status == BR_STATUS_OK) {
            (void)br__tracking_index_remove(tracking, req->ptr, NULL);
            br__tracking_remove_entry(tracking, entry_index);
          }
          return result;
        }

        result = br_allocator_call(tracking->backing, req);
        if (result.status != BR_STATUS_OK) {
          return result;
        }

        /*
        Odin updates a map entry in place here. Bedrock keeps a dense public
        entry list for inspection plus a private pointer index for fast lookup.
        */
        br__tracking_resize_entry(tracking,
                                  entry_index,
                                  req->ptr,
                                  result.ptr,
                                  result.size,
                                  req->alignment,
                                  req->op,
                                  result.status);
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

static br_alloc_result br__tracking_allocator_fn(void *ctx, const br_alloc_request *req) {
  br_tracking_allocator *tracking = (br_tracking_allocator *)ctx;
  br_alloc_result result;

  if (req == NULL) {
    return br__tracking_alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (tracking == NULL) {
    return br__tracking_alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  br_mutex_lock(&tracking->mutex);
  result = br__tracking_allocator_fn_unlocked(tracking, req);
  br_mutex_unlock(&tracking->mutex);
  return result;
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
  tracking->mutex = (br_mutex)BR_MUTEX_INIT;
}

void br_tracking_allocator_destroy(br_tracking_allocator *tracking) {
  br_allocator allocator;

  if (tracking == NULL) {
    return;
  }

  br_mutex_lock(&tracking->mutex);
  allocator = br__tracking_internals_allocator(tracking);
  if (tracking->entries != NULL) {
    (void)br_allocator_free(
      allocator, tracking->entries, tracking->entry_cap * sizeof(*tracking->entries));
  }
  if (tracking->bad_frees != NULL) {
    (void)br_allocator_free(
      allocator, tracking->bad_frees, tracking->bad_free_cap * sizeof(*tracking->bad_frees));
  }
  if (tracking->internal != NULL) {
    if (tracking->internal->slots != NULL) {
      (void)br_allocator_free(allocator,
                              tracking->internal->slots,
                              tracking->internal->slot_cap * sizeof(*tracking->internal->slots));
    }
    (void)br_allocator_free(allocator, tracking->internal, sizeof(*tracking->internal));
  }
  br_mutex_unlock(&tracking->mutex);

  memset(tracking, 0, sizeof(*tracking));
}

void br_tracking_allocator_clear(br_tracking_allocator *tracking) {
  if (tracking == NULL) {
    return;
  }

  br_mutex_lock(&tracking->mutex);
  br__tracking_clear_unlocked(tracking);
  br_mutex_unlock(&tracking->mutex);
}

void br_tracking_allocator_reset(br_tracking_allocator *tracking) {
  if (tracking == NULL) {
    return;
  }

  br_mutex_lock(&tracking->mutex);
  br__tracking_reset_unlocked(tracking);
  br_mutex_unlock(&tracking->mutex);
}

br_allocator br_tracking_allocator_allocator(br_tracking_allocator *tracking) {
  br_allocator allocator;

  allocator.fn = br__tracking_allocator_fn;
  allocator.ctx = tracking;
  return allocator;
}
