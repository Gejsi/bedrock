# Print

`printf`-family formatting composed over the strconv leaves: familiar `%`-verbs,
locale-free by construction, with the format string checked by the compiler.
The name is `print` (verb over the leaves) — `fmt` is the excluded reflection
engine, `format` collides with `br_format_*`.

## The safety story (stated plainly — this is the module's whole justification)

`fmt` was excluded because RTTI reflection is a large, runtime-typed,
open-ended surface; `print` is a CLOSED fixed-specifier parser whose one unsafe
coupling is compiler-checked and whose outputs are bounded and write-back-free.
Concretely, the bounded-safety properties:

1. CLOSED specifier set — a fixed, known grammar; no open-ended type dispatch.
2. COMPILER-CHECKED fmt↔args coupling — `BR_PRINTF_LIKE` makes gcc/clang verify
   argument types against the format string (see the attribute + width contract).
3. NO write-back verbs — `%n` is refused (below); nothing writes through a
   vararg pointer.
4. BOUNDED-BY-CONSTRUCTION sinks — every entry point writes to a caller-sized
   buffer or a growing builder; the parser is bounds-checked and cannot walk off
   the format string.
5. NO allocation or ambient state in the buffer path; no globals.
6. DETERMINISTIC output — locale-free leaves (`br_format_*`: always `.`, ASCII
   digits); the same args produce the same bytes on every platform/locale.

The ONE thing C cannot provide: there is NO runtime validation that the varargs
MATCH the format string — C varargs are untyped at runtime. The compiler
attribute is the ONLY backstop, so it must not be defeated (see the width
contract), and a DYNAMIC (non-literal) format string gets NO checking — those
callers must use the builder path with `br_format_*` directly, NOT `print`.
Stated explicitly so nobody passes a runtime-built format string to `br_print_*`
expecting safety.

## The compiler attribute

```c
#if defined(__GNUC__) || defined(__clang__)
/* archetype `printf`, NOT `gnu_printf` — gnu_printf is ignored on Apple clang. */
#define BR_PRINTF_LIKE(fmt_index, args_index) __attribute__((format(printf, fmt_index, args_index)))
#else
#define BR_PRINTF_LIKE(fmt_index, args_index)
#endif

/* MSVC /analyze users get checking via the SAL annotation on the fmt param. */
#if defined(_MSC_VER)
#include <sal.h>
#define BR_PRINTF_FMT _Printf_format_string_
#else
#define BR_PRINTF_FMT
#endif
```

Index rule (state it so porters never re-derive it per entry point): the first
index is the 1-based position of the format-string parameter; the second is the
position of the first vararg (always fmt+1 for these shapes); the second index
is 0 for the `va_list` twins. Checking fires under the existing `-Wall` — no new
build flags. The `BR_PRINTF_FMT` marker precedes the `const char *fmt` parameter.

## THE HARD CONTRACT: parser va_arg widths == what the attribute validates

The make-or-break invariant. `format(printf)` tells the compiler to check each
specifier against a specific type; the runtime parser's `va_arg` MUST read
exactly that type, or the compiler's blessing is a lie (it validates one type,
the parser reads another → UB the attribute "approved"). Accounting for C's
default argument promotions (char/short/float → int/double in varargs), the
binding table:

| Specifier | Attribute-validated type | Parser reads |
| --- | --- | --- |
| `%d` `%i` | int | `va_arg(int)` |
| `%u` `%x` `%X` `%o` | unsigned int | `va_arg(unsigned)` |
| `%ld` `%lu` `%lx` … | long / unsigned long | `va_arg(long)` etc. |
| `%lld` `%llu` … | long long / unsigned long long | `va_arg(long long)` etc. |
| `%zd` `%zu` | ptrdiff_t / size_t | `va_arg(size_t)` |
| `%f` `%e` `%g` | double (float promotes) | `va_arg(double)` — note no distinct `%lf` in varargs |
| `%c` | int (char promotes) | `va_arg(int)`, narrow |
| `%s` | const char* | `va_arg(const char*)` |
| `%.*s` | int, then const char* | `va_arg(int)` precision, `va_arg(const char*)` |
| `%p` | void* | `va_arg(void*)` |
| `%%` | (none) | — |

Changing a specifier's read width without changing what the attribute validates
is a memory-safety bug. Length modifiers `l`/`ll`/`z` are parsed AND honored and
route to the matching `br_format_*` width — a bare `%d` with an `int64_t` arg is
the caller's bug the attribute is meant to CATCH.

## The view idiom (the cstr-grievance fix)

```c
#define BR_SV_ARG(sv) (int)((sv).len), (const char *)((sv).data)
/* br_print_to_builder(b, "path=%.*s port=%d\n", BR_SV_ARG(path), port); */
```

`%.*s` + `BR_SV_ARG` is the blessed way to print a `br_string_view` — no cstr
conversion, FULL compiler checking. NOT a custom specifier: probe 8 proved a
custom specifier desyncs the attribute's positional tracking (every arg after it
goes unchecked — shipping that would silently disable the safety feature). `%s`
stays `const char*`. The `(int)` cast lives INSIDE `BR_SV_ARG` (silences
`-Wconversion`). A view longer than `INT_MAX` can't use `%.*s` — the standard
`%.*s` limitation, not ours; a >2 GB path/log value isn't realistic (documented).

