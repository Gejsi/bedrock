#include <bedrock/byte_buffer.h>

enum {
    BR__BYTE_BUFFER_MIN_CAPACITY = 64
};

static br_byte_buffer_io_result br__byte_buffer_io_result(usize count, br_status status) {
    br_byte_buffer_io_result result;

    result.count = count;
    result.status = status;
    return result;
}

static br_byte_buffer_byte_result br__byte_buffer_byte_result(u8 value, br_status status) {
    br_byte_buffer_byte_result result;

    result.value = value;
    result.status = status;
    return result;
}

static void br__byte_buffer_clear_state(br_byte_buffer *buffer) {
    buffer->len = 0u;
    buffer->off = 0u;
    buffer->can_unread_byte = 0;
}

static usize br__byte_buffer_unread_len(const br_byte_buffer *buffer) {
    return buffer->len - buffer->off;
}

static void br__byte_buffer_compact(br_byte_buffer *buffer) {
    usize unread_len;

    if (buffer == NULL || buffer->off == 0u) {
        return;
    }

    unread_len = br__byte_buffer_unread_len(buffer);
    if (unread_len > 0u) {
        memmove(buffer->data, buffer->data + buffer->off, unread_len);
    }
    buffer->off = 0u;
    buffer->len = unread_len;
}

static br_status br__byte_buffer_grow(br_byte_buffer *buffer, usize additional) {
    br_alloc_result resized;
    usize unread_len;
    usize target_len;
    usize new_cap;

    if (buffer == NULL) {
        return BR_STATUS_INVALID_ARGUMENT;
    }
    if (additional == 0u) {
        return BR_STATUS_OK;
    }

    unread_len = br__byte_buffer_unread_len(buffer);
    if (unread_len == 0u && buffer->off != 0u) {
        br__byte_buffer_clear_state(buffer);
    }

    if (additional <= buffer->cap - buffer->len) {
        return BR_STATUS_OK;
    }

    if (br__byte_buffer_unread_len(buffer) + additional < br__byte_buffer_unread_len(buffer)) {
        return BR_STATUS_OUT_OF_MEMORY;
    }

    unread_len = br__byte_buffer_unread_len(buffer);
    target_len = unread_len + additional;

    if (target_len <= buffer->cap) {
        br__byte_buffer_compact(buffer);
        return BR_STATUS_OK;
    }

    new_cap = buffer->cap;
    if (new_cap < BR__BYTE_BUFFER_MIN_CAPACITY) {
        new_cap = BR__BYTE_BUFFER_MIN_CAPACITY;
    }
    while (new_cap < target_len) {
        usize doubled = new_cap * 2u;
        if (doubled <= new_cap) {
            new_cap = target_len;
            break;
        }
        new_cap = doubled;
    }

    if (buffer->data == NULL) {
        resized = br_allocator_alloc_uninit(buffer->allocator, new_cap, 1u);
        if (resized.status != BR_STATUS_OK) {
            return resized.status;
        }
    } else {
        br__byte_buffer_compact(buffer);
        resized = br_allocator_resize_uninit(
            buffer->allocator,
            buffer->data,
            buffer->cap,
            new_cap,
            1u
        );
        if (resized.status != BR_STATUS_OK) {
            return resized.status;
        }
    }

    buffer->data = resized.ptr;
    buffer->cap = new_cap;
    return BR_STATUS_OK;
}

void br_byte_buffer_init(br_byte_buffer *buffer, br_allocator allocator) {
    if (buffer == NULL) {
        return;
    }

    buffer->data = NULL;
    buffer->cap = 0u;
    buffer->allocator = allocator;
    br__byte_buffer_clear_state(buffer);
}

br_status br_byte_buffer_init_copy(
    br_byte_buffer *buffer,
    br_bytes_view initial_data,
    br_allocator allocator
) {
    br_alloc_result alloc;

    if (buffer == NULL) {
        return BR_STATUS_INVALID_ARGUMENT;
    }

    br_byte_buffer_init(buffer, allocator);
    if (initial_data.len == 0u) {
        return BR_STATUS_OK;
    }

    alloc = br_allocator_alloc_uninit(allocator, initial_data.len, 1u);
    if (alloc.status != BR_STATUS_OK) {
        return alloc.status;
    }

    memcpy(alloc.ptr, initial_data.data, initial_data.len);
    buffer->data = alloc.ptr;
    buffer->len = initial_data.len;
    buffer->cap = initial_data.len;
    return BR_STATUS_OK;
}

void br_byte_buffer_destroy(br_byte_buffer *buffer) {
    if (buffer == NULL) {
        return;
    }

    if (buffer->data != NULL) {
        (void)br_allocator_free(buffer->allocator, buffer->data, buffer->cap);
    }

    buffer->data = NULL;
    buffer->cap = 0u;
    br__byte_buffer_clear_state(buffer);
}

void br_byte_buffer_reset(br_byte_buffer *buffer) {
    if (buffer == NULL) {
        return;
    }

    br__byte_buffer_clear_state(buffer);
}

int br_byte_buffer_is_empty(const br_byte_buffer *buffer) {
    return buffer == NULL || br__byte_buffer_unread_len(buffer) == 0u;
}

usize br_byte_buffer_len(const br_byte_buffer *buffer) {
    if (buffer == NULL) {
        return 0u;
    }

    return br__byte_buffer_unread_len(buffer);
}

