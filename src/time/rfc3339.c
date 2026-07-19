#include <bedrock/time/rfc3339.h>

#include <bedrock/time/datetime.h>

/*
RFC 3339 parse/format. The parser is a single internal routine shared by the
strict and prefix public entry points; they differ only in whether trailing
bytes are tolerated. The error model mirrors strconv: INVALID_ENCODING for
malformed text (with `consumed` at the first bad byte), OUT_OF_RANGE for a valid
instant the fixed grammar cannot represent.
*/

static br_rfc3339_result br__rfc3339_error(usize consumed) {
  br_rfc3339_result result;
  memset(&result, 0, sizeof(result));
  result.consumed = consumed;
  result.status = BR_STATUS_INVALID_ENCODING;
  return result;
}

static bool br__is_digit(u8 c) {
  return c >= '0' && c <= '9';
}

/* Read exactly `count` digits at `s[pos]` into `*out`. Returns false (leaving
   pos where the bad byte is) if fewer than `count` digits are present. */
static bool br__read_digits(br_string_view s, usize *pos, int count, i64 *out) {
  i64 value = 0;
  int i;
  for (i = 0; i < count; ++i) {
    u8 c;
    if (*pos >= s.len) {
      return false;
    }
    c = (u8)s.data[*pos];
    if (!br__is_digit(c)) {
      return false;
    }
    value = value * 10 + (i64)(c - '0');
    *pos += 1u;
  }
  *out = value;
  return true;
}

static bool br__expect(br_string_view s, usize *pos, char ch) {
  if (*pos >= s.len || s.data[*pos] != ch) {
    return false;
  }
  *pos += 1u;
  return true;
}

/*
Parse an RFC 3339 timestamp starting at the front of `s`. On success fills the
result and sets consumed; on malformed text returns INVALID_ENCODING with
consumed at the first bad byte. `require_all` makes trailing bytes an error.
*/
static br_rfc3339_result br__rfc3339_parse(br_string_view s, bool require_all) {
  br_rfc3339_result result;
  usize pos = 0u;
  i64 year, month, day, hour, minute, second;
  i64 nano = 0;
  bool is_leap = false;
  int32_t offset_min = 0;
  br_datetime dt;
  br_time base;
  br_time_of_day tod;

  if (s.data == NULL || s.len == 0u) {
    return br__rfc3339_error(0u);
  }

  /* date: YYYY-MM-DD */
  if (!br__read_digits(s, &pos, 4, &year) || !br__expect(s, &pos, '-') ||
      !br__read_digits(s, &pos, 2, &month) || !br__expect(s, &pos, '-') ||
      !br__read_digits(s, &pos, 2, &day)) {
    return br__rfc3339_error(pos);
  }

  /* date-time separator: T, t, or space */
  if (pos >= s.len || (s.data[pos] != 'T' && s.data[pos] != 't' && s.data[pos] != ' ')) {
    return br__rfc3339_error(pos);
  }
  pos += 1u;

  /* time: hh:mm:ss */
  if (!br__read_digits(s, &pos, 2, &hour) || !br__expect(s, &pos, ':') ||
      !br__read_digits(s, &pos, 2, &minute) || !br__expect(s, &pos, ':') ||
      !br__read_digits(s, &pos, 2, &second)) {
    return br__rfc3339_error(pos);
  }

  /* optional fractional seconds: . then 1..9 significant digits (accept but
     ignore any beyond 9 — sub-nanosecond is unrepresentable). */
  if (pos < s.len && s.data[pos] == '.') {
    usize frac_start;
    int scale_digits = 0;
    pos += 1u;
    frac_start = pos;
    while (pos < s.len && br__is_digit((u8)s.data[pos])) {
      if (scale_digits < 9) {
        nano = nano * 10 + (i64)(s.data[pos] - '0');
        scale_digits += 1;
      }
      pos += 1u;
    }
    if (pos == frac_start) {
      return br__rfc3339_error(pos); /* '.' with no digits */
    }
    /* Scale the captured digits up to nanoseconds. */
    while (scale_digits < 9) {
      nano *= 10;
      scale_digits += 1;
    }
  }

  /* offset: Z, z, or +/-hh:mm */
  if (pos < s.len && (s.data[pos] == 'Z' || s.data[pos] == 'z')) {
    pos += 1u;
    offset_min = 0;
  } else if (pos < s.len && (s.data[pos] == '+' || s.data[pos] == '-')) {
    char sign = s.data[pos];
    i64 off_h, off_m;
    pos += 1u;
    if (!br__read_digits(s, &pos, 2, &off_h) || !br__expect(s, &pos, ':') ||
        !br__read_digits(s, &pos, 2, &off_m)) {
      return br__rfc3339_error(pos);
    }
    if (off_h > 23 || off_m > 59) {
      return br__rfc3339_error(pos);
    }
    offset_min = (int32_t)(60 * off_h + off_m);
    if (sign == '-') {
      offset_min = -offset_min;
    }
  } else {
    return br__rfc3339_error(pos); /* missing offset */
  }

  /* Leap second: accept :60 ONLY as minute 59 second 60; smear to
     :59.999999999 and flag. Any other :60 falls through to validation, which
     rejects it as malformed. */
  if (second == 60) {
    if (minute == 59) {
      is_leap = true;
      second = 59;
      nano = 999999999;
    }
    /* else: leave second==60 so component validation below rejects it */
  }

  /* Build and validate the civil components. A bad component (e.g. month 13,
     day 32, hour 25, or a non-minute-59 :60) is malformed text here. */
  tod.hour = (int8_t)hour;
  tod.minute = (int8_t)minute;
  tod.second = (int8_t)second;
  tod.nano = (int32_t)nano;
  dt.date.year = year;
  dt.date.month = (int8_t)month;
  dt.date.day = (int8_t)day;
  dt.time = tod;
  if (br_datetime_validate(dt) != BR_STATUS_OK) {
    return br__rfc3339_error(pos);
  }

  if (require_all && pos != s.len) {
    return br__rfc3339_error(pos); /* strict: trailing bytes */
  }

  /* Bridge to an instant. RFC 3339 admits years the int64-ns br_time cannot
     hold (e.g. 1500 or 3000), so this is a genuinely reachable range failure:
     the text was well-formed (consumed = full length) but the instant is
     unrepresentable. Propagate OUT_OF_RANGE. */
  {
    br_time_result bridged = br_datetime_to_time(dt);
    if (bridged.status != BR_STATUS_OK) {
      memset(&result, 0, sizeof(result));
      result.consumed = pos;
      result.status = bridged.status; /* BR_STATUS_OUT_OF_RANGE */
      return result;
    }
    base = bridged.value;
  }

  /* Normalize to UTC: the civil value is at local offset, so subtract it. The
     offset is bounded (|offset| <= 23:59), a small adjustment within the
     representable window the bridge just confirmed. */
  base.nsec -= (i64)offset_min * 60ll * 1000000000ll;

  result.value = base;
  result.utc_offset_min = offset_min;
  result.is_leap_second = is_leap;
  result.consumed = pos;
  result.status = BR_STATUS_OK;
  return result;
}

