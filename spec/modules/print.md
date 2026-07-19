# Print

`printf`-family formatting composed over the strconv leaves: familiar `%`-verbs,
locale-free by construction, format string checked by the compiler. Named
`print` (the composer verb over the leaves) — `fmt` is the excluded reflection
engine; `format` collides with `br_format_*`.

## The safety story (this module's whole justification)

`fmt` was excluded because RTTI reflection is a large, runtime-typed,
open-ended surface; `print` is a CLOSED fixed-specifier parser whose one unsafe
coupling is compiler-checked and whose outputs are bounded and write-back-free.
The bounded-safety properties:

1. CLOSED specifier set — a fixed, known grammar; no open-ended type dispatch.
2. COMPILER-CHECKED fmt↔args coupling — `BR_PRINTF_LIKE` makes gcc/clang verify
   argument types against the format string.
3. NO write-back verbs — `%n` is refused; nothing writes through a vararg.
4. BOUNDED-BY-CONSTRUCTION sinks — every entry writes to a caller-sized buffer
   or a growing builder; the parser is bounds-checked against the format length.
5. NO allocation or ambient state in the buffer path; no globals.
6. DETERMINISTIC output — locale-free leaves (always `.`, ASCII digits).

The one thing C CANNOT provide: there is no runtime validation that the varargs
MATCH the format string (C varargs are untyped at runtime). The compiler
attribute is the ONLY backstop, so it must not be defeated (the width contract),
and a DYNAMIC (non-literal) format string gets NO checking — those callers use
the builder path (`br_string_builder_write_int/_uint/_f64/_write`), NOT `print`.
Stated so nobody passes a runtime-built format to `br_*printf` expecting safety.

## The compiler attribute

```c
#if defined(__GNUC__) || defined(__clang__)
/* archetype `printf`, NOT `gnu_printf` (gnu_printf is silently ignored on Apple clang). */
#define BR_PRINTF_LIKE(fmt_index, args_index) __attribute__((format(printf, fmt_index, args_index)))
#else
#define BR_PRINTF_LIKE(fmt_index, args_index)
#endif
#if defined(_MSC_VER)
#include <sal.h>
#define BR_PRINTF_FMT _Printf_format_string_
#else
#define BR_PRINTF_FMT
#endif
```

INDEX RULE (state it so porters never re-derive per entry point): first index =
the 1-based position of the format-string parameter; second index = the position
of the first vararg (always fmt+1 for these shapes); second index is 0 for the
`va_list` twins. Checking fires under the existing `-Wall` — no new flags. The
`BR_PRINTF_FMT` marker precedes the `const char *fmt` parameter (MSVC /analyze).

## THE HARD CONTRACT: parser va_arg widths == what the attribute validates

The make-or-break invariant. `format(printf)` tells the compiler which type to
check each specifier against; the parser's `va_arg` MUST read exactly that type,
including C's default argument promotions, or the compiler's blessing is a lie.

| Specifier | Attribute type | Parser reads |
| --- | --- | --- |
| `%d` `%i` | int | `va_arg(int)` |
| `%u` `%x` `%X` `%o` | unsigned int | `va_arg(unsigned)` |
| `%ld`/`%lu`/`%lx` … | long / unsigned long | `va_arg(long)` … |
| `%lld`/`%llu` … | long long / unsigned long long | `va_arg(long long)` … |
| `%zd` `%zu` | ptrdiff_t / size_t | `va_arg(size_t)` |
| `%f` `%e` `%g` | double | `va_arg(double)` (float promotes; no `%lf` in varargs) |
| `%c` | int | `va_arg(int)`, narrow |
| `%s` | const char* | `va_arg(const char*)` |
| `%.*s` | int, then const char* | `va_arg(int)` prec, `va_arg(const char*)` |
| `%p` | void* | `va_arg(void*)` |
| `%%` | (none) | — |

PROMOTION RULE (verbatim, non-negotiable — the parser reads the PROMOTED type,
never the nominal small type): `%c` and `%hhd`/`%hd` read `va_arg(int)` (char/
short promote to int); `%f`/`%e`/`%g` read `va_arg(double)` (float promotes to
double). Reading `va_arg(char)` or `va_arg(float)` is undefined behavior. The
attribute models these promotions (a `double` passed to `%c` DOES warn), so the
parser matching them is what keeps compiler and runtime in agreement. Length
modifiers `l`/`ll`/`z`/`h`/`hh` are parsed AND honored, routed to the matching
`br_format_*` width — a bare `%d` with an `int64_t` is the caller's bug the
attribute CATCHES.

