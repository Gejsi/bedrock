# Time Module

Bedrock's `time` module: the low-level duration/instant/tick pieces that `sync`
timeouts need, plus a minimal civil-datetime + RFC 3339 slice. It deliberately
does NOT include a timezone database, TZif parsing, or ISO-8601 beyond RFC 3339.

## Landed

- `Duration` as `br_duration` (`i64` nanoseconds)
- duration constants from nanosecond through hour
- `Time` as Unix nanoseconds (`br_time { int64_t nsec; }`)
- `Tick` as monotonic nanoseconds
- `now`, `sleep`, `yield`, `tick_now`, `tick_add`, `tick_diff`, `tick_since`,
  `tick_lap_time`, and duration conversion helpers
- platform split matching Odin's small files: `time_linux.c`, `time_unix.c`,
  `time_windows.c`, `time_other.c`

## Civil datetime slice (planned)

A minimal calendrical layer — enough for RFC 3339, with NO timezone database.
All fields are standard C types; field widths match Odin's datetime package.

```c
typedef int64_t br_ordinal; /* proleptic-Gregorian day number; ordinal 1 = Mon Jan 1, 1 A.D. */

typedef struct br_date {
  int64_t year;   /* BR_DATETIME_MIN_YEAR .. BR_DATETIME_MAX_YEAR */
  int8_t  month;  /* 1..12 */
  int8_t  day;    /* 1..days-in-month (leap-aware) */
} br_date;

typedef struct br_time_of_day {
  int8_t  hour;    /* 0..23 */
  int8_t  minute;  /* 0..59 */
  int8_t  second;  /* 0..59 (a parsed :60 leap second is smeared before any
                      datetime is built, so no br_time_of_day ever carries 60) */
  int32_t nano;    /* 0..999_999_999 */
} br_time_of_day;

/* Flattened (C has no Odin `using`); NO tz field — the timezone subsystem is
   out of scope, and RFC 3339 offsets are a plain integer carried by the parse/
   format calls, not the struct. A br_datetime is always a naive/civil value. */
typedef struct br_datetime {
  br_date        date;
  br_time_of_day time;
} br_datetime;

typedef struct br_delta { int64_t days; int64_t seconds; int64_t nanos; } br_delta;

/* The widest OVERFLOW-FREE pure-int64 ordinal range: every intermediate of the
   400-year-cycle ordinal computation stays in int64 with wide margin
   (~9.2e16 ordinals of headroom at these bounds vs ~2e5 at the i64-saturating
   maximum). Deliberately a hair inside Odin's saturating constants: the
   trimmed sliver is unreachable by br_time (civil years ~1677..2262) and any
   real calendar, and the trim keeps the calendrical core a single portable
   int64 path — no __int128, no per-compiler split. */
#define BR_DATETIME_MIN_YEAR (-25000000000000000ll)
#define BR_DATETIME_MAX_YEAR ( 25000000000000000ll)
#define BR_ORDINAL_MIN ((br_ordinal)-9131062500000000365ll)
#define BR_ORDINAL_MAX ((br_ordinal) 9131062500000000000ll)
```

### Calendrical core

```c
br_ordinal br_date_to_ordinal(br_date date);   /* Reingold-Dershowitz proleptic Gregorian */
br_date    br_ordinal_to_date(br_ordinal ord);
bool       br_is_leap_year(int64_t year);

/* Validation: a caller-built struct with an out-of-range field is caller
   misuse -> BR_STATUS_INVALID_ARGUMENT (distinct from parse-layer malformed
   text, which is BR_STATUS_INVALID_ENCODING). */
br_status br_date_validate(br_date date);
br_status br_time_of_day_validate(br_time_of_day t);
br_status br_datetime_validate(br_datetime dt);

typedef struct br_datetime_result { br_datetime value; br_status status; } br_datetime_result;
br_datetime_result br_datetime_from_components(int64_t year, int8_t month, int8_t day,
                                               int8_t hour, int8_t minute, int8_t second, int32_t nano);

br_datetime_result br_datetime_add_delta(br_datetime dt, br_delta delta); /* normalizes + re-validates */
br_delta           br_datetime_subtract(br_datetime a, br_datetime b);    /* a - b, normalized */
```

Deferred within this slice (not ported now): `ordinal_to_datetime`,
`day_of_week`, `last_day_of_month`, `year_range`, `add_days_to_date`, and the
internal gcd/lcm toolkit. Excluded entirely: all `TZ_*` timezone types.

### Epoch bridge

