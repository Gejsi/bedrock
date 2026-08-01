#include <assert.h>
#include <string.h>

#include <bedrock.h>
#include <bedrock/path/slashpath.h>

static br_string_view sv(const char *s) {
  return br_string_view_from_cstr(s);
}

static bool sv_eq(br_string_view a, const char *b) {
  return br_string_equal(a, sv(b));
}

typedef struct test_slashpath_strict_allocation {
  void *ptr;
  usize size;
} test_slashpath_strict_allocation;

typedef struct test_slashpath_strict_allocator {
  test_slashpath_strict_allocation allocations[8];
  usize allocation_count;
  usize size_errors;
} test_slashpath_strict_allocator;

static br_alloc_result test_slashpath_alloc_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static usize test_slashpath_strict_find(const test_slashpath_strict_allocator *strict,
                                        const void *ptr) {
  usize i;

  for (i = 0u; i < strict->allocation_count; i += 1u) {
    if (strict->allocations[i].ptr == ptr) {
      return i;
    }
  }
  return SIZE_MAX;
}

static br_alloc_result test_slashpath_strict_allocator_proc(void *context,
                                                            const br_alloc_request *request) {
  test_slashpath_strict_allocator *strict = (test_slashpath_strict_allocator *)context;
  br_alloc_result result;
  usize index;

  switch (request->op) {
    case BR_ALLOC_OP_ALLOC:
    case BR_ALLOC_OP_ALLOC_UNINIT:
      result = br_allocator_call(br_allocator_heap(), request);
      if (result.status != BR_STATUS_OK || result.ptr == NULL) {
        return result;
      }
      if (strict->allocation_count == BR_ARRAY_COUNT(strict->allocations)) {
        (void)br_allocator_free(br_allocator_heap(), result.ptr, result.size);
        return test_slashpath_alloc_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
      }
      strict->allocations[strict->allocation_count].ptr = result.ptr;
      strict->allocations[strict->allocation_count].size = result.size;
      strict->allocation_count += 1u;
      return result;

    case BR_ALLOC_OP_RESIZE:
    case BR_ALLOC_OP_RESIZE_UNINIT:
    case BR_ALLOC_OP_FREE:
      if (request->ptr == NULL) {
        return br_allocator_call(br_allocator_heap(), request);
      }
      index = test_slashpath_strict_find(strict, request->ptr);
      if (index == SIZE_MAX || strict->allocations[index].size != request->old_size) {
        strict->size_errors += 1u;
        return test_slashpath_alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
      }

      result = br_allocator_call(br_allocator_heap(), request);
      if (result.status != BR_STATUS_OK) {
        return result;
      }
      if (request->op == BR_ALLOC_OP_FREE || request->size == 0u) {
        strict->allocation_count -= 1u;
        strict->allocations[index] = strict->allocations[strict->allocation_count];
      } else {
        strict->allocations[index].ptr = result.ptr;
        strict->allocations[index].size = result.size;
      }
      return result;

    case BR_ALLOC_OP_RESET:
      return test_slashpath_alloc_result(NULL, 0u, BR_STATUS_NOT_SUPPORTED);
  }

  return test_slashpath_alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

static br_allocator test_slashpath_strict_allocator_make(test_slashpath_strict_allocator *strict) {
  br_allocator allocator;

  allocator.fn = test_slashpath_strict_allocator_proc;
  allocator.ctx = strict;
  return allocator;
}

/* clean, with the aliasing contract checked: an already-clean input must come
   back allocated=false. */
static void assert_clean(const char *in, const char *want) {
  br_string_rewrite_result r = br_slashpath_clean(sv(in), br_allocator_heap());
  assert(r.status == BR_STATUS_OK);
  assert(sv_eq(r.value, want));
  assert(br_string_rewrite_free(r, br_allocator_heap()) == BR_STATUS_OK);
}

static void assert_clean_idempotent(const char *want) {
  br_string_rewrite_result r = br_slashpath_clean(sv(want), br_allocator_heap());
  assert(r.status == BR_STATUS_OK);
  assert(sv_eq(r.value, want));
  /* Cleaning an already-clean path must not allocate. */
  assert(!r.allocated);
  assert(br_string_rewrite_free(r, br_allocator_heap()) == BR_STATUS_OK);
}

static void test_clean(void) {
  /* Already clean. */
  assert_clean("", ".");
  assert_clean("abc", "abc");
  assert_clean("abc/def", "abc/def");
  assert_clean("a/b/c", "a/b/c");
  assert_clean(".", ".");
  assert_clean("..", "..");
  assert_clean("../..", "../..");
  assert_clean("../../abc", "../../abc");
  assert_clean("/abc", "/abc");
  assert_clean("/", "/");

  /* Remove trailing slash. */
  assert_clean("abc/", "abc");
  assert_clean("abc/def/", "abc/def");
  assert_clean("a/b/c/", "a/b/c");
  assert_clean("./", ".");
  assert_clean("../", "..");
  assert_clean("../../", "../..");
  assert_clean("/abc/", "/abc");

  /* Remove doubled slash. */
  assert_clean("abc//def//ghi", "abc/def/ghi");
  assert_clean("//abc", "/abc");
  assert_clean("///abc", "/abc");
  assert_clean("//abc//", "/abc");
  assert_clean("abc//", "abc");

  /* Remove . elements. */
  assert_clean("abc/./def", "abc/def");
  assert_clean("/./abc/def", "/abc/def");
  assert_clean("abc/.", "abc");

  /* Remove .. elements. */
  assert_clean("abc/def/ghi/../jkl", "abc/def/jkl");
  assert_clean("abc/def/../ghi/../jkl", "abc/jkl");
  assert_clean("abc/def/..", "abc");
  assert_clean("abc/def/../..", ".");
  assert_clean("/abc/def/../..", "/");
  assert_clean("abc/def/../../..", "..");
  assert_clean("/abc/def/../../..", "/");
  assert_clean("abc/def/../../../ghi/jkl/../../../mno", "../../mno");

  /* Combinations. */
  assert_clean("abc/./../def", "def");
  assert_clean("abc//./../def", "def");
  assert_clean("abc/../../././../def", "../../def");

  /* clean is idempotent on every canonical output. */
  assert_clean_idempotent(".");
  assert_clean_idempotent("/");
  assert_clean_idempotent("abc");
  assert_clean_idempotent("abc/def");
  assert_clean_idempotent("/abc");
  assert_clean_idempotent("../../mno");

  /* Extra: all-slashes and all-dots soups. */
  assert_clean("////", "/");
  assert_clean("/../../..", "/");
  assert_clean("a/../..", "..");
}

static void test_owned_result_sizes(void) {
  test_slashpath_strict_allocator strict;
  br_allocator allocator;
  br_string_rewrite_result cleaned;
  br_string_result joined;
  br_string_view parts[3] = {sv("a"), sv(".."), sv("b")};

  memset(&strict, 0, sizeof(strict));
  allocator = test_slashpath_strict_allocator_make(&strict);

  cleaned = br_slashpath_clean(sv("a/../b"), allocator);
  assert(cleaned.status == BR_STATUS_OK);
  assert(cleaned.allocated);
  assert(sv_eq(cleaned.value, "b"));
  assert(strict.allocation_count == 1u);
  assert(strict.allocations[0].ptr == cleaned.owned.data);
  assert(strict.allocations[0].size == cleaned.owned.len);
  assert(br_string_rewrite_free(cleaned, allocator) == BR_STATUS_OK);
  assert(strict.size_errors == 0u);
  assert(strict.allocation_count == 0u);

  joined = br_slashpath_join(parts, BR_ARRAY_COUNT(parts), allocator);
  assert(joined.status == BR_STATUS_OK);
  assert(sv_eq(br_string_view_from_string(joined.value), "b"));
  assert(strict.allocation_count == 1u);
  assert(strict.allocations[0].ptr == joined.value.data);
  assert(strict.allocations[0].size == joined.value.len);
  assert(br_string_free(joined.value, allocator) == BR_STATUS_OK);
  assert(strict.size_errors == 0u);
  assert(strict.allocation_count == 0u);
}

static void test_split(void) {
  struct {
    const char *path;
    const char *dir;
    const char *file;
  } cases[] = {
    {"a/b", "a/", "b"},
    {"a/b/", "a/b/", ""},
    {"a/", "a/", ""},
    {"a", "", "a"},
    {"/", "/", ""},
    {"", "", ""},
  };
  usize i;

  for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    br_slashpath_split_result r = br_slashpath_split(sv(cases[i].path));
    assert(sv_eq(r.dir, cases[i].dir));
    assert(sv_eq(r.file, cases[i].file));
    /* Invariant: path == dir + file. */
    assert(r.dir.len + r.file.len == strlen(cases[i].path));
    assert(r.dir.data == sv(cases[i].path).data || r.dir.len == 0u);
  }
}