### The int64_t width trap (state it; it bites a fixed-width-types library)

`int64_t` is `long long` on macOS/LLP64 but `long` on Linux/LP64, so a hardcoded
`%lld` is clean on macOS and WARNS on Linux (and `%ld` is the reverse). Bedrock's
types ARE `int64_t`/`uint64_t`, so this is the daily case, not hypothetical. THE
RULE: for the 64-bit types, callers use the `<inttypes.h>` macros —
`br_*printf("count=%" PRId64 "\n", n)`. `format(printf)` follows the macro
expansion correctly on each platform, so it stays fully checked and portable.
Bedrock does NOT invent a width letter for i64 (a non-standard specifier would
trip the positional-desync below) and does NOT wrap `<inttypes.h>` (no gain over
`PRId64`). This is the clean parser-vs-attribute reconciliation for fixed widths.

## The view idiom (the cstr-grievance fix)

```c
#define BR_SV_ARG(sv) (int)((sv).len), (const char *)((sv).data)
/* br_builder_printf(b, "path=%.*s port=%d\n", BR_SV_ARG(path), port); */
```

`%.*s` + `BR_SV_ARG` is the blessed way to print a `br_string_view` — no cstr
conversion, FULL compiler checking (the `.*` precision pulls an int the attribute
validates; the pointer is checked as `char*`). `%s` stays `const char*`. The
`(int)` cast lives INSIDE `BR_SV_ARG` (silences `-Wconversion`). NOT a custom
specifier — see the positional-desync rule. A view longer than `INT_MAX` can't
use `%.*s` (the standard `%.*s` limitation; a >2 GB path/log value isn't real —
documented).

### Why the specifier set is closed (the positional-desync mechanism)

`format(printf)` walks the format left to right, advancing its argument cursor by
one per RECOGNIZED conversion. A specifier it does not recognize (a custom `%v`,
or a repurposed `%S` which it already binds to `wchar_t*`) is NOT counted as
consuming an argument — so the cursor desyncs, every subsequent conversion is
checked against the WRONG argument, and the desync is permanent for the rest of
the call and cannot be suppressed without also suppressing genuine mismatches.
Therefore br_print's specifier set is CLOSED and every accepted specifier is one
the `printf` archetype also recognizes; views use standard `%.*s`, never a custom
letter. This is why a custom view specifier was rejected — it would silently
disable the safety feature for the rest of the line, not merely add a spelling.

## Specifier set (v1)

`%d %i` (int), `%u %x %X %o` (unsigned int; x lowercase / X uppercase hex /
o octal), `%f %e %g` (double, via `br_format_f64`), `%s` (char*), `%.*s` (the
view idiom), `%c`, `%p` (pointer: `0x` + minimal lowercase hex of the pointer
value — Bedrock DEFINES this rendering rather than matching libc's
implementation-defined `%p`; `va_arg(void*)`), `%%`.

`%n` is NEVER supported: the `printf` archetype ACCEPTS `%n` (standard C), so
the compiler will NOT stop it — its safety is entirely that the parser NEVER
implements it (no write-back verb exists). A `%n` in the format emits the
visible marker instead. This is a hard PARSER INVARIANT, not a compiler
guarantee.

`%b` (C23 binary) is DEFERRED, not included: `format(printf)` recognition of
`%b` is TOOLCHAIN-DEPENDENT. It is verified accepted on current Apple clang
(probe 14), but a CONSUMER building with an older gcc/clang whose checker does
not know `%b` gets the positional desync silently — the per-compiler-varying
version of exactly the failure class the closed-set rule exists to prevent. One
verified toolchain cannot carry a safety guarantee that must hold on every
consumer's compiler. Reintroduce `%b` when the supported-toolchain floor
uniformly recognizes it. Binary output meanwhile: `br_format_u64(..., 2, ...)`
via the builder.

### Float semantics: `%f` is FIXED, default precision 6 (C muscle memory)