usize br_byte_buffer_capacity(const br_byte_buffer *buffer) {
    return buffer != NULL ? buffer->cap : 0u;
}

br_bytes_view br_byte_buffer_view(const br_byte_buffer *buffer) {
    if (buffer == NULL || br__byte_buffer_unread_len(buffer) == 0u) {
        return br_bytes_view_make(NULL, 0u);
    }

    return br_bytes_view_make(buffer->data + buffer->off, br__byte_buffer_unread_len(buffer));
}

br_status br_byte_buffer_reserve(br_byte_buffer *buffer, usize additional) {
    return br__byte_buffer_grow(buffer, additional);
}

br_status br_byte_buffer_truncate(br_byte_buffer *buffer, usize n) {
    if (buffer == NULL) {
        return BR_STATUS_INVALID_ARGUMENT;
    }
    if (n > br__byte_buffer_unread_len(buffer)) {
        return BR_STATUS_INVALID_ARGUMENT;
    }

    buffer->len = buffer->off + n;
    buffer->can_unread_byte = 0;
    if (n == 0u && buffer->off == buffer->len) {
        br__byte_buffer_clear_state(buffer);
    }
    return BR_STATUS_OK;
}

br_byte_buffer_io_result br_byte_buffer_write(br_byte_buffer *buffer, br_bytes_view src) {
    br_status status;

    if (buffer == NULL) {
        return br__byte_buffer_io_result(0u, BR_STATUS_INVALID_ARGUMENT);
    }
    if (src.len == 0u) {
        buffer->can_unread_byte = 0;
        return br__byte_buffer_io_result(0u, BR_STATUS_OK);
    }

    status = br__byte_buffer_grow(buffer, src.len);
    if (status != BR_STATUS_OK) {
        return br__byte_buffer_io_result(0u, status);
    }

    memcpy(buffer->data + buffer->len, src.data, src.len);
    buffer->len += src.len;
    buffer->can_unread_byte = 0;
    return br__byte_buffer_io_result(src.len, BR_STATUS_OK);
}

br_status br_byte_buffer_write_byte(br_byte_buffer *buffer, u8 byte_value) {
    br_status status;

    if (buffer == NULL) {
        return BR_STATUS_INVALID_ARGUMENT;
    }

    status = br__byte_buffer_grow(buffer, 1u);
    if (status != BR_STATUS_OK) {
        return status;
    }

    buffer->data[buffer->len] = byte_value;
    buffer->len += 1u;
    buffer->can_unread_byte = 0;
    return BR_STATUS_OK;
}

br_bytes_view br_byte_buffer_next(br_byte_buffer *buffer, usize n) {
    usize unread_len;
    br_bytes_view result;

    if (buffer == NULL) {
        return br_bytes_view_make(NULL, 0u);
    }

    unread_len = br__byte_buffer_unread_len(buffer);
    if (n > unread_len) {
        n = unread_len;
    }

    result = br_bytes_view_make(buffer->data + buffer->off, n);
    buffer->off += n;
    buffer->can_unread_byte = 0;
    if (buffer->off == buffer->len) {
        br__byte_buffer_clear_state(buffer);
    }
    return result;
}

br_byte_buffer_io_result br_byte_buffer_read(br_byte_buffer *buffer, void *dst, usize dst_len) {
    usize unread_len;
    usize count;

    if (buffer == NULL || (dst == NULL && dst_len > 0u)) {
        return br__byte_buffer_io_result(0u, BR_STATUS_INVALID_ARGUMENT);
    }
    if (dst_len == 0u) {
        return br__byte_buffer_io_result(0u, BR_STATUS_OK);
    }

    unread_len = br__byte_buffer_unread_len(buffer);
    if (unread_len == 0u) {
        br__byte_buffer_clear_state(buffer);
        return br__byte_buffer_io_result(0u, BR_STATUS_EOF);
    }

    count = br_min_size(unread_len, dst_len);
    memcpy(dst, buffer->data + buffer->off, count);
    buffer->off += count;
    buffer->can_unread_byte = count > 0u;
    return br__byte_buffer_io_result(count, BR_STATUS_OK);
}

br_byte_buffer_byte_result br_byte_buffer_read_byte(br_byte_buffer *buffer) {
    u8 byte_value;

    if (buffer == NULL) {
        return br__byte_buffer_byte_result(0u, BR_STATUS_INVALID_ARGUMENT);
    }
    if (br__byte_buffer_unread_len(buffer) == 0u) {
        br__byte_buffer_clear_state(buffer);
        return br__byte_buffer_byte_result(0u, BR_STATUS_EOF);
    }

    byte_value = buffer->data[buffer->off];
    buffer->off += 1u;
    buffer->can_unread_byte = 1;
    if (buffer->off == buffer->len) {
        buffer->off = buffer->len;
    }
    return br__byte_buffer_byte_result(byte_value, BR_STATUS_OK);
}

br_status br_byte_buffer_unread_byte(br_byte_buffer *buffer) {
    if (buffer == NULL) {
        return BR_STATUS_INVALID_ARGUMENT;
    }
    if (!buffer->can_unread_byte || buffer->off == 0u) {
        return BR_STATUS_INVALID_STATE;
    }

    buffer->off -= 1u;
    buffer->can_unread_byte = 0;
    return BR_STATUS_OK;
}