static void test_base(void) {
  assert(sv_eq(br_slashpath_base(sv("")), "."));
  assert(sv_eq(br_slashpath_base(sv(".")), "."));
  assert(sv_eq(br_slashpath_base(sv("/.")), "."));
  assert(sv_eq(br_slashpath_base(sv("/")), "/"));
  assert(sv_eq(br_slashpath_base(sv("////")), "/"));
  assert(sv_eq(br_slashpath_base(sv("x/")), "x"));
  assert(sv_eq(br_slashpath_base(sv("abc")), "abc"));
  assert(sv_eq(br_slashpath_base(sv("abc/def")), "def"));
  assert(sv_eq(br_slashpath_base(sv("a/b/.x")), ".x"));
  assert(sv_eq(br_slashpath_base(sv("a/b/c.")), "c."));
  assert(sv_eq(br_slashpath_base(sv("a/b/c.x")), "c.x"));
}

static void test_dir(void) {
  struct {
    const char *path;
    const char *dir;
  } cases[] = {
    {"", "."},
    {".", "."},
    {"/.", "/"},
    {"/", "/"},
    {"////", "/"},
    {"/foo", "/"},
    {"x/", "x"},
    {"abc", "."},
    {"abc/def", "abc"},
    {"abc////def", "abc"},
    {"a/b/.x", "a/b"},
    {"a/b/c.", "a/b"},
    {"a/b/c.x", "a/b"},
  };
  usize i;

  for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    br_string_rewrite_result r = br_slashpath_dir(sv(cases[i].path), br_allocator_heap());
    assert(r.status == BR_STATUS_OK);
    assert(sv_eq(r.value, cases[i].dir));
    assert(br_string_rewrite_free(r, br_allocator_heap()) == BR_STATUS_OK);
  }
}

