#ifndef BEDROCK_PATH_SLASHPATH_H
#define BEDROCK_PATH_SLASHPATH_H

#include <bedrock/strings/strings.h>

BR_EXTERN_C_BEGIN

/*
Pure slash-separated path logic, ported from Odin `core/path/slashpath` (itself
modeled on Go's `path`). This is lexical only: the separator is always '/',
there are no volume letters or backslashes, and nothing touches the filesystem.
OS-specific paths are a future `filepath` module and own the `br_path_` name;
this module is `br_slashpath_`.
*/

typedef struct br_slashpath_split_result {
  br_string_view dir;  /* everything up to and including the last '/' */
  br_string_view file; /* everything after the last '/' */
} br_slashpath_split_result;

typedef struct br_match_result {
  bool matched;
  br_status status; /* OK, or INVALID_ARGUMENT on a malformed pattern */
} br_match_result;

/*
Report whether `c` is the path separator ('/').
*/
static inline bool br_slashpath_is_separator(char c) {
  return c == '/';
}

/*
Report whether `path` is absolute (begins with '/').
*/
static inline bool br_slashpath_is_abs(br_string_view path) {
  return path.len > 0u && path.data[0] == '/';
}

/*
Split `path` immediately after the final '/', into a directory and file name.

If there is no '/', `dir` is empty and `file` is the whole path. The results
have the property `path == dir + file`; the final '/' stays attached to `dir`.
Both fields are views into `path`; no allocation occurs.
*/
br_slashpath_split_result br_slashpath_split(br_string_view path);

/*
Return the last element of `path`.

Trailing slashes are removed. Returns "." if `path` is empty, and "/" if `path`
is entirely slashes. The result is a view into `path` or a static literal.
*/
br_string_view br_slashpath_base(br_string_view path);

/*
Return the file-name extension of `path`, including the leading '.'.

The extension is the suffix from the final '.' in the last '/'-separated
element. Returns an empty view when there is no '.'. The result is a view into
`path`.
*/
br_string_view br_slashpath_ext(br_string_view path);

/*
Return the last element of `path` without its extension.

The result is a view into `path`.
*/
br_string_view br_slashpath_name(br_string_view path);

/*
Return the shortest path equivalent to `path` by lexical processing.

Applies these rules until no change is possible:
  1. replace runs of '/' with a single '/';
  2. drop each '.' path element;
  3. drop each inner '..' element and the non-'..' element before it;
  4. drop '..' that begin a rooted path ("/.." becomes "/").
Returns "." for the empty path or any path that reduces to nothing.

Aliases the input when it is already clean: on an unchanged result `allocated`
is false and `value` views the input (or a static "." / "/" literal for the
degenerate cases), so no allocation happens. When cleaning changes the path the
result is owned; free either case with `br_string_rewrite_free`.
*/
br_string_rewrite_result br_slashpath_clean(br_string_view path, br_allocator allocator);

/*
Return all but the last element of `path`, cleaned.

Equivalent to cleaning the directory half of `br_slashpath_split`. Follows the
same aliasing contract as `br_slashpath_clean`; free with
`br_string_rewrite_free`.
*/
br_string_rewrite_result br_slashpath_dir(br_string_view path, br_allocator allocator);

/*
Join `elems` with '/' separators and clean the result.

Empty elements are ignored. The result is always owned (free with
`br_string_free`); an empty or all-empty input yields an empty string.
*/
br_string_result
br_slashpath_join(const br_string_view *elems, size_t elem_count, br_allocator allocator);

/*
Split `path` into its '/'-separated elements.

A thin wrapper over `br_string_split` on "/"; the elements are views into
`path`. Free the returned list with `br_string_view_list_free`.
*/
br_string_view_list_result br_slashpath_split_elements(br_string_view path, br_allocator allocator);

/*
Report whether `pattern` matches the whole of `name` (not a substring).

Pattern grammar:
  '*'           matches any run of non-'/' characters
  '?'           matches any single non-'/' character
  '[' ['^'] { c | lo '-' hi } ']'   character class (may be negated; non-empty)
  '\\' c        matches the literal character c
  c             matches the literal character c

Matching is rune-aware. A malformed pattern (an unterminated '\\' escape or '['
class) yields `status == BR_STATUS_INVALID_ARGUMENT` with `matched == false`.
*/
br_match_result br_slashpath_match(br_string_view pattern, br_string_view name);

BR_EXTERN_C_END

#endif
