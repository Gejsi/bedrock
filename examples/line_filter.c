#include <limits.h>

#include <bedrock.h>

enum { LINE_FILTER_INPUT_BUFFER_SIZE = 4096, LINE_FILTER_RECORD_SCRATCH_SIZE = 16 * 1024 };

static br_status write_view(br_writer sink, br_string_view value) {
  return br_write_full(sink, value.data, value.len).status;
}

static int report_error(const char *operation, br_status status) {
  br_writer error_output;

  error_output = br_stderr();
  BR_UNUSED(write_view(error_output, br_string_view_from_cstr(operation)));
  BR_UNUSED(write_view(error_output, BR_STR_LIT(": ")));
  BR_UNUSED(write_view(error_output, br_string_view_from_cstr(br_status_string(status))));
  BR_UNUSED(write_view(error_output, BR_STR_LIT("\n")));
  return 1;
}

static br_status add_checked(int64_t *total, int64_t value) {
  if ((value > 0 && *total > INT64_MAX - value) || (value < 0 && *total < INT64_MIN - value)) {
    return BR_STATUS_OUT_OF_RANGE;
  }

  *total += value;
  return BR_STATUS_OK;
}

static br_status process_record(br_string_view line, int64_t *total) {
  br_string_split_iter fields;
  br_string_view action;
  br_string_view amount;
  br_string_view extra;
  br_parse_i64_result parsed;

  line = br_string_trim_suffix(line, BR_STR_LIT("\n"));
  line = br_string_trim_suffix(line, BR_STR_LIT("\r"));
  fields = br_string_split_iter_make(line, BR_STR_LIT(","));
  if (!br_string_split_iter_next(&fields, &action) ||
      !br_string_split_iter_next(&fields, &amount) || br_string_split_iter_next(&fields, &extra)) {
    return BR_STATUS_INVALID_ENCODING;
  }

  action = br_string_trim_space(action);
  amount = br_string_trim_space(amount);
  parsed = br_parse_i64(amount, 10);
  if (parsed.status != BR_STATUS_OK) {
    return parsed.status;
  }

  if (br_string_equal(action, BR_STR_LIT("keep"))) {
    return add_checked(total, parsed.value);
  }
  if (br_string_equal(action, BR_STR_LIT("skip"))) {
    return BR_STATUS_OK;
  }
  return BR_STATUS_INVALID_ENCODING;
}

static br_status write_total(int64_t total) {
  uint8_t number[BR_FORMAT_I64_MAX];
  br_io_result formatted;
  br_writer output;
  br_status status;

  formatted = br_format_i64(total, 10, number, sizeof(number));
  if (formatted.status != BR_STATUS_OK) {
    return formatted.status;
  }

  output = br_stdout();
  status = write_view(output, BR_STR_LIT("total="));
  if (status != BR_STATUS_OK) {
    return status;
  }
  status = br_write_full(output, number, formatted.count).status;
  if (status != BR_STATUS_OK) {
    return status;
  }
  return write_view(output, BR_STR_LIT("\n"));
}

int main(void) {
  uint8_t input_buffer[LINE_FILTER_INPUT_BUFFER_SIZE];
  br_allocator allocator;
  br_bufio_reader reader;
  br_scratch record_scratch;
  int64_t total;
  br_status status;

  status = br_scratch_init(&record_scratch, LINE_FILTER_RECORD_SCRATCH_SIZE, br_allocator_heap());
  if (status != BR_STATUS_OK) {
    return report_error("initialize record memory", status);
  }
  allocator = br_scratch_allocator(&record_scratch);
  status =
    br_bufio_reader_init_with_buffer(&reader, br_stdin(), input_buffer, sizeof(input_buffer));
  if (status != BR_STATUS_OK) {
    br_scratch_destroy(&record_scratch);
    return report_error("initialize input", status);
  }

  total = 0;
  for (;;) {
    br_bufio_reader_string_result record;
    br_status free_status;
    const char *operation;

    record = br_bufio_reader_read_string(&reader, (uint8_t)'\n', allocator);
    operation = "read record";
    status = record.status;
    if (record.status == BR_STATUS_OK || record.status == BR_STATUS_EOF) {
      status = BR_STATUS_OK;
      if (record.value.len > 0u) {
        operation = "parse record";
        status = process_record(br_string_view_from_string(record.value), &total);
      }
    }

    free_status = br_string_free(record.value, allocator);
    if (status != BR_STATUS_OK) {
      br_bufio_reader_destroy(&reader);
      br_scratch_destroy(&record_scratch);
      return report_error(operation, status);
    }
    if (free_status != BR_STATUS_OK) {
      br_bufio_reader_destroy(&reader);
      br_scratch_destroy(&record_scratch);
      return report_error("release record", free_status);
    }

    if (record.status == BR_STATUS_EOF) {
      break;
    }
  }

  br_bufio_reader_destroy(&reader);
  br_scratch_destroy(&record_scratch);
  status = write_total(total);
  if (status != BR_STATUS_OK) {
    return report_error("write total", status);
  }
  return 0;
}