static void test_ext(void) {
  assert(sv_eq(br_slashpath_ext(sv("path.go")), ".go"));
  assert(sv_eq(br_slashpath_ext(sv("path.pb.go")), ".go"));
  assert(sv_eq(br_slashpath_ext(sv("a.dir/b")), ""));
  assert(sv_eq(br_slashpath_ext(sv("a.dir/b.go")), ".go"));
  assert(sv_eq(br_slashpath_ext(sv("a.dir/")), ""));
  assert(sv_eq(br_slashpath_ext(sv("")), ""));
}

static void test_name(void) {
  assert(sv_eq(br_slashpath_name(sv("path.go")), "path"));
  assert(sv_eq(br_slashpath_name(sv("a/b/c.x")), "c"));
  assert(sv_eq(br_slashpath_name(sv("a/b/c")), "c"));
  assert(sv_eq(br_slashpath_name(sv("a/b/.x")), ""));
  assert(sv_eq(br_slashpath_name(sv("")), ""));
}

static void assert_join(const char *want, const br_string_view *elems, usize n) {
  br_string_result r = br_slashpath_join(elems, n, br_allocator_heap());
  assert(r.status == BR_STATUS_OK);
  assert(sv_eq(br_string_view_from_string(r.value), want));
  assert(br_string_free(r.value, br_allocator_heap()) == BR_STATUS_OK);
}

