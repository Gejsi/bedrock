# Time Module

Bedrock's first `time` slice ports the low-level pieces needed by `sync`
timeouts without pulling in Odin's full datetime/timezone/formatting surface.

## Landed

- `Duration` as `br_duration` (`i64` nanoseconds)
- duration constants from nanosecond through hour
- `Time` as Unix nanoseconds
- `Tick` as monotonic nanoseconds
- `now`, `sleep`, `yield`, `tick_now`, `tick_add`, `tick_diff`, `tick_since`,
  `tick_lap_time`, and duration conversion helpers
- platform split matching Odin's small files:
  - `time_linux.c`
  - `time_unix.c`
  - `time_windows.c`
  - `time_other.c`

## Deferred

- datetime/calendar types
- timezone loading and TZif parsing
- RFC3339/ISO8601 formatting and parsing
- stopwatch helpers
- TSC/perf frequency helpers

Those should be ported as separate, focused slices.