## Specifier set (v1)

Conversions: `%d %i` (int), `%u %x %X %o` (unsigned; x lowercase / X uppercase
hex / o octal), `%f %e %g` (double, via `br_format_f64` DECIMAL / EXPONENT /
GENERAL), `%s` (char*), `%.*s` (the view idiom), `%c`, `%p` (pointer as `0x` +
lowercase hex, minimal), `%%`.

Float precision DEFAULTS follow C exactly (muscle memory over cleverness): a
bare `%f` or `%e` means precision 6 — `br_print_to_buffer(..., "%f", 3.14)`
emits `3.140000`, as every C programmer's fingers expect — and `%g` follows C's
significant-digit default. Shortest-round-trip output is NOT a printf verb; it
stays reachable through the builder path (`br_string_builder_write_f64` with
`BR_FLOAT_SHORTEST`).

`%n` is NEVER supported — the parser REFUSES it, emitting the visible marker
(below) rather than the write-back it names; it is the classic format-string
security hole and Bedrock does not parse it.

`%b` (C23 binary) is DEFERRED, not included: `format(printf)` recognition of
`%b` is toolchain-dependent (verified on current Apple clang, but an older
gcc/clang that does not know `%b` positionally DESYNCS the attribute — every
argument after it silently goes unchecked, the exact failure class the custom-
specifier rejection exists to prevent, except here it varies by the CONSUMER's
compiler). Reintroduce `%b` only when the supported-toolchain floor uniformly
recognizes it. Binary dumps meanwhile: `br_format_u64(..., 2, ...)` via the
builder.

## Flags, width, precision

