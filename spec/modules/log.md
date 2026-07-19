# Log

Structured, leveled logging over an explicit `br_logger`. Key-value structured
records are the core; message text is composed by the caller on the landed
substrate (strings builder + `br_format_*`), locale-free. No printf-style format
string in v1.

## Conventions

- No global logger and no ambient state: a `br_logger` is a value the caller
  holds and threads explicitly, like `br_allocator`. The ready-to-use default is
  a CONSTRUCTOR returning a value (`br_log_stderr()`), never a settable
  process-wide singleton.
- Structured key-value attrs are typed (a tagged union), not `printf` varargs —
  type-safe at the call site, no sentinel to forget.
- Rendering is synchronous and bounded: each record renders into a fixed
  stack buffer and is handed to the sink before the call returns; attr string
  and bytes VALUES are borrowed, not copied, so they must outlive the call.
- Locale-free: numeric attrs render through `br_format_i64`/`_u64`/`_f64`
  (always `.` radix, ASCII digits), never libc `printf`/locale.

## Levels

```c
typedef enum br_log_level {
  BR_LOG_DEBUG = 0, BR_LOG_INFO, BR_LOG_WARN, BR_LOG_ERROR
} br_log_level;
```

Four levels (matching Zig's std.log). Ordered so filtering is `level >=
threshold`. No `FATAL`: a library logger must not abort (the no-panic rule);
"log an ERROR then exit" is the caller's decision, not the logger's.

### Two independent filters

1. COMPILE-TIME strip (`BR_LOG_COMPILE_LEVEL`): the per-level macros expand a
   sub-threshold call to `((void)0)`, so it — and its attr compound-literal —
   compile out entirely. Designed in from day one (retrofit is painful; the
   Rust/Zig lesson).
2. RUNTIME filter (`logger->min_level`): the cheap first check inside `br_log`,
   for calls that survive compilation.

A record must pass BOTH. Both default off the build mode via the macro Bedrock
already sets on every build:

```c
#if defined(NDEBUG)
#  define BR_LOG_DEFAULT_LEVEL  BR_LOG_INFO   /* release: -DNDEBUG */
#  define BR_LOG_COMPILE_LEVEL  BR_LOG_INFO
#else
#  define BR_LOG_DEFAULT_LEVEL  BR_LOG_DEBUG  /* debug/sanitize/thread-sanitize: -DDEBUG */
#  define BR_LOG_COMPILE_LEVEL  BR_LOG_DEBUG
#endif
```
(`BR_LOG_COMPILE_LEVEL` is overridable per-TU by defining it before the header —
a release TU can force `BR_LOG_DEBUG` back on for one file, or a hot TU can strip
to `BR_LOG_WARN`.)

## Attrs (typed key-value)

```c
typedef enum br_log_value_kind {
  BR_LOG_I64, BR_LOG_U64, BR_LOG_F64, BR_LOG_BOOL, BR_LOG_STR, BR_LOG_BYTES, BR_LOG_PTR
} br_log_value_kind;

typedef struct br_log_attr {
  br_string_view    key;
  br_log_value_kind kind;
  union {
    int64_t  i64; uint64_t u64; double f64; bool b;
    br_string_view str;    /* borrowed, not copied — must outlive the log call */
    br_bytes_view  bytes;  /* borrowed */
    const void    *ptr;    /* rendered as hex */
  } v;
} br_log_attr;

/* Typed constructors: terse call sites AND full type-checking. */
static inline br_log_attr br_log_i64(br_string_view key, int64_t x);
static inline br_log_attr br_log_u64(br_string_view key, uint64_t x);
static inline br_log_attr br_log_f64(br_string_view key, double x);
static inline br_log_attr br_log_bool(br_string_view key, bool x);
static inline br_log_attr br_log_str(br_string_view key, br_string_view x);
static inline br_log_attr br_log_bytes(br_string_view key, br_bytes_view x);
static inline br_log_attr br_log_ptr(br_string_view key, const void *x);
```

Tagged-union array (with a count), NOT sentinel varargs: varargs logging silently
corrupts on a promoted/wrong-width value and walks off the end on a missing
sentinel (the UB class the house bans); the typed array is fully checked. Mirrors
Go `slog.Attr`, not C `printf`.

## Logger and sink

```c
typedef void (*br_log_sink_fn)(void *ctx, br_log_level level, br_string_view line);
typedef struct br_log_sink { br_log_sink_fn fn; void *ctx; } br_log_sink;

typedef struct br_logger {
  br_log_level   min_level;  /* runtime filter */
  br_log_sink    sink;       /* where rendered lines go */
  br_string_view scope;      /* optional subsystem tag, may be empty */
} br_logger;

br_log_sink br_log_sink_stderr(void);
br_log_sink br_log_sink_callback(br_log_sink_fn fn, void *ctx);
/* Wrap any sink so each dispatch is serialized under the caller's mutex — the
   atomic-line option. The caller owns the mutex (no hidden lock). */
br_log_sink br_log_sink_locked(br_log_sink inner, br_mutex *mutex);

br_logger br_log_stderr(void);  /* {BR_LOG_DEFAULT_LEVEL, stderr sink, no scope} — the don't-think default */
```

The sink receives the fully RENDERED line (level + timestamp + thread id + scope
+ `key=value` attrs) — it only writes bytes somewhere, so routing to journald / a
ring buffer / a test capture is a trivial callback. File sink arrives with the
`os` module (file handles) — a noted dependency, not v1, not blocking.

### Zero-value contract

A zero-initialized `br_logger` is a valid NULL LOGGER: `min_level` 0
(`BR_LOG_DEBUG`), empty scope, and a zeroed sink whose `fn` is NULL. `br_log`
checks `sink.fn == NULL` and returns without emitting — so `br_logger lg = {0}`
safely DISCARDS all output (defined, no crash, useful as a deliberate null sink).
Set a sink to emit. (The `br_thread`-inert lesson: defined and safe, but you
configure it to do anything.)

## Record and the log call

```c
void br_log(br_logger *lg, br_log_level level, br_string_view msg,
            const br_log_attr *attrs, size_t attr_count);

/* Pre-built message (the caller-formats escape hatch): the caller renders msg
   itself (strings builder + br_format_*) and logs the finished view. Identical
   to br_log with a rendered msg + zero attrs; named for intent. */
void br_log_write(br_logger *lg, br_log_level level, br_string_view msg);

/* Per-level convenience MACROS — compile-stripped below BR_LOG_COMPILE_LEVEL. */
#define br_log_debug(lg, msg, attrs, n) /* -> ((void)0) if BR_LOG_DEBUG < BR_LOG_COMPILE_LEVEL */
#define br_log_info(lg, msg, attrs, n)  /* ... */
#define br_log_warn(lg, msg, attrs, n)
#define br_log_error(lg, msg, attrs, n)
```

Rendered record (one line): `<rfc3339-timestamp> <LEVEL> [tid=<n>] [<scope>]
<msg> key=value key=value ...` — timestamp from `br_time_now()` +
`br_rfc3339_format` (which cannot fail for any representable `br_time`, so the
timestamp path is infallible); thread id from `br_current_thread_id()`
(`sync/thread.h`, reused not redefined); scope omitted when empty. Numeric attrs
via `br_format_*`, str/bytes verbatim, bool as `true`/`false`, ptr as `0x…`.

Early-out: `br_log` returns immediately if `sink.fn == NULL` or `level <
lg->min_level`, before rendering anything — a filtered call costs one comparison.

### Rendering buffer contract (bounded, truncation-safe)

`br_log` takes NO allocator, so the record renders into a FIXED stack buffer:

```c
#define BR_LOG_LINE_MAX 2048  /* bytes per rendered line, including newline */
```

2 KiB is chosen deliberately: the fixed prefix (rfc3339 stamp ≤35 + level + tid +
scope) is well under 128 bytes, leaving ~1.9 KiB for message + attrs — comfortable
for ordinary structured records (a dozen KV pairs with short values), while
bounding the per-call stack cost to one page-ish buffer safe for deep call stacks
and threads with small stacks. It is a LINE cap, not a total-output cap; a
consumer needing larger records uses `br_log_write` with its own
allocator-backed rendering and its own sink.

The line is rendered through a `br_string_builder` initialized with
`br_string_builder_init_with_backing` over that stack buffer (fixed, non-growing),
whose writes return `SHORT_BUFFER` at the boundary. So overflow is detected
structurally, never by manual arithmetic, and NEVER writes past the buffer.
TRUNCATION semantics on overflow: rendering stops at the last COMPLETE unit
written (a whole `key=value` pair or the message, never a half-pair that would
parse as a false value), and a visible marker ` ...[truncated]` is appended
within the buffer (the renderer reserves the marker's length up front so the
marker always fits). The truncated line is still emitted — a truncated-but-marked
record beats a dropped one or a lie. This is the strconv never-truncate-silently
discipline applied to the log line.

## Thread-safety

A `br_logger` is a plain value; concurrent calls on the same logger sharing the
same sink are the caller's synchronization responsibility, like any Bedrock
handle. The STDERR SINK CONTRACT: it emits each rendered line (including its
trailing newline) in a SINGLE `write`/`fwrite` call — one syscall per line. That
does not make concurrent writes atomic, but it is the practical interleaving
minimizer (a line is written whole, not field-by-field). A consumer needing
guaranteed non-interleaved lines wraps the sink with `br_log_sink_locked`. No
hidden lock in v1 (no ambient state).

## Scopes

A `br_logger.scope` is an optional subsystem name-tag emitted with each line. Zig's
per-scope COMPILE-TIME level filtering is NOT ported to v1: C has no comptime, so
it would need either an X-macro registry (ugly, central) or a hot-path runtime
scope compare (the cost Zig avoids at compile time). The v1 answer for
per-subsystem levels is one `br_logger` per subsystem with its own `min_level` —
explicit, zero new machinery. Comptime-style per-scope compile levels revisit in
v2 only if needed.

## Deviations / notes

- Structured-KV core; no printf/mini-formatter (the ruled fork). Caller-formats
  via strings builder + `br_format_*` is the escape hatch (`br_log_write`) and the
  strconv dogfood consumer.
- Tagged-union attr array, not sentinel varargs (type safety over Go's variadic
  terseness).
- No global logger; `br_log_stderr()` constructor is the sanctioned default, held
  as a value. No settable process-wide slot (it would be ambient mutable state).
- Build-mode-defaulted level (compile + runtime) via `-DDEBUG`/`-DNDEBUG`, Zig's
  model through Bedrock's `MODE=`.
- No `FATAL` (no-panic rule). Scopes are a name tag; per-scope compile filtering
  deferred. File sink deferred to `os`.
- First consumer of the rfc3339 slice (timestamps).

## Testing

- Compile strip: a TU with `BR_LOG_COMPILE_LEVEL BR_LOG_WARN` — a `br_log_debug`
  call emits nothing AND its attr expression is not evaluated (a side-effect probe
  in the attr args does not fire); assert via a captured counter.
- Runtime filter: below-`min_level` calls don't reach the sink (callback sink
  counts invocations); the early-out doesn't touch attrs.
- KV rendering: each kind formats correctly (i64/u64/f64 via `br_format_*`,
  str/bytes verbatim, bool, ptr hex); line matches expected byte-for-byte.
- BUFFER/TRUNCATION (the required test): a call with attrs whose rendered size
  exceeds `BR_LOG_LINE_MAX` produces a line that (a) never exceeds the buffer
  (ASan clean), (b) ends at a complete unit, (c) carries the `...[truncated]`
  marker. Boundary case: attrs that render to exactly `BR_LOG_LINE_MAX` with and
  without the newline.
- Zeroed logger discards (sink NULL → no emit, no crash).
- stderr single-write: the sink issues one write per line (observable via a
  wrapper counting write calls in the callback-sink test).
- `br_log_sink_locked`: two threads logging through a locked sink produce
  non-interleaved lines (each line intact).
- Timestamp: emitted stamp parses back via `br_rfc3339_parse`.
- Non-owning attrs: a str value backed by a buffer freed right after the call is
  fine (synchronous emit; ASan clean).
- TSan: two threads, two separate loggers, no shared state, no race.