static void test_join(void) {
  br_string_view two[2];

  assert_join("", NULL, 0u);

  {
    br_string_view one[1] = {sv("")};
    assert_join("", one, 1u);
  }
  {
    br_string_view one[1] = {sv("a")};
    assert_join("a", one, 1u);
  }

  two[0] = sv("a");
  two[1] = sv("b");
  assert_join("a/b", two, 2u);
  two[0] = sv("a");
  two[1] = sv("");
  assert_join("a", two, 2u);
  two[0] = sv("");
  two[1] = sv("b");
  assert_join("b", two, 2u);
  two[0] = sv("/");
  two[1] = sv("a");
  assert_join("/a", two, 2u);
  two[0] = sv("/");
  two[1] = sv("");
  assert_join("/", two, 2u);
  two[0] = sv("a/");
  two[1] = sv("b");
  assert_join("a/b", two, 2u);
  two[0] = sv("a/");
  two[1] = sv("");
  assert_join("a", two, 2u);
  two[0] = sv("");
  two[1] = sv("");
  assert_join("", two, 2u);

  /* join(split parts) round-trips a cleaned path. */
  {
    br_string_view parts[3] = {sv("a"), sv("b"), sv("c")};
    assert_join("a/b/c", parts, 3u);
  }

  /* clean shortens the joined buffer to a prefix (trailing-slash removal): the
     result must be exactly sized so br_string_free frees the right length. */
  {
    br_string_view one[1] = {sv("ab/")};
    assert_join("ab", one, 1u);
  }
  {
    br_string_view parts[2] = {sv("a/"), sv("b/")};
    assert_join("a/b", parts, 2u);
  }
  /* clean reduces the join to nothing -> "." */
  {
    br_string_view parts[2] = {sv("a"), sv("..")};
    assert_join(".", parts, 2u);
  }
}

static void test_split_elements(void) {
  br_string_view_list_result r = br_slashpath_split_elements(sv("a/b/c"), br_allocator_heap());
  assert(r.status == BR_STATUS_OK);
  assert(r.value.len == 3u);
  assert(sv_eq(r.value.data[0], "a"));
  assert(sv_eq(r.value.data[1], "b"));
  assert(sv_eq(r.value.data[2], "c"));
  assert(br_string_view_list_free(r.value, br_allocator_heap()) == BR_STATUS_OK);
}

static void assert_match(const char *pattern, const char *name, bool want, br_status want_status) {
  br_match_result r = br_slashpath_match(sv(pattern), sv(name));
  assert(r.status == want_status);
  if (want_status == BR_STATUS_OK) {
    assert(r.matched == want);
  } else {
    assert(!r.matched);
  }
}