`%f` = `br_format_f64` DECIMAL with precision 6 — `br_*printf("%f", 3.14)` prints
`3.140000`, exactly as C fingers expect (verified: BR_FLOAT_DECIMAL is the
`ddd.ddd`, prec-fractional-digits mode). `%.Nf` = N fractional digits. This is
deliberately NOT shortest — a bare `%f` printing `3.14` would be the surprise
this familiar-style module exists to avoid. SHORTEST float output stays reachable
through the builder (`br_string_builder_write_f64` with SHORTEST), documented.
`%e` = EXPONENT, `%g` = GENERAL, both prec from the spec (default 6). Always
locale-free `.`.

## Flags, width, precision

Flags (v1): `-` (left-justify), `0` (zero-pad), `+` (force sign on non-negative),
space (sign placeholder), `#` (alternate form — INCLUDED for `%x/%X/%o` as the
base prefix `0x`/`0X`/`0`; EXCLUDED for floats, a documented non-goal). These
flags are checker-neutral (they don't change the expected argument type), so the
parser owns their semantics entirely. EXCLUDED OUTRIGHT: `'` (locale grouping — no
locale-free meaning) and `$` positional args (complicate the parser AND the
attribute's positional tracking).

Width: decimal count OR `*` (int arg). Precision: `.N` OR `.*` (int arg; required
by the view idiom). `*`-ARG CONSUMPTION ORDER (state it — implementations differ):
width `*`, then precision `*`, then the value, LEFT-TO-RIGHT (matching C/Go). So
`"%*.*d"` reads width, precision, value in that order.

## Field-mechanics ownership (the substrate split)

The `br_format_*` leaves emit ONLY raw digits plus a baked `-` for negatives — no
width, fill, sign flag, or prefix, anywhere. `print` therefore OWNS ALL field
mechanics in ONE shared padder (Rust's `pad_integral` shape): width,
justification, zero-fill, `+`/space sign injection on the NON-NEGATIVE path (the
leaf already baked `-` — don't double-sign), and the `#` base prefix. Nobody adds
padding to strconv. Widths/precision are BYTE-measured (C muscle memory, O(1)) —
NOT runes; `%-10.*s` pads to 10 BYTES and a multibyte view can exceed its rune
count. Deliberate, printf-compatible, documented (rune-width is a v2 non-goal).

## Architecture

Parse each `%…` spec ONCE into `{flags, width, precision, length, verb}` (single
pass, no re-scan), then dispatch. Each conversion renders its leaf via the
matching `br_format_*` into a stack buffer sized by `BR_FORMAT_*_MAX` /
`br_format_f64_bound(fmt, prec)` (so a pathological precision cannot overflow),
then hands the leaf view + spec to the ONE `br__print_pad` applicator. `%X`/`%#X`
need uppercase hex from strconv: add a `bool uppercase` parameter to the internal
`br__format_u64_base` (public `br_format_u64` stays lowercase-default) — a ~5-line
`src/strconv/format.c` touch, NO public strconv API change. Named for the porter.

## Entry points

```c
/* Fixed caller buffer, C99 snprintf semantics: writes up to dst_cap-1 bytes +
   NUL when cap>0, and returns the count that the FULL output WOULD need (so a
   caller sizes a second pass). Never writes past dst_cap. */
br_io_result br_snprintf(uint8_t *dst, size_t dst_cap, const char BR_PRINTF_FMT *fmt, ...)
    BR_PRINTF_LIKE(3, 4);
br_io_result br_vsnprintf(uint8_t *dst, size_t dst_cap, const char BR_PRINTF_FMT *fmt, va_list ap)
    BR_PRINTF_LIKE(3, 0);

/* Growing builder — the allocator-backed path, no size guessing. */
br_string_builder_io_result br_builder_printf(br_string_builder *b, const char BR_PRINTF_FMT *fmt, ...)
    BR_PRINTF_LIKE(2, 3);
br_string_builder_io_result br_vbuilder_printf(br_string_builder *b, const char BR_PRINTF_FMT *fmt, va_list ap)
    BR_PRINTF_LIKE(2, 0);
```

The `va_list` twins take `BR_PRINTF_LIKE(N, 0)` and the fmt-literal contract still
propagates through a varargs wrapper that forwards to them (so a future logger
built as a thin wrapper over `br_vbuilder_printf` stays call-site-checked). No
stdout/stderr/writer entry in v1 — a stream sink needs `os`, and a bare stderr
convenience was considered and CUT (`fprintf(stderr, ...)` already exists for
eyeball debugging; the module's value is the growable/locale-free paths libc
lacks). `br_fprintf` / `br_eprintf` are the noted future add when os lands.

## Bad input: the visible marker (never abort, never silent, bounded)

A malformed or unsupported specifier does NOT abort and is NOT silently dropped —
it emits a visible bounded marker (Go/Odin `%!` style): `%q` → `%!q`, `%n` →
`%!n` (refused), a `%` at format end → `%!(NOVERB)`. The output stays bounded and
the caller SEES the mistake. Combined with the bounds-checked parser (never
over-reads the format), malformed formats are a defined, visible, non-crashing
outcome.

## Zero-value / empty contracts

- Empty `fmt` (`""`) → zero bytes, OK (count 0).
- `fmt == NULL` → `BR_STATUS_INVALID_ARGUMENT` (caller misuse).
- `br_snprintf(NULL, 0, fmt, …)` → the pure-measure path: writes nothing, returns
  the count the full output needs (size before allocating).

## Deviations / notes

- Composes over strconv (locale-free); NEVER libc `printf`/`vsnprintf` (which
  honor `LC_NUMERIC` — libc was MEASURED emitting `3,14` under `de_DE`).
- `%f` is FIXED prec-6 (C semantics), not shortest; SHORTEST via the builder.
- `%.*s` + `BR_SV_ARG` for views (not a custom specifier — positional desync);
  `%s` stays char*.
- SNPRINTF SEMANTICS ARE A DEVIATION FROM THE HOUSE NEVER-TRUNCATE RULE, recorded
  here with rationale: `br_snprintf` truncates at `dst_cap` (+ NUL) and returns
  the would-be length. Unlike a silent half-number, the truncation is VISIBLE
  (return > cap), and the two-pass size-then-fill idiom REQUIRES the would-be
  length — which is the genuinely useful contract for a printf-family buffer
  function, and the semantics every C programmer already knows under this name.
  BYTE-TRUNCATION CORNER: cutting at `dst_cap` can split a multibyte UTF-8
  sequence (printf-compatible byte semantics) — stated plainly; a caller needing
  whole-rune truncation sizes via the measure path first.
- `%n` refused (parser invariant); `%b` deferred (toolchain-dependent attribute
  recognition — the consumer-side desync class); `#` limited to integer base
  prefixes; `'`/`$` excluded; positional args out.
- `int64_t`/`uint64_t` use `PRId64`/`PRIu64` (the width trap); byte-measured
  widths; `%X` via internal strconv digit-case param; `%p` Bedrock-defined.
- No writer/stdout sink until os (`br_fprintf`/`br_eprintf` deferred; the stderr
  convenience cut on maintainer review).

## Testing

- ATTRIBUTE↔PARSER WIDTH (critical): per-specifier assertion that the parser's
  va_arg type == the attribute's validated type, incl. the promotions (`%c`←int,
  `%f`←double); a COMPILE-TIME negative test — a TU with a deliberate
  `%d`-vs-`int64_t` mismatch must WARN under `-Wall -Werror` (proves the attribute
  is live); the `PRId64` case is clean on both LP64 and LLP64.
- Rendering: each conversion × flags/width/precision/`#`/`*` vs hand-computed
  bytes; `%f` prints `3.140000` (fixed prec-6); `%.2f` → `3.14`; `%.*s` +
  `BR_SV_ARG` (embedded content, precision-clip); `%X`/`%#x`; the `*`-consumption
  order (`%*.*d`).
- MARKERS: `%q`→`%!q`, `%n`→`%!n`, `%` at end→`%!(NOVERB)`; bounded, no over-read
  (ASan; fuzz the format string).
- SNPRINTF semantics: truncate-at-cap + NUL + would-be-length return; the
  measure-then-fill two-pass; NULL/0-cap measure path; the split-multibyte
  truncation corner is byte-exact.
- DIFFERENTIAL vs libc printf for the overlap set (d/i/u/x/o/f/s/c + widths) —
  same bytes EXCEPT under a non-C locale.
- LOCALE REGRESSION: with `LC_NUMERIC=de_DE`, assert `br_snprintf(..., "%f", 3.14)`
  emits `3.140000` (a `.`), never `3,140000` — the strconv locale-free property.
