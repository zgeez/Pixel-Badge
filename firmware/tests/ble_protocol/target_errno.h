/* SPDX-License-Identifier: MIT */
#pragma once
#include <errno.h>
/* The DK's SDK picolibc uses 119, unlike native Linux's 115. The existing
 * byte-for-byte protocol expectation must pass under either libc value. */
#undef EINPROGRESS
#define EINPROGRESS 119
