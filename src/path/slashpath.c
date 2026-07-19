#include <bedrock/path/slashpath.h>

#include <bedrock/unicode/utf8.h>

/*
Static results for the degenerate clean/base outputs. Returning views onto these
lets clean("") and clean("/..") stay allocation-free (allocated=false).
*/
static const char BR__SLASHPATH_DOT[] = ".";
static const char BR__SLASHPATH_SLASH[] = "/";

static br_string_view br__slashpath_view(const char *data, usize len) {
  return br_string_view_make(data, len);
}

static br_string_rewrite_result br__slashpath_alias(br_string_view value) {
  br_string_rewrite_result result;

  result.value = value;
  result.owned = br_string_make(NULL, 0u);
  result.allocated = false;
  result.status = BR_STATUS_OK;
  return result;
}

static br_string_rewrite_result br__slashpath_owned(void *data, usize len, br_status status) {
  br_string_rewrite_result result;

  result.owned = br_string_make(data, len);
  result.value = br_string_view_from_string(result.owned);
  result.allocated = status == BR_STATUS_OK;
  result.status = status;
  return result;
}

static br_string_result br__slashpath_string_result(void *data, usize len, br_status status) {
  br_string_result result;

  result.value = br_string_make(data, len);
  result.status = status;
  return result;
}

static bool br__slashpath_is_sep(char c) {
  return c == '/';
}

/*
Lazy path buffer mirroring Odin's Lazy_Buffer. It writes into the original input
as long as the output byte matches the input byte at the same index; the first
divergence allocates a working buffer of the input length and copies the matched
prefix. `w` is the write index; `err` records an allocation failure so the caller
can bail without a separate error channel.
*/
typedef struct br__slashpath_lazy {
  br_string_view s;
  char *b;
  usize w;
  br_allocator allocator;
  bool err;
} br__slashpath_lazy;

static char br__slashpath_lazy_index(const br__slashpath_lazy *lb, usize i) {
  if (lb->b != NULL) {
    return lb->b[i];
  }
  return lb->s.data[i];
}

static void br__slashpath_lazy_append(br__slashpath_lazy *lb, char c) {
  if (lb->err) {
    return;
  }

  if (lb->b == NULL) {
    if (lb->w < lb->s.len && lb->s.data[lb->w] == c) {
      lb->w += 1u;
      return;
    }

    if (lb->s.len == 0u) {
      /* Nothing to alias; force allocation of at least one byte below. */
      lb->err = true;
      return;
    }

    br_alloc_result alloc = br_allocator_alloc_uninit(lb->allocator, lb->s.len, 1u);
    if (alloc.status != BR_STATUS_OK) {
      lb->err = true;
      return;
    }
    lb->b = (char *)alloc.ptr;
    if (lb->w > 0u) {
      memcpy(lb->b, lb->s.data, lb->w);
    }
  }

  lb->b[lb->w] = c;
  lb->w += 1u;
}