br_rfc3339_result br_rfc3339_parse(br_string_view s) {
  return br__rfc3339_parse(s, true);
}

br_rfc3339_result br_rfc3339_parse_prefix(br_string_view s) {
  return br__rfc3339_parse(s, false);
}

static br_io_result br__io_result(usize count, br_status status) {
  br_io_result result;
  result.count = count;
  result.status = status;
  return result;
}

/* Write `count` zero-padded digits of `value` at buf[pos..], returning the new
   position. Caller guarantees room. */
static usize br__write_digits(u8 *buf, usize pos, i64 value, int count) {
  int i;
  for (i = count - 1; i >= 0; --i) {
    buf[pos + (usize)i] = (u8)('0' + (int)(value % 10));
    value /= 10;
  }
  return pos + (usize)count;
}

br_io_result br_rfc3339_format(br_time t, int32_t utc_offset_min, u8 *dst, usize dst_cap) {
  u8 buf[BR_RFC3339_MAX];
  usize pos = 0u;
  br_time local;
  br_datetime dt;
  i64 nano;

  /* Render at the requested offset: the stored instant is UTC, so add the
     offset back to recover the local wall clock. */
  local = t;
  local.nsec += (i64)utc_offset_min * 60ll * 1000000000ll;
  dt = br_time_to_datetime(local);

  /* No year-range guard is needed: br_time's int64-ns window spans civil years
     ~1677..2262, entirely inside RFC 3339's 0000..9999 grammar, so the year is
     always 4 digits here. (If br_time ever widens, restore a guard.) The only
     failure this function can report is SHORT_BUFFER. */
  pos = br__write_digits(buf, pos, dt.date.year, 4);
  buf[pos++] = '-';
  pos = br__write_digits(buf, pos, dt.date.month, 2);
  buf[pos++] = '-';
  pos = br__write_digits(buf, pos, dt.date.day, 2);
  buf[pos++] = 'T';
  pos = br__write_digits(buf, pos, dt.time.hour, 2);
  buf[pos++] = ':';
  pos = br__write_digits(buf, pos, dt.time.minute, 2);
  buf[pos++] = ':';
  pos = br__write_digits(buf, pos, dt.time.second, 2);

  /* Fractional seconds only when nonzero, trailing zeros trimmed. */
  nano = dt.time.nano;
  if (nano > 0) {
    u8 frac[9];
    int i;
    int last = -1;
    for (i = 8; i >= 0; --i) {
      frac[i] = (u8)('0' + (int)(nano % 10));
      nano /= 10;
    }
    for (i = 8; i >= 0; --i) {
      if (frac[i] != '0') {
        last = i;
        break;
      }
    }
    buf[pos++] = '.';
    for (i = 0; i <= last; ++i) {
      buf[pos++] = frac[i];
    }
  }

  /* Offset. */
  if (utc_offset_min == 0) {
    buf[pos++] = 'Z';
  } else {
    int32_t mag = utc_offset_min < 0 ? -utc_offset_min : utc_offset_min;
    buf[pos++] = utc_offset_min < 0 ? '-' : '+';
    pos = br__write_digits(buf, pos, mag / 60, 2);
    buf[pos++] = ':';
    pos = br__write_digits(buf, pos, mag % 60, 2);
  }

  if (pos > dst_cap) {
    return br__io_result(0u, BR_STATUS_SHORT_BUFFER); /* never truncate */
  }
  memcpy(dst, buf, pos);
  return br__io_result(pos, BR_STATUS_OK);
}
