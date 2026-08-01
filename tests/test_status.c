#include <assert.h>
#include <string.h>

#include <bedrock.h>

static void test_status_strings(void) {
  assert(strcmp(br_status_string(BR_STATUS_OK), "ok") == 0);
  assert(strcmp(br_status_string(BR_STATUS_NOT_FOUND), "not found") == 0);
  assert(strcmp(br_status_string(BR_STATUS_IO_ERROR), "I/O error") == 0);
  assert(strcmp(br_status_string((br_status)-1), "unknown status") == 0);
  assert(strcmp(br_status_string((br_status)9999), "unknown status") == 0);
}

static void test_errors(void) {
  br_error plain;
  br_error native;

  plain = br_error_make(BR_STATUS_NOT_FOUND);
  assert(plain.status == BR_STATUS_NOT_FOUND);
  assert(plain.native.domain == BR_ERROR_DOMAIN_NONE);
  assert(plain.native.code == 0u);
  assert(!br_error_is_ok(plain));

  native = br_error_make_native(BR_STATUS_PERMISSION_DENIED, BR_ERROR_DOMAIN_POSIX_ERRNO, 13u);
  assert(native.status == BR_STATUS_PERMISSION_DENIED);
  assert(native.native.domain == BR_ERROR_DOMAIN_POSIX_ERRNO);
  assert(native.native.code == 13u);

  native = br_error_make_native(BR_STATUS_OK, BR_ERROR_DOMAIN_WIN32, 5u);
  assert(br_error_is_ok(native));
  assert(native.native.domain == BR_ERROR_DOMAIN_NONE);
  assert(native.native.code == 0u);
}

int main(void) {
  test_status_strings();
  test_errors();
  return 0;
}
