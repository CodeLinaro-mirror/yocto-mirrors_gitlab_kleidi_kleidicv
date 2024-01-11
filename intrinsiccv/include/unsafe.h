// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_INCLUDE_UNSAFE_H_
#define INTRINSICCV_INCLUDE_UNSAFE_H_

// Inclusion of standard library headers.
#ifdef __cplusplus
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#else  // __cplusplus
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#endif  // __cplusplus

// -----------------------------------------------------------------------------
// Conform to SEI CERT* C Coding Standard MSC24-C: Do not use deprecated or
// obsolescent functions
// * CERT is registered in the U.S. Patent and Trademark Office by Carnegie
// Mellon University.
// -----------------------------------------------------------------------------

#ifdef __GNUC__

// <stdio.h>
#pragma GCC poison gets rewind setbuf
#pragma GCC poison fopen freopen
#pragma GCC poison printf fprintf sprintf snprintf
#pragma GCC poison vprintf vfprintf vsprintf vsnprintf
#pragma GCC poison scanf fscanf sscanf
#pragma GCC poison vscanf vfscanf vsscanf

// <stdlib.h>
#pragma GCC poison atof atoi atol atoll
#pragma GCC poison bsearch getenv mbstowcs qsort

// <string.h>
// NOTE: memcpy and memmove are still useful. Their *_s alternatives would not
// make a difference in practice in this lib.
#pragma GCC poison strcat strncat
#pragma GCC poison strcpy strncpy
#pragma GCC poison strlen strtok strerror

// <time.h>
#pragma GCC poison asctime ctime gmtime localtime

// <wchar.h>
#pragma GCC poison mbsrtowcs
#pragma GCC poison wmemcpy wmemmove
#pragma GCC poison wprintf fwprintf swprintf
#pragma GCC poison vwprintf vfwprintf vswprintf
#pragma GCC poison fwscanf wscanf swscanf
#pragma GCC poison vfwscanf vwscanf vswscanf
#pragma GCC poison wcscat wcscpy wcslen wcstok wctomb wcrtomb wcstombs
#pragma GCC poison wcsncat wcsncpy wcsrtombs

#endif  // __GNUC__

#endif  // INTRINSICCV_INCLUDE_UNSAFE_H_
