#include <bedrock/rand/rand.h>

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

/* BCRYPT_USE_SYSTEM_PREFERRED_RNG lets us pass a NULL algorithm handle and use
   the system default RNG. Declared here to avoid a hard dependency on
   <bcrypt.h> / the bcrypt import library — the function is resolved at runtime
   via GetProcAddress, like the futex backend's WaitOnAddress. */
#define BR__RAND_BCRYPT_USE_SYSTEM_PREFERRED_RNG 0x00000002

typedef LONG(WINAPI *br__rand_gen_random_fn)(void *algorithm,
                                             unsigned char *buffer,
                                             unsigned long count,
                                             unsigned long flags);

br_status br_rand_entropy_fill(void *dst, usize len) {
  HMODULE bcrypt;
  br__rand_gen_random_fn gen_random;
  u8 *out = (u8 *)dst;
  usize filled = 0u;

  if (len == 0u) {
    return BR_STATUS_OK;
  }

  bcrypt = LoadLibraryW(L"bcrypt.dll");
  if (bcrypt == NULL) {
    return BR_STATUS_NOT_SUPPORTED;
  }

  gen_random = (br__rand_gen_random_fn)(void (*)(void))GetProcAddress(bcrypt, "BCryptGenRandom");
  if (gen_random == NULL) {
    FreeLibrary(bcrypt);
    return BR_STATUS_NOT_SUPPORTED;
  }

  /* BCryptGenRandom takes a ULONG count; fill in <=UINT32_MAX chunks so a huge
     len cannot truncate. Whole-buffer-or-error. */
  while (filled < len) {
    usize chunk = len - filled;
    if (chunk > 0xffffffffu) {
      chunk = 0xffffffffu;
    }
    if (gen_random(
          NULL, out + filled, (unsigned long)chunk, BR__RAND_BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) {
      FreeLibrary(bcrypt);
      return BR_STATUS_NOT_SUPPORTED;
    }
    filled += chunk;
  }

  FreeLibrary(bcrypt);
  return BR_STATUS_OK;
}

#endif /* defined(_WIN32) */
