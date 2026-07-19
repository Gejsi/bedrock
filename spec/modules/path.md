# Path

Pure slash-separated path logic ported from Odin `core/path/slashpath`
(itself modeled on Go's `path`). This is lexical only: forward-slash separator,
no OS volumes or backslashes, no filesystem access. OS-specific paths are a
future `filepath` module and own the `br_path_` name; this module is
`br_slashpath_`.

## Scope

| Package | Bedrock header | Impl | Status |
| --- | --- | --- | --- |
| path/slashpath | path/slashpath.h | src/path/slashpath.c | planned |

Module umbrella: include/bedrock/path.h.

## Conventions

- Inputs are `br_string_view`; the module never mutates input.
- Predicates and decomposition (`is_separator`, `is_abs`, `split`, `base`,
  `ext`, `name`) are pure and allocation-free: they return views into the input
  or into static `"."` / `"/"` literals.
- `clean`/`dir` return `br_string_rewrite_result` and allocate ONLY when the
  cleaned output diverges from a prefix of the input (improves on Odin's
  Lazy_Buffer, which always clones). Free with `br_string_rewrite_free`.
- `join` always produces owned storage (`br_string_result`, free with
  `br_string_free`); it uses a two-pass length-then-fill strategy with a single
  allocation and no scratch-allocator parameter.
- `match` is pure and returns `br_match_result`.

## Error Model

Only `match` can fail, with `BR_STATUS_INVALID_ARGUMENT` on a malformed pattern
(unterminated `\` or `[`). Rationale: a glob pattern is caller-authored program
text, not encoded runtime data, so it is an argument error rather than
`BR_STATUS_INVALID_ENCODING` (see spec/modules/encoding.md). If a future module
makes runtime-supplied patterns central, add a dedicated status rather than
overloading either code. `clean`/`dir`/`join` can return
`BR_STATUS_OUT_OF_MEMORY` from the allocator.

## Types

```c
typedef struct br_slashpath_split_result {  /* views into input; no alloc */
  br_string_view dir;   /* everything up to and including the last '/' */
  br_string_view file;  /* everything after the last '/' */
} br_slashpath_split_result;

typedef struct br_match_result {
  bool matched;
  br_status status;     /* OK, or INVALID_ARGUMENT on pattern syntax error */
} br_match_result;
```

## API

```c
/* Pure predicates (static inline). */
static inline bool br_slashpath_is_separator(char c);        /* c == '/' */
static inline bool br_slashpath_is_abs(br_string_view path); /* len>0 && [0]=='/' */

/* Pure decomposition — return views into `path` or static "."/"/" literals. */
br_slashpath_split_result br_slashpath_split(br_string_view path);
br_string_view br_slashpath_base(br_string_view path); /* last elem; "." if empty; "/" if all-slash */
br_string_view br_slashpath_ext(br_string_view path);  /* ".ext" incl dot, or empty */
br_string_view br_slashpath_name(br_string_view path); /* base without extension */

/* Lexical clean — aliases input when already clean, else allocates. */
br_string_rewrite_result br_slashpath_clean(br_string_view path, br_allocator allocator);
br_string_rewrite_result br_slashpath_dir(br_string_view path, br_allocator allocator);

/* Join elements with '/', then clean. Always owned; empty input -> empty string. */
br_string_result br_slashpath_join(const br_string_view *elems, size_t elem_count,
                                   br_allocator allocator);

/* Split on '/' into element views (thin wrapper over br_string_split). */
br_string_view_list_result br_slashpath_split_elements(br_string_view path,
                                                       br_allocator allocator);

/* Shell glob match over the whole name (not substring). */
br_match_result br_slashpath_match(br_string_view pattern, br_string_view name);
```

## Semantics and edge cases (from Odin)

- clean rules: collapse multiple '/', drop '.' elements, resolve inner '..',
  and "/.." at root becomes "/". clean("") == ".". A result that reduces to
  empty becomes ".".
- base: trailing slashes stripped; "" -> "."; all-slashes -> "/".
- split: `path == dir + file`; no slash -> dir empty, file == path; the last
  slash stays attached to dir.
- ext: scans back from end within the last element; empty if no '.'.
- name: file component minus its extension.
- match pattern grammar: `*` (any run of non-'/'), `?` (one non-'/'), `[...]`
  and `[^...]` classes with `lo-hi` ranges and `\` escapes; classes must be
  non-empty; must match the ENTIRE name. Unterminated escape/class ->
  INVALID_ARGUMENT.

## Intentional deviations from Odin

- clean/dir avoid Odin's always-clone by aliasing the input via
  `br_string_rewrite_result` when unchanged.
- Named `br_slashpath_` (Odin package `slashpath`); `br_path_` reserved for a
  future OS-path module.
- No implicit context allocator; explicit `br_allocator` on allocating calls.
- match returns a typed status, not Odin's `Match_Error` enum.

## Testing and fuzzing

Go/Odin clean+match vectors; assert clean is idempotent
(clean(clean(p)) == clean(p)) and join(split parts) round-trips a cleaned path.
Fuzz targets:

- clean: random '/', '.', '..' soups; assert idempotent, no OOB, ASan-clean,
  and that allocated=false results are byte-identical prefixes of the input.
- match: random patterns x names; assert no OOB on truncated escapes/classes
  and that syntax errors return INVALID_ARGUMENT (never a spurious match or a
  crash), and that matching always terminates.
