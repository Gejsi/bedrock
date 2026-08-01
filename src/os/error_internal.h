#ifndef BEDROCK_OS_ERROR_INTERNAL_H
#define BEDROCK_OS_ERROR_INTERNAL_H

#include <bedrock/base.h>

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

br_error br__os_error_from_win32(DWORD error);
br_error br__os_error_from_win32_status(DWORD error, br_status status);

#else

br_error br__os_error_from_errno(int error);

#endif

#endif