```c
/* Civil datetime <-> Unix-nanosecond instant, treating the datetime as UTC.
   The bridge lives in `time` (the epoch is time's domain; the calendar math is
   the datetime slice's).

   br_time's int64 nanosecond range spans civil years ~1677..2262 only, while
   br_datetime spans ±25 quadrillion years — so datetime->instant is the one
   direction that can fail. A datetime outside the representable window returns
   BR_STATUS_OUT_OF_RANGE via an overflow-safe bounds check (never a silent
   int64 wrap). instant->datetime is total: every int64 nanosecond value maps
   to a valid civil datetime. */
typedef struct br_time_result { br_time value; br_status status; } br_time_result;
br_time_result br_datetime_to_time(br_datetime dt);
br_datetime    br_time_to_datetime(br_time t);
```

### Zero-value contracts

- `br_time_of_day{0}` is VALID: 00:00:00.0 (midnight). hour/minute/second/nano
  are all in range at zero.
- `br_date{0}` is INVALID: month 0 and day 0 are out of range;
  `br_date_validate` returns `BR_STATUS_INVALID_ARGUMENT`. There is no natural
  valid zero date (year 0 exists in the proleptic calendar, but month/day 0 do
  not), so a `br_datetime{0}` is likewise invalid. Build datetimes with
  `br_datetime_from_components` or by parsing; do not rely on zero-init.

## RFC 3339 (planned)

Parse and format RFC 3339 timestamps against `br_time`, reusing the strconv
error model. NO timezone database: offsets are fixed integer minutes.

```c
typedef struct br_rfc3339_result {
  br_time  value;          /* the instant, normalized to UTC (offset applied) */
  int32_t  utc_offset_min; /* parsed offset in minutes (0 for Z); preserved for the caller */
  bool     is_leap_second; /* the accepted :60 case (see below) */
  size_t   consumed;       /* bytes consumed — same convention as strconv parse */
  br_status status;
} br_rfc3339_result;

/* STRICT: the WHOLE view must be a single timestamp, else BR_STATUS_INVALID_ENCODING. */
br_rfc3339_result br_rfc3339_parse(br_string_view s);
/* PERMISSIVE: parse a leading timestamp, allow trailing bytes; `consumed` = bytes used.
   The common form for timestamps embedded in a larger line (e.g. log prefixes). */
br_rfc3339_result br_rfc3339_parse_prefix(br_string_view s);

/* Format the instant at the given fixed offset (offset_min = 0 emits 'Z').
   SHORT_BUFFER (count 0) if dst too small; never truncates. */
br_io_result br_rfc3339_format(br_time t, int32_t utc_offset_min, uint8_t *dst, size_t dst_cap);

/* Widest legal output: YYYY(4) + "-MM-DD"(6)... = date 10, 'T' 1, "hh:mm:ss" 8,
   ".nnnnnnnnn" 10 (dot + 9), "+hh:mm" 6  ->  10+1+8+10+6 = 35. */
#define BR_RFC3339_MAX 35
```

### Parse semantics and decisions

- STRICT BY DEFAULT: `br_rfc3339_parse` requires the whole view to be the
  timestamp (trailing bytes -> `BR_STATUS_INVALID_ENCODING`). The obvious name
  does the safe thing; the permissive consumed-length form is the explicitly
  named `br_rfc3339_parse_prefix` (which timestamps-in-log-lines make common).
  Both report `consumed`; malformed input reports `consumed` at the first bad
  byte and status `BR_STATUS_INVALID_ENCODING`. Caller-misuse cases (a NULL/empty
  view) also return `INVALID_ENCODING` with `consumed = 0` (empty is not a
  timestamp). Mirrors the strconv strict/`_prefix` split exactly.
- SEPARATORS (liberal in, strict out): the date-time separator accepts `T`, `t`,
  OR a space; the zero-offset terminator accepts `Z` or `z` (RFC 3339 §5.6
  permits all of these). Format always EMITS uppercase `T` and `Z`.
- FRACTIONAL SECONDS: parse 1..9 fractional digits into `nano` (scaled by the
  digit count); accept-but-ignore digits beyond 9 (sub-nanosecond is
  unrepresentable). This is a deliberate improvement over Odin, whose parser
  reads only two fractional digits ("hundredths") and silently truncates finer
  input — recorded as an Odin shortfall (suspected-bugs).
