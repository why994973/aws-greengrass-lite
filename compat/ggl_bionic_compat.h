// Bionic libc compat shims for Android
// SPDX-License-Identifier: Apache-2.0

#ifndef GGL_ANDROID_BIONIC_COMPAT_H
#define GGL_ANDROID_BIONIC_COMPAT_H

#if GG_PLATFORM_ANDROID

#include <string.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

// strverscmp is a GNU extension not available in Bionic
static inline int strverscmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        if ((*s1 >= '0' && *s1 <= '9') && (*s2 >= '0' && *s2 <= '9')) {
            long v1 = strtol(s1, (char **) &s1, 10);
            long v2 = strtol(s2, (char **) &s2, 10);
            if (v1 != v2) {
                return (v1 < v2) ? -1 : 1;
            }
        } else {
            if (*s1 != *s2) {
                return (unsigned char) *s1 - (unsigned char) *s2;
            }
            s1++;
            s2++;
        }
    }
    return (unsigned char) *s1 - (unsigned char) *s2;
}

// memfd_create available in API 30+, shim for API 28 via syscall
#if __ANDROID_API__ < 30
static inline int memfd_create(const char *name, unsigned int flags) {
    return (int) syscall(__NR_memfd_create, name, flags);
}
#endif

#endif // GG_PLATFORM_ANDROID
#endif
