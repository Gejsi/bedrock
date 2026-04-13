#ifndef BR_VEC_T
#error "BR_VEC_T must be defined before including <bedrock/template/vec.h>"
#endif

#ifndef BR_VEC_NAME
#error "BR_VEC_NAME must be defined before including <bedrock/template/vec.h>"
#endif

#ifndef BR_VEC_API
#define BR_VEC_API static inline
#define BR__VEC_UNDEF_API 1
#endif

typedef struct BR_VEC_NAME {
    BR_VEC_T *data;
    usize len;
    usize cap;
    br_allocator allocator;
} BR_VEC_NAME;

BR_VEC_API void BR_CONCAT(BR_VEC_NAME, _init)(BR_VEC_NAME *vec, br_allocator allocator) {
    if (vec == NULL) {
        return;
    }

    vec->data = NULL;
    vec->len = 0u;
    vec->cap = 0u;
    vec->allocator = allocator;
}

BR_VEC_API void BR_CONCAT(BR_VEC_NAME, _destroy)(BR_VEC_NAME *vec) {
    if (vec == NULL) {
        return;
    }

    if (vec->data != NULL) {
        (void)br_allocator_free(vec->allocator, vec->data, vec->cap * sizeof(BR_VEC_T));
    }

    vec->data = NULL;
    vec->len = 0u;
    vec->cap = 0u;
}

BR_VEC_API br_status BR_CONCAT(BR_VEC_NAME, _reserve)(BR_VEC_NAME *vec, usize new_cap) {
    br_alloc_result res;
    usize new_size;

    if (vec == NULL) {
        return BR_STATUS_INVALID_ARGUMENT;
    }

    if (new_cap <= vec->cap) {
        return BR_STATUS_OK;
    }

    if (new_cap > (SIZE_MAX / sizeof(BR_VEC_T))) {
        return BR_STATUS_INVALID_ARGUMENT;
    }

    new_size = new_cap * sizeof(BR_VEC_T);

    if (vec->data == NULL) {
        res = br_allocator_alloc_uninit(vec->allocator, new_size, _Alignof(BR_VEC_T));
    } else {
        res = br_allocator_resize_uninit(
            vec->allocator, vec->data, vec->cap * sizeof(BR_VEC_T), new_size, _Alignof(BR_VEC_T));
    }

    if (res.status != BR_STATUS_OK) {
        return res.status;
    }

    vec->data = (BR_VEC_T *)res.ptr;
    vec->cap = new_cap;
    return BR_STATUS_OK;
}

BR_VEC_API br_status BR_CONCAT(BR_VEC_NAME, _push)(BR_VEC_NAME *vec, BR_VEC_T value) {
    br_status status;
    usize new_cap;

    if (vec == NULL) {
        return BR_STATUS_INVALID_ARGUMENT;
    }

    if (vec->len == vec->cap) {
        if (vec->cap == 0u) {
            new_cap = 8u;
        } else if (vec->cap > (SIZE_MAX / 2u)) {
            return BR_STATUS_OUT_OF_MEMORY;
        } else {
            new_cap = vec->cap * 2u;
        }

        status = BR_CONCAT(BR_VEC_NAME, _reserve)(vec, new_cap);
        if (status != BR_STATUS_OK) {
            return status;
        }
    }

    vec->data[vec->len] = value;
    vec->len += 1u;
    return BR_STATUS_OK;
}

BR_VEC_API br_status BR_CONCAT(BR_VEC_NAME, _pop)(BR_VEC_NAME *vec, BR_VEC_T *out_value) {
    if (vec == NULL || vec->len == 0u) {
        return BR_STATUS_INVALID_ARGUMENT;
    }

    vec->len -= 1u;
    if (out_value != NULL) {
        *out_value = vec->data[vec->len];
    }

    return BR_STATUS_OK;
}

#ifdef BR__VEC_UNDEF_API
#undef BR_VEC_API
#undef BR__VEC_UNDEF_API
#endif

#undef BR_VEC_T
#undef BR_VEC_NAME