- LEAP SECOND: accept `:60` ONLY in the `minute == 59 && second == 60` form
  (a `:60` at any other minute is `BR_STATUS_INVALID_ENCODING`, since component
  validation rejects second 60 there — matching Odin's exact guard). The
  accepted leap second is smeared to `:59.999999999` in the stored `br_time`
  (the last representable instant of that minute) and reported via
  `is_leap_second = true`. A leap-table-free module cannot store a distinct leap
  instant; the flag preserves what the text asserted.
- OFFSET: parsed as signed integer minutes (`±(60*hh + mm)`), applied to
  normalize `value` to UTC, and preserved in `utc_offset_min` so the caller can
  reconstruct the original local wall-clock. No IANA, no tz database.

### Format semantics and decisions

- OFFSET-PRESERVING with an always-Z convenience via the single function: pass
  `utc_offset_min = 0` for canonical `...Z`, or the round-tripped
  `utc_offset_min` from a parse to re-emit the original local offset. Always
  uppercase `T`/`Z`. Fractional output emits `nano` only when nonzero, trailing
  zeros trimmed (Go's RFC3339Nano behavior — no `.000`).
- YEAR RANGE: RFC 3339's grammar is a FIXED 4-digit year (0000..9999) — and
  every representable `br_time` already satisfies it, because int64 nanoseconds
  span civil years ~1677..2262 only. `br_rfc3339_format` therefore CANNOT fail
  on range and carries no dead year guard; its only failure is `SHORT_BUFFER`.
  The valid-input-unrepresentable-target case lives where it is actually
  reachable: at the epoch bridge (`br_datetime_to_time` returns
  `BR_STATUS_OUT_OF_RANGE` for datetimes outside the int64-ns window). If
  `br_time` ever widens beyond int64 nanoseconds, the format year guard must
  be revisited with it.

## Deviations from Odin / Go / Rust

- No timezone/IANA subsystem: RFC 3339 offsets are fixed integer minutes;
  `br_datetime` has no tz field. All `TZ_*` types are out of scope.
- Fractional seconds to 9 digits, vs Odin's 2-digit ("hundredths") scan — a
  fixed shortfall, filed in suspected-bugs.
- Leap second accepted (minute-59 form only) and FLAGGED, instant smeared to
  `:59.999999999`. ROUND-TRIP NOTE: because `br_rfc3339_format` takes a
  `br_time` (which carries no leap flag), re-formatting a parsed `:60` timestamp
  emits `:59.999999999...` — the instant is preserved but the TEXT is not
  identical. This asymmetry is intentional and the only honest behavior for a
  leap-table-free module; callers needing the original text must keep it
  themselves.
- Strict-by-default parse (`_prefix` permissive), consumed-length, and the
  `INVALID_ENCODING` (malformed text) vs `INVALID_ARGUMENT` (caller-built bad
  struct) vs `OUT_OF_RANGE` (datetime outside `br_time`'s int64-ns window, at
  the epoch bridge) taxonomy are reused from strconv.
- vs Go: Go's `time.RFC3339` parses via a reference-layout string and its `Time`
  carries a `*Location`; Bedrock is a dedicated parser producing a naive
  datetime + explicit offset int (no Location). Bedrock matches Go on full
  nanosecond fractional precision, and is more permissive than Go's strict
  layout on the t/space/z separators (RFC 3339 allows them).
- vs Rust: `std` has no datetime/parsing at all; the `chrono`/`time` crates
  confirm the fixed-offset + full-fractional target Bedrock aims at (naive type
  + explicit offset, no IANA unless a tz crate is added).

## Deferred (beyond this slice)

- the remaining datetime conveniences (`day_of_week`, `last_day_of_month`, ...)
- stopwatch helpers (on demand)
- timezone loading and TZif parsing, ISO-8601, TSC/perf helpers: struck — see
  `tracking/cut-list.md`

## Testing

- RFC 3339 round-trip: `parse` then `format` preserves the instant and offset
  for canonical cases; the `:60` leap case round-trips to `:59.999999999` with
  `is_leap_second` set (documented text asymmetry).
- Parse vectors: uppercase/lowercase/space separators; `Z`/`z`; fractional 1..9
  digits plus >9 truncation; the minute-59 `:60` leap (flag set, smeared) AND a
  non-minute-59 `:60` rejected as `INVALID_ENCODING`; all offset signs;
  malformed inputs (bad month/day/hour, missing fields, short strings) ->
  `INVALID_ENCODING` at the right `consumed` offset; strict rejects trailing
  bytes, `_prefix` accepts them with `consumed` set.
- Range boundaries at the TRUE extremes: the exact INT64_MAX- and INT64_MIN-
  nanosecond instants (civil years 2262 and 1677) format and round-trip;
  `br_datetime_to_time` returns `BR_STATUS_OUT_OF_RANGE` for datetimes outside
  the window (e.g. year 9999 and year 2263+) with no silent wrap — the
  overflow-safe check is the regression target; undersized `dst` ->
  `SHORT_BUFFER`, count 0.
- date<->ordinal round-trip across the year range incl. leap years,
  century/400-year boundaries, negative (B.C.) years, and `BR_ORDINAL_MIN/MAX`.
- `is_leap_year` known cases (1900 no, 2000 yes, 2004 yes).
- Epoch bridge: `br_datetime_to_time`/`br_time_to_datetime` round-trip against
  known Unix timestamps (1970-01-01T00:00:00Z == 0 nsec).
- Validation: every out-of-range field -> `INVALID_ARGUMENT`; `br_date{0}`
  invalid, `br_time_of_day{0}` valid (00:00:00).