/*
Materialize the lazy buffer into a rewrite result. When nothing diverged the
result aliases the untouched input prefix (allocated=false); otherwise it hands
back the owned working buffer.
*/
static br_string_rewrite_result br__slashpath_lazy_result(br__slashpath_lazy *lb) {
  if (lb->err) {
    if (lb->b != NULL) {
      (void)br_allocator_free(lb->allocator, lb->b, lb->s.len);
      lb->b = NULL;
    }
    return br__slashpath_owned(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }
  if (lb->b == NULL) {
    return br__slashpath_alias(br__slashpath_view(lb->s.data, lb->w));
  }
  return br__slashpath_owned(lb->b, lb->w, BR_STATUS_OK);
}

br_slashpath_split_result br_slashpath_split(br_string_view path) {
  br_slashpath_split_result result;
  ptrdiff_t i = br_string_last_index_byte(path, (uint8_t)'/');
  usize cut = i < 0 ? 0u : (usize)i + 1u;

  result.dir = br__slashpath_view(path.data, cut);
  result.file = br__slashpath_view(path.data + cut, path.len - cut);
  return result;
}

br_string_view br_slashpath_base(br_string_view path) {
  usize end;
  ptrdiff_t slash;

  if (path.len == 0u) {
    return br__slashpath_view(BR__SLASHPATH_DOT, 1u);
  }

  end = path.len;
  while (end > 0u && br__slashpath_is_sep(path.data[end - 1u])) {
    end -= 1u;
  }
  if (end == 0u) {
    /* Path was entirely slashes. */
    return br__slashpath_view(BR__SLASHPATH_SLASH, 1u);
  }

  slash = br_string_last_index_byte(br__slashpath_view(path.data, end), (uint8_t)'/');
  if (slash >= 0) {
    return br__slashpath_view(path.data + (usize)slash + 1u, end - ((usize)slash + 1u));
  }
  return br__slashpath_view(path.data, end);
}

br_string_view br_slashpath_ext(br_string_view path) {
  usize i = path.len;

  while (i > 0u && !br__slashpath_is_sep(path.data[i - 1u])) {
    i -= 1u;
    if (path.data[i] == '.') {
      return br__slashpath_view(path.data + i, path.len - i);
    }
  }
  return br__slashpath_view(path.data, 0u);
}

br_string_view br_slashpath_name(br_string_view path) {
  br_string_view file = br_slashpath_split(path).file;
  usize i = file.len;

  while (i > 0u && !br__slashpath_is_sep(file.data[i - 1u])) {
    i -= 1u;
    if (file.data[i] == '.') {
      return br__slashpath_view(file.data, i);
    }
  }
  return file;
}

br_string_rewrite_result br_slashpath_clean(br_string_view path, br_allocator allocator) {
  br__slashpath_lazy out;
  bool rooted;
  usize n = path.len;
  usize r = 0u;
  usize dot_dot = 0u;

  if (path.len == 0u) {
    return br__slashpath_alias(br__slashpath_view(BR__SLASHPATH_DOT, 1u));
  }

  out.s = path;
  out.b = NULL;
  out.w = 0u;
  out.allocator = allocator;
  out.err = false;

  rooted = path.data[0] == '/';
  if (rooted) {
    br__slashpath_lazy_append(&out, '/');
    r = 1u;
    dot_dot = 1u;
  }

  while (r < n) {
    if (br__slashpath_is_sep(path.data[r])) {
      /* empty path element: collapse the slash */
      r += 1u;
    } else if (path.data[r] == '.' && (r + 1u == n || br__slashpath_is_sep(path.data[r + 1u]))) {
      /* . element */
      r += 1u;
    } else if (path.data[r] == '.' && path.data[r + 1u] == '.' &&
               (r + 2u == n || br__slashpath_is_sep(path.data[r + 2u]))) {
      /* .. element: remove the last written element */
      r += 2u;
      if (out.w > dot_dot) {
        out.w -= 1u;
        while (out.w > dot_dot && !br__slashpath_is_sep(br__slashpath_lazy_index(&out, out.w))) {
          out.w -= 1u;
        }
      } else if (!rooted) {
        if (out.w > 0u) {
          br__slashpath_lazy_append(&out, '/');
        }
        br__slashpath_lazy_append(&out, '.');
        br__slashpath_lazy_append(&out, '.');
        dot_dot = out.w;
      }
    } else {
      /* real path element: add separator if needed, then copy it */
      if ((rooted && out.w != 1u) || (!rooted && out.w != 0u)) {
        br__slashpath_lazy_append(&out, '/');
      }
      for (; r < n && !br__slashpath_is_sep(path.data[r]); r += 1u) {
        br__slashpath_lazy_append(&out, path.data[r]);
      }
    }
  }

  if (out.err) {
    return br__slashpath_lazy_result(&out);
  }

  if (out.w == 0u) {
    if (out.b != NULL) {
      (void)br_allocator_free(allocator, out.b, out.s.len);
    }
    return br__slashpath_alias(br__slashpath_view(BR__SLASHPATH_DOT, 1u));
  }

  return br__slashpath_lazy_result(&out);
}

br_string_rewrite_result br_slashpath_dir(br_string_view path, br_allocator allocator) {
  br_slashpath_split_result split = br_slashpath_split(path);
  return br_slashpath_clean(split.dir, allocator);
}

br_string_result
br_slashpath_join(const br_string_view *elems, size_t elem_count, br_allocator allocator) {
  size_t i;
  size_t first;
  bool any = false;
  usize total = 0u;
  char *buf;
  usize pos = 0u;
  br_string_view joined;
  br_string_rewrite_result cleaned;
  br_string_result out;
  br_alloc_result alloc;

  /* Find the first non-empty element; matches Odin's join skipping leading
     empties before joining the rest with '/'. */
  for (first = 0u; first < elem_count; first += 1u) {
    if (elems[first].len > 0u) {
      any = true;
      break;
    }
  }
  if (!any) {
    return br__slashpath_string_result(NULL, 0u, BR_STATUS_OK);
  }

  /* First pass: total length of elems[first:] joined by single '/'. Empty
     interior elements contribute nothing but are still separated correctly
     because we only add a separator before a non-empty element. */
  for (i = first; i < elem_count; i += 1u) {
    if (elems[i].len == 0u) {
      continue;
    }
    if (total > 0u) {
      total += 1u; /* separator */
    }
    total += elems[i].len;
  }

  alloc = br_allocator_alloc_uninit(allocator, total, 1u);
  if (alloc.status != BR_STATUS_OK) {
    return br__slashpath_string_result(NULL, 0u, alloc.status);
  }
  buf = (char *)alloc.ptr;

  for (i = first; i < elem_count; i += 1u) {
    if (elems[i].len == 0u) {
      continue;
    }
    if (pos > 0u) {
      buf[pos] = '/';
      pos += 1u;
    }
    memcpy(buf + pos, elems[i].data, elems[i].len);
    pos += elems[i].len;
  }

  joined = br__slashpath_view(buf, total);
  cleaned = br_slashpath_clean(joined, allocator);
  if (cleaned.status != BR_STATUS_OK) {
    (void)br_allocator_free(allocator, buf, total);
    return br__slashpath_string_result(NULL, 0u, cleaned.status);
  }

  if (cleaned.allocated) {
    /* clean allocated a fresh owned buffer; hand it back and drop ours. */
    (void)br_allocator_free(allocator, buf, total);
    out = br__slashpath_string_result(cleaned.owned.data, cleaned.owned.len, BR_STATUS_OK);
  } else if (cleaned.value.len == total) {
    /* clean aliased the whole buffer unchanged; return it as-is (exact size). */
    out = br__slashpath_string_result(buf, total, BR_STATUS_OK);
  } else {
    /* clean aliased a shorter PREFIX of our buffer (e.g. trailing-slash
       removal). Returning `buf` with the shorter length would leave the freed
       size disagreeing with the allocation, so clone to an exactly-sized buffer
       and free the working one. */
    br_string_result clone = br_string_clone(cleaned.value, allocator);
    (void)br_allocator_free(allocator, buf, total);
    out = clone;
  }
  return out;
}

br_string_view_list_result br_slashpath_split_elements(br_string_view path,
                                                       br_allocator allocator) {
  return br_string_split(path, br__slashpath_view(BR__SLASHPATH_SLASH, 1u), allocator);
}

/*
Glob matcher ported from Odin `core/path/slashpath` match.odin. Operates on
byte cursors into `pattern`/`name`; single-character reads are rune-aware via
br_utf8_decode. Whole-name match. Returns Syntax_Error as
BR_STATUS_INVALID_ARGUMENT.
*/

static br_match_result br__slashpath_match_result(bool matched, br_status status) {
  br_match_result result;

  result.matched = matched;
  result.status = status;
  return result;
}

static bool br__slashpath_contains_slash(br_string_view s) {
  return br_string_index_byte(s, (uint8_t)'/') >= 0;
}

/*
Scan a leading run of '*' then a literal chunk up to the next '*' outside a
'[...]' class. Mirrors Odin's scan_chunk.
*/
static void br__slashpath_scan_chunk(br_string_view pattern,
                                     bool *out_star,
                                     br_string_view *out_chunk,
                                     br_string_view *out_rest) {
  usize i = 0u;
  bool in_range = false;
  bool star = false;

  while (i < pattern.len && pattern.data[i] == '*') {
    i += 1u;
    star = true;
  }
  pattern = br__slashpath_view(pattern.data + i, pattern.len - i);

  for (i = 0u; i < pattern.len; i += 1u) {
    char c = pattern.data[i];
    if (c == '\\') {
      if (i + 1u < pattern.len) {
        i += 1u;
      }
    } else if (c == '[') {
      in_range = true;
    } else if (c == ']') {
      in_range = false;
    } else if (c == '*') {
      if (!in_range) {
        break;
      }
    }
  }

  *out_star = star;
  *out_chunk = br__slashpath_view(pattern.data, i);
  *out_rest = br__slashpath_view(pattern.data + i, pattern.len - i);
}

/*
Read one class member, honoring a leading '\\' escape. Mirrors Odin's
get_escape. `*chunk` is advanced past the consumed bytes. On a malformed class
(empty, or terminated at '-'/']', or a dangling escape) sets *err true.
*/
static br_rune br__slashpath_get_escape(br_string_view *chunk, bool *err) {
  br_utf8_decode_result decoded;

  if (chunk->len == 0u || chunk->data[0] == '-' || chunk->data[0] == ']') {
    *err = true;
    return 0;
  }
  if (chunk->data[0] == '\\') {
    *chunk = br__slashpath_view(chunk->data + 1u, chunk->len - 1u);
    if (chunk->len == 0u) {
      *err = true;
      return 0;
    }
  }

  decoded = br_utf8_decode(br_string_view_as_bytes(*chunk));
  if (decoded.value == BR_RUNE_ERROR && decoded.width == 1u) {
    *err = true;
  }

  *chunk = br__slashpath_view(chunk->data + decoded.width, chunk->len - decoded.width);
  if (chunk->len == 0u) {
    *err = true;
  }
  return decoded.value;
}

/*
Try to match `chunk` at the front of `s`. On success returns the unmatched
remainder of `s` in *rest and sets *ok. On a pattern syntax error sets *err.

Follows Go's matchChunk rather than Odin's: a `failed` flag lets the loop keep
validating the rest of the chunk after the match has failed (including when `s`
is exhausted) without reading past `s`. This is how Go surfaces a malformed
class/escape even when the name ran out first; Odin's port drops this and so
under-reports syntax errors (see done-report / suspected-bug note).
*/
static void br__slashpath_match_chunk(
  br_string_view chunk, br_string_view s, br_string_view *rest, bool *ok, bool *err) {
  bool failed = false;

  *ok = false;
  *err = false;

  while (chunk.len > 0u) {
    if (!failed && s.len == 0u) {
      failed = true;
    }
    switch (chunk.data[0]) {
      case '[': {
        br_rune r = 0;
        bool is_negated = false;
        bool matched = false;
        usize range_count = 0u;

        if (!failed) {
          br_utf8_decode_result decoded = br_utf8_decode(br_string_view_as_bytes(s));
          r = decoded.value;
          s = br__slashpath_view(s.data + decoded.width, s.len - decoded.width);
        }
        chunk = br__slashpath_view(chunk.data + 1u, chunk.len - 1u);
        if (chunk.len > 0u && chunk.data[0] == '^') {
          is_negated = true;
          chunk = br__slashpath_view(chunk.data + 1u, chunk.len - 1u);
        }

        for (;;) {
          br_rune lo;
          br_rune hi;

          if (chunk.len > 0u && chunk.data[0] == ']' && range_count > 0u) {
            chunk = br__slashpath_view(chunk.data + 1u, chunk.len - 1u);
            break;
          }
          lo = br__slashpath_get_escape(&chunk, err);
          if (*err) {
            return;
          }
          hi = lo;
          /* Bounds guard: Go/Odin read chunk[0] here; guarding the length keeps
             a pattern ending in "lo-" from reading past the class. getEsc has
             already errored on the exhausted case, so this only hardens against
             OOB and never changes a valid result. */
          if (chunk.len > 0u && chunk.data[0] == '-') {
            br_string_view after = br__slashpath_view(chunk.data + 1u, chunk.len - 1u);
            hi = br__slashpath_get_escape(&after, err);
            if (*err) {
              return;
            }
            chunk = after;
          }
          if (lo <= r && r <= hi) {
            matched = true;
          }
          range_count += 1u;
        }
        if (matched == is_negated) {
          failed = true;
        }
        break;
      }
      case '?':
        if (!failed) {
          br_utf8_decode_result decoded;
          if (s.data[0] == '/') {
            failed = true;
          } else {
            decoded = br_utf8_decode(br_string_view_as_bytes(s));
            s = br__slashpath_view(s.data + decoded.width, s.len - decoded.width);
          }
        }
        chunk = br__slashpath_view(chunk.data + 1u, chunk.len - 1u);
        break;

      case '\\':
        chunk = br__slashpath_view(chunk.data + 1u, chunk.len - 1u);
        if (chunk.len == 0u) {
          *err = true;
          return;
        }
        /* FALLTHROUGH */
      default:
        if (!failed) {
          if (chunk.data[0] != s.data[0]) {
            failed = true;
          } else {
            s = br__slashpath_view(s.data + 1u, s.len - 1u);
          }
        }
        chunk = br__slashpath_view(chunk.data + 1u, chunk.len - 1u);
        break;
    }
  }

  if (failed) {
    return;
  }
  *rest = s;
  *ok = true;
}

br_match_result br_slashpath_match(br_string_view pattern, br_string_view name) {
  while (pattern.len > 0u) {
    bool star;
    br_string_view chunk;
    br_string_view rest;
    br_string_view t;
    bool ok;
    bool err;

    br__slashpath_scan_chunk(pattern, &star, &chunk, &rest);
    pattern = rest;

    if (star && chunk.len == 0u) {
      /* Trailing '*' matches the rest of the element (no '/'). */
      return br__slashpath_match_result(!br__slashpath_contains_slash(name), BR_STATUS_OK);
    }

    br__slashpath_match_chunk(chunk, name, &t, &ok, &err);
    if (ok && (t.len == 0u || pattern.len > 0u)) {
      name = t;
      continue;
    }
    if (err) {
      return br__slashpath_match_result(false, BR_STATUS_INVALID_ARGUMENT);
    }
    if (star) {
      usize i = 0u;
      bool advanced = false;
      for (i = 0u; i < name.len && name.data[i] != '/'; i += 1u) {
        br_string_view sub = br__slashpath_view(name.data + i + 1u, name.len - (i + 1u));
        br__slashpath_match_chunk(chunk, sub, &t, &ok, &err);
        if (ok) {
          if (pattern.len == 0u && t.len > 0u) {
            continue;
          }
          name = t;
          advanced = true;
          break;
        }
        if (err) {
          return br__slashpath_match_result(false, BR_STATUS_INVALID_ARGUMENT);
        }
      }
      if (advanced) {
        continue;
      }
    }

    /* Before returning a plain no-match, validate the rest of the pattern:
       Go surfaces a malformed class/escape here even when the name matched too
       little to reach it. Scan remaining chunks against an empty name. */
    while (pattern.len > 0u) {
      bool star2;
      br_string_view chunk2;
      br_string_view rest2;
      br_string_view t2;
      bool ok2;
      bool err2;

      br__slashpath_scan_chunk(pattern, &star2, &chunk2, &rest2);
      pattern = rest2;
      br__slashpath_match_chunk(chunk2, br__slashpath_view(NULL, 0u), &t2, &ok2, &err2);
      if (err2) {
        return br__slashpath_match_result(false, BR_STATUS_INVALID_ARGUMENT);
      }
    }

    return br__slashpath_match_result(false, BR_STATUS_OK);
  }

  return br__slashpath_match_result(name.len == 0u, BR_STATUS_OK);
}
