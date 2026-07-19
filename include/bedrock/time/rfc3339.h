#ifndef BEDROCK_TIME_RFC3339_H
#define BEDROCK_TIME_RFC3339_H

#include <bedrock/base.h>
#include <bedrock/io/io.h>
#include <bedrock/strings/strings.h>
#include <bedrock/time/time.h>

BR_EXTERN_C_BEGIN

/*
RFC 3339 timestamp parsing and formatting against br_time. There is no timezone
database: offsets are fixed integer minutes carried alongside the instant.
*/

typedef struct br_rfc3339_result {
  br_time value;          /* the instant, normalized to UTC (offset applied) */
  int32_t utc_offset_min; /* parsed offset in minutes (0 for 'Z'); preserved */
  bool is_leap_second;    /* true when an accepted :60 leap second was smeared */
  size_t consumed;        /* bytes consumed (strconv parse convention) */
  br_status status;
} br_rfc3339_result;

/*
Parse a complete RFC 3339 timestamp. The whole view must be the timestamp;
trailing bytes yield BR_STATUS_INVALID_ENCODING. An empty view is not a
timestamp (BR_STATUS_INVALID_ENCODING, consumed 0).
*/
br_rfc3339_result br_rfc3339_parse(br_string_view s);

/*
Parse a leading RFC 3339 timestamp, allowing trailing bytes; `consumed` reports
how many bytes were used. The common form for a timestamp embedded in a larger
line, such as a log prefix.
*/
br_rfc3339_result br_rfc3339_parse_prefix(br_string_view s);

/*
Format `t` at the given fixed offset. An offset of 0 emits the canonical 'Z';
any other offset emits '+hh:mm' / '-hh:mm'. The separator and zero terminator
are always uppercase 'T' and 'Z'. Fractional seconds are emitted only when
nonzero, with trailing zeros trimmed.

Returns the number of bytes written. If `dst` is too small the result is
BR_STATUS_SHORT_BUFFER with count 0 (never a truncated write). This is the only
failure: every representable br_time falls in civil years ~1677..2262, well
inside RFC 3339's fixed 4-digit year, so formatting cannot fail on range. (An
unrepresentable instant is rejected earlier, at the datetime-to-time bridge.)
*/
br_io_result br_rfc3339_format(br_time t, int32_t utc_offset_min, uint8_t *dst, size_t dst_cap);

/* Widest legal output: "YYYY-MM-DDThh:mm:ss.nnnnnnnnn+hh:mm" = 35 bytes. */
#define BR_RFC3339_MAX 35

BR_EXTERN_C_END

#endif
