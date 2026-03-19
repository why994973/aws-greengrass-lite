// Android UUID compat - wraps /dev/urandom for uuid_generate_random
// SPDX-License-Identifier: Apache-2.0

#include "uuid/uuid.h"
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

void uuid_generate_random(uuid_t out) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        (void) !read(fd, out, 16);
        close(fd);
    }
    // Set version 4 and variant bits per RFC 4122
    out[6] = (unsigned char) ((out[6] & 0x0F) | 0x40);
    out[8] = (unsigned char) ((out[8] & 0x3F) | 0x80);
}

void uuid_unparse(const uuid_t uu, char *out) {
    snprintf(
        out, 37,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uu[0], uu[1], uu[2], uu[3], uu[4], uu[5], uu[6], uu[7],
        uu[8], uu[9], uu[10], uu[11], uu[12], uu[13], uu[14], uu[15]
    );
}