Flags (v1): `-` (left-justify), `0` (zero-pad), `+` (force sign on non-negative),
space (sign placeholder), and `#` (alternate form — see below). EXCLUDED
OUTRIGHT (not deferred): `'` (locale grouping — no locale-free meaning) and `$`
(positional args — complicates both the parser and the attribute's tracking).

Width: a decimal count OR `*` (int arg). Precision: `.N` OR `.*` (int arg) —
`.*` is REQUIRED anyway by the view idiom. For `%f/%e/%g` precision is the
fractional/significant digits; for `%s/%.*s` the max bytes; for integers the
minimum digits.

`*`-ARG CONSUMPTION ORDER (state it — two implementations get this differently):
width `*`, then precision `*`, then the value, consumed LEFT-TO-RIGHT in the
vararg list (matching Go/C). So `"%*.*d"` reads width, precision, value in that
order.

### The `#` decision: INCLUDE for x/X/o, EXCLUDE for floats.

`#` injects the BASE PREFIX: `0x`/`0X` for `%x`/`%X`, `0` for `%o`. Promoted to
v1 because (a) the hex-dump ergonomic is real, and (b) for integer bases it is
ONE uniform mechanism — prefix the rendered digits before padding, in the one
padder, no scattered per-verb code. `#` for `%f/%g` (force-always-decimal-point)
is EXCLUDED: it fights the float leaf and has near-zero demand — a documented
non-goal.

## Field mechanics ownership (the substrate split — state loudly)

The strconv leaves (`br_format_i64/u64/f64/…`) emit ONLY raw digits plus a baked
`-` for negatives — NO width, NO fill, NO sign flags, NO prefix, anywhere.
`print` therefore OWNS ALL field mechanics in the ONE shared padder (Rust's
`pad_integral` shape): width, justification, zero-fill, the `+`/space sign
injection on the NON-NEGATIVE path, and the `#` base prefix. Nobody adds padding
to strconv. Widths and precision are measured in BYTES (C muscle memory, O(1)) —
NOT runes; `%-10.*s` pads to 10 BYTES and a multibyte view can exceed its rune
count. Deliberate, matches C printf, documented (rune-aware display width is a
separate non-goal).

## Architecture

Parse each `%…` spec ONCE into a small options struct
`{ flags, width, precision, length, conv }` (single pass, no re-scan). Each
conversion renders its leaf via the matching `br_format_*` into a stack buffer
bounded by `BR_FORMAT_*_MAX`, then hands the leaf view + spec to the ONE
`br__print_pad` applicator. `%X` needs an uppercase hex path from strconv: add a
`bool uppercase` parameter to the internal `br__format_u64_base` (public
`br_format_u64` stays lowercase-default; print's `%X`/`%#X` calls the internal
with uppercase=true) — a ~5-line src/strconv touch, NO public strconv API
change. Named explicitly for the porter.

## Entry points

```c
/* Fixed caller buffer. Never truncates: if dst_cap is too small, writes nothing
   and returns SHORT_BUFFER with count = the total bytes the full output needs,
   so the caller can size a second pass. On OK, count = bytes written. The
   buffer is a COUNTED result, NOT NUL-terminated (consistent with br_format_*
   and the rest of the into-buffer family); a caller needing a C string reserves
   count+1 and appends '\0', or uses the builder + br_string_clone_to_cstr. */
br_io_result br_print_to_buffer(uint8_t *dst, size_t dst_cap, const char BR_PRINTF_FMT *fmt, ...)
    BR_PRINTF_LIKE(3, 4);
br_io_result br_print_to_buffer_v(uint8_t *dst, size_t dst_cap, const char BR_PRINTF_FMT *fmt, va_list ap)
    BR_PRINTF_LIKE(3, 0);

/* Growing builder — the allocator-backed path, no size guessing. */
br_string_builder_io_result br_print_to_builder(br_string_builder *b, const char BR_PRINTF_FMT *fmt, ...)
    BR_PRINTF_LIKE(2, 3);
br_string_builder_io_result br_print_to_builder_v(br_string_builder *b, const char BR_PRINTF_FMT *fmt, va_list ap)
    BR_PRINTF_LIKE(2, 0);
```

No stdout/stderr/writer entry point in v1. A stderr convenience (`br_eprintf`)
was considered and CUT: `fprintf(stderr, ...)` already exists for eyeball
debugging, and the module's real value is the growable/arbitrary-sink/locale-free
paths libc lacks — a thin stderr wrapper adds a name without adding capability.
`br_print_to_writer` / `br_print_to_file` are the noted future adds when the
`os` module provides real stream sinks.

## Bad input: the visible marker (never abort, never silent, bounded)

A malformed or unsupported specifier does NOT abort and is NOT silently dropped
— it emits a visible bounded marker in the Go/Odin `%!` style: an unknown
conversion `%q` emits `%!q`, `%n` emits `%!n` (refused), a `%` at end-of-format
emits `%!(NOVERB)`, a spec that runs past the format end is marked likewise. The
output stays bounded and the caller SEES the mistake in the text. Combined with
the memory-safe parser (bounds-checked against the fmt length, never over-reads),
malformed formats are a defined, visible, non-crashing outcome.

## Zero-value / empty contracts

- Empty `fmt` (`""`) with no args → zero bytes written, OK (count 0).
- `fmt == NULL` → `BR_STATUS_INVALID_ARGUMENT` (caller misuse, not a format).
- `br_print_to_buffer(NULL, 0, fmt, …)` → SHORT_BUFFER with count = needed bytes
  (the pure-measure path — the way to size before allocating).

## Deviations / notes

- Composes over strconv (locale-free) instead of libc `printf`/`vsnprintf`
  (which honor `LC_NUMERIC`). `print` NEVER consults a locale.
- `%.*s` + `BR_SV_ARG` for views (not a custom specifier — probe 8); `%s` stays
  char*.
- Buffer path is SHORT_BUFFER-never-truncate + measure-on-too-small, NOT libc
  snprintf's truncate-and-return-would-be-length; counted result, no implicit NUL
  (Bedrock into-buffer consistency, and it avoids the mid-UTF-8 truncation corner
  entirely while preserving the two-pass size-then-fill idiom via the measured
  count).
- Bare `%f`/`%e` default to precision 6 (C semantics); shortest-round-trip floats
  live in the builder path, not a printf verb.
- `%n` refused (security); `%b` deferred (toolchain-dependent attribute
  recognition — the consumer-side desync class); `#` limited to integer base
  prefixes; `'`/`$` excluded; positional args out.
- Byte-measured widths (printf-compatible; rune width is a non-goal).
- `%X` via an internal strconv digit-case parameter (no public strconv change).
- No stderr/writer sink in v1 (`br_eprintf` considered and cut — libc's
  `fprintf(stderr,...)` covers eyeball debugging; `_to_writer`/`_to_file` arrive
  with os).
- va_arg widths pinned to the attribute's validated types — the make-or-break
  contract.

## Testing

- ATTRIBUTE↔PARSER WIDTH (the critical set): per-specifier assertion that the
  parser's va_arg type == the attribute's validated type; a compile-time NEGATIVE
  test — a TU with a deliberate `%d`-vs-`int64_t` mismatch must WARN under
  `-Wall -Werror` (proves the attribute is live). Where feasible, the
  probe-derived attribute cases become compile tests.
- Rendering: each conversion × flags/width/precision/`#`/`*` combos vs
  hand-computed expected bytes; `%.*s` with `BR_SV_ARG` (embedded content,
  precision-clip); `%X`/`%#x` uppercase + prefix; the sign/zero/left-justify
  padder matrix; the `*`-consumption order (`%*.*d`); bare-`%f` emits precision-6
  (`3.140000`).
- MARKER PINS: `%q` → `%!q`, `%n` → `%!n`, `%` at end → `%!(NOVERB)`; bounded, no
  over-read (ASan; fuzz the format string).
- BUFFER semantics: never-truncate + SHORT_BUFFER-with-needed-count; the
  measure-then-fill two-pass; NULL/0-cap measure path.
- DIFFERENTIAL vs libc printf for the overlap set (d/i/u/x/o/f/s/c + widths) —
  same bytes, EXCEPT the locale case.
- LOCALE REGRESSION: with `LC_NUMERIC=de_DE` (libc measured emitting `3,14`),
  assert `br_print_to_buffer(..., "%f", 3.14)` emits `3.140000` — print never
  produces the comma.