static void test_match(void) {
  assert_match("abc", "abc", true, BR_STATUS_OK);
  assert_match("*", "abc", true, BR_STATUS_OK);
  assert_match("*c", "abc", true, BR_STATUS_OK);
  assert_match("a*", "a", true, BR_STATUS_OK);
  assert_match("a*", "abc", true, BR_STATUS_OK);
  assert_match("a*", "ab/c", false, BR_STATUS_OK);
  assert_match("a*/b", "abc/b", true, BR_STATUS_OK);
  assert_match("a*/b", "a/c/b", false, BR_STATUS_OK);
  assert_match("a*b*c*d*e*/f", "axbxcxdxe/f", true, BR_STATUS_OK);
  assert_match("a*b*c*d*e*/f", "axbxcxdxexxx/f", true, BR_STATUS_OK);
  assert_match("a*b*c*d*e*/f", "axbxcxdxe/xxx/f", false, BR_STATUS_OK);
  assert_match("a*b*c*d*e*/f", "axbxcxdxexxx/fff", false, BR_STATUS_OK);
  assert_match("a*b?c*x", "abxbbxdbxebxczzx", true, BR_STATUS_OK);
  assert_match("a*b?c*x", "abxbbxdbxebxczzy", false, BR_STATUS_OK);
  assert_match("ab[c]", "abc", true, BR_STATUS_OK);
  assert_match("ab[b-d]", "abc", true, BR_STATUS_OK);
  assert_match("ab[e-g]", "abc", false, BR_STATUS_OK);
  assert_match("ab[^c]", "abc", false, BR_STATUS_OK);
  assert_match("ab[^b-d]", "abc", false, BR_STATUS_OK);
  assert_match("ab[^e-g]", "abc", true, BR_STATUS_OK);
  assert_match("a\\*b", "a*b", true, BR_STATUS_OK);
  assert_match("a\\*b", "ab", false, BR_STATUS_OK);
  assert_match("a?b",
               "a\xe2\x98\xba"
               "b",
               true,
               BR_STATUS_OK); /* a☺b */
  assert_match("a[^a]b",
               "a\xe2\x98\xba"
               "b",
               true,
               BR_STATUS_OK);
  assert_match("a???b",
               "a\xe2\x98\xba"
               "b",
               false,
               BR_STATUS_OK);
  assert_match("a[^a][^a][^a]b",
               "a\xe2\x98\xba"
               "b",
               false,
               BR_STATUS_OK);
  assert_match("[a-\xce\xb6]*", "\xce\xb1", true, BR_STATUS_OK); /* [a-ζ]* vs α */
  assert_match("*[a-\xce\xb6]", "A", false, BR_STATUS_OK);
  assert_match("a?b", "a/b", false, BR_STATUS_OK);
  assert_match("a*b", "a/b", false, BR_STATUS_OK);
  assert_match("[\\]a]", "]", true, BR_STATUS_OK);
  assert_match("[\\-]", "-", true, BR_STATUS_OK);
  assert_match("[x\\-]", "x", true, BR_STATUS_OK);
  assert_match("[x\\-]", "-", true, BR_STATUS_OK);
  assert_match("[x\\-]", "z", false, BR_STATUS_OK);
  assert_match("[\\-x]", "x", true, BR_STATUS_OK);
  assert_match("[\\-x]", "-", true, BR_STATUS_OK);
  assert_match("[\\-x]", "a", false, BR_STATUS_OK);
  assert_match("*x", "xxx", true, BR_STATUS_OK);

  /* Malformed patterns -> INVALID_ARGUMENT. */
  assert_match("[]a]", "]", false, BR_STATUS_INVALID_ARGUMENT);
  assert_match("[-]", "-", false, BR_STATUS_INVALID_ARGUMENT);
  assert_match("[x-]", "x", false, BR_STATUS_INVALID_ARGUMENT);
  assert_match("[x-]", "-", false, BR_STATUS_INVALID_ARGUMENT);
  assert_match("[x-]", "z", false, BR_STATUS_INVALID_ARGUMENT);
  assert_match("[-x]", "x", false, BR_STATUS_INVALID_ARGUMENT);
  assert_match("[-x]", "-", false, BR_STATUS_INVALID_ARGUMENT);
  assert_match("[-x]", "a", false, BR_STATUS_INVALID_ARGUMENT);
  assert_match("\\", "a", false, BR_STATUS_INVALID_ARGUMENT);
  assert_match("[a-b-c]", "a", false, BR_STATUS_INVALID_ARGUMENT);
  assert_match("[", "a", false, BR_STATUS_INVALID_ARGUMENT);
  assert_match("[^", "a", false, BR_STATUS_INVALID_ARGUMENT);
  assert_match("[^bc", "a", false, BR_STATUS_INVALID_ARGUMENT);
  assert_match("a[", "a", false, BR_STATUS_INVALID_ARGUMENT);
  assert_match("a[", "ab", false, BR_STATUS_INVALID_ARGUMENT);
  assert_match("a[", "x", false, BR_STATUS_INVALID_ARGUMENT);
  assert_match("a/b[", "x", false, BR_STATUS_INVALID_ARGUMENT);

  /* Pathological: truncated escape/class must terminate and never read OOB.
     (Run under ASan; the assertions here are just that we get a defined
     answer, not a crash.) */
  assert_match("[a-", "a", false, BR_STATUS_INVALID_ARGUMENT);
  assert_match("\\", "", false, BR_STATUS_INVALID_ARGUMENT);
  assert_match("[abc", "abc", false, BR_STATUS_INVALID_ARGUMENT);
}

static void test_is_predicates(void) {
  assert(br_slashpath_is_separator('/'));
  assert(!br_slashpath_is_separator('\\'));
  assert(br_slashpath_is_abs(sv("/abc")));
  assert(!br_slashpath_is_abs(sv("abc")));
  assert(!br_slashpath_is_abs(sv("")));
}

int main(void) {
  test_is_predicates();
  test_clean();
  test_owned_result_sizes();
  test_split();
  test_base();
  test_dir();
  test_ext();
  test_name();
  test_join();
  test_split_elements();
  test_match();
  return 0;
}
