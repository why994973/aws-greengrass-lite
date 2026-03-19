// Android UUID compat - wraps /dev/urandom for uuid_generate_random
// SPDX-License-Identifier: Apache-2.0

#ifndef ANDROID_UUID_H
#define ANDROID_UUID_H

typedef unsigned char uuid_t[16];

void uuid_generate_random(uuid_t out);
void uuid_unparse(const uuid_t uu, char *out);

#endif
