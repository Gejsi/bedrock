#include <bedrock/string_reader.h>

static br_string_reader_io_result br__string_reader_io_result(usize count, br_status status) {
    br_string_reader_io_result result;

    result.count = count;
    result.status = status;
    return result;
}

static br_string_reader_byte_result br__string_reader_byte_result(u8 value, br_status status) {
    br_string_reader_byte_result result;

    result.value = value;
    result.status = status;
    return result;
}

static br_string_reader_rune_result br__string_reader_rune_result(
    br_rune value,
    usize width,
    br_status status
) {
    br_string_reader_rune_result result;

    result.value = value;
    result.width = width;
    result.status = status;
    return result;
}

static br_string_reader_seek_result br__string_reader_seek_result(i64 offset, br_status status) {
    br_string_reader_seek_result result;

    result.offset = offset;
    result.status = status;
    return result;
}

void br_string_reader_init(br_string_reader *reader, br_string_view source) {
    if (reader == NULL) {
        return;
    }

    reader->source = source;
    reader->index = 0;
    reader->prev_rune = -1;
}

void br_string_reader_reset(br_string_reader *reader) {
    if (reader == NULL) {
        return;
    }

    reader->index = 0;
    reader->prev_rune = -1;
}

br_string_view br_string_reader_view(const br_string_reader *reader) {
    usize remaining;

    if (reader == NULL) {
        return br_string_view_make(NULL, 0u);
    }

    remaining = br_string_reader_len(reader);
    if (remaining == 0u) {
        return br_string_view_make(NULL, 0u);
    }

    return br_string_view_make(reader->source.data + (usize)reader->index, remaining);
}

usize br_string_reader_len(const br_string_reader *reader) {
    if (reader == NULL || reader->index >= (i64)reader->source.len) {
        return 0u;
    }
    if (reader->index < 0) {
        return reader->source.len;
    }

    return reader->source.len - (usize)reader->index;
}

usize br_string_reader_size(const br_string_reader *reader) {
    return reader != NULL ? reader->source.len : 0u;
}

br_string_reader_io_result br_string_reader_read(br_string_reader *reader, void *dst, usize dst_len) {
    usize count;

    if (reader == NULL || (dst == NULL && dst_len > 0u)) {
        return br__string_reader_io_result(0u, BR_STATUS_INVALID_ARGUMENT);
    }
    if (dst_len == 0u) {
        return br__string_reader_io_result(0u, BR_STATUS_OK);
    }
    if (reader->index >= (i64)reader->source.len) {
        reader->prev_rune = -1;
        return br__string_reader_io_result(0u, BR_STATUS_EOF);
    }

    reader->prev_rune = -1;
    count = br_min_size(dst_len, reader->source.len - (usize)reader->index);
    memcpy(dst, reader->source.data + (usize)reader->index, count);
    reader->index += (i64)count;
    return br__string_reader_io_result(count, BR_STATUS_OK);
}

br_string_reader_io_result br_string_reader_read_at(
    const br_string_reader *reader,
    void *dst,
    usize dst_len,
    i64 offset
) {
    usize count;

    if (reader == NULL || (dst == NULL && dst_len > 0u)) {
        return br__string_reader_io_result(0u, BR_STATUS_INVALID_ARGUMENT);
    }
    if (dst_len == 0u) {
        return br__string_reader_io_result(0u, BR_STATUS_OK);
    }
    if (offset < 0) {
        return br__string_reader_io_result(0u, BR_STATUS_INVALID_ARGUMENT);
    }
    if (offset >= (i64)reader->source.len) {
        return br__string_reader_io_result(0u, BR_STATUS_EOF);
    }

    count = br_min_size(dst_len, reader->source.len - (usize)offset);
    memcpy(dst, reader->source.data + (usize)offset, count);
    if (count < dst_len) {
        return br__string_reader_io_result(count, BR_STATUS_EOF);
    }
    return br__string_reader_io_result(count, BR_STATUS_OK);
}

br_string_reader_byte_result br_string_reader_read_byte(br_string_reader *reader) {
    if (reader == NULL) {
        return br__string_reader_byte_result(0u, BR_STATUS_INVALID_ARGUMENT);
    }

    reader->prev_rune = -1;
    if (reader->index >= (i64)reader->source.len) {
        return br__string_reader_byte_result(0u, BR_STATUS_EOF);
    }

    reader->index += 1;
    return br__string_reader_byte_result((u8)reader->source.data[(usize)reader->index - 1u], BR_STATUS_OK);
}

br_status br_string_reader_unread_byte(br_string_reader *reader) {
    if (reader == NULL) {
        return BR_STATUS_INVALID_ARGUMENT;
    }
    if (reader->index <= 0) {
        return BR_STATUS_INVALID_STATE;
    }

    reader->prev_rune = -1;
    reader->index -= 1;
    return BR_STATUS_OK;
}

br_string_reader_rune_result br_string_reader_read_rune(br_string_reader *reader) {
    br_utf8_decode_result decoded;
    const char *remaining;
    usize remaining_len;

    if (reader == NULL) {
        return br__string_reader_rune_result(0, 0u, BR_STATUS_INVALID_ARGUMENT);
    }
    if (reader->index >= (i64)reader->source.len) {
        reader->prev_rune = -1;
        return br__string_reader_rune_result(0, 0u, BR_STATUS_EOF);
    }

    reader->prev_rune = reader->index;
    remaining = reader->source.data + (usize)reader->index;
    remaining_len = reader->source.len - (usize)reader->index;
    if ((u8)remaining[0] < (u8)BR_RUNE_SELF) {
        reader->index += 1;
        return br__string_reader_rune_result((br_rune)(u8)remaining[0], 1u, BR_STATUS_OK);
    }

    decoded = br_utf8_decode(br_bytes_view_make(remaining, remaining_len));
    reader->index += (i64)decoded.width;
    return br__string_reader_rune_result(decoded.value, decoded.width, BR_STATUS_OK);
}

br_status br_string_reader_unread_rune(br_string_reader *reader) {
    if (reader == NULL) {
        return BR_STATUS_INVALID_ARGUMENT;
    }
    if (reader->index <= 0 || reader->prev_rune < 0) {
        return BR_STATUS_INVALID_STATE;
    }

    reader->index = reader->prev_rune;
    reader->prev_rune = -1;
    return BR_STATUS_OK;
}

br_string_reader_seek_result br_string_reader_seek(
    br_string_reader *reader,
    i64 offset,
    br_seek_from whence
) {
    i64 absolute;

    if (reader == NULL) {
        return br__string_reader_seek_result(0, BR_STATUS_INVALID_ARGUMENT);
    }

    reader->prev_rune = -1;
    switch (whence) {
    case BR_SEEK_FROM_START:
        absolute = offset;
        break;
    case BR_SEEK_FROM_CURRENT:
        absolute = reader->index + offset;
        break;
    case BR_SEEK_FROM_END:
        absolute = (i64)reader->source.len + offset;
        break;
    default:
        return br__string_reader_seek_result(0, BR_STATUS_INVALID_ARGUMENT);
    }

    if (absolute < 0) {
        return br__string_reader_seek_result(0, BR_STATUS_INVALID_ARGUMENT);
    }

    reader->index = absolute;
    return br__string_reader_seek_result(absolute, BR_STATUS_OK);
}
