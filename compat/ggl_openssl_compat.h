// OpenSSL 3.x -> 1.1.1 compat shims for Android
// SPDX-License-Identifier: Apache-2.0

#ifndef GGL_ANDROID_OPENSSL_COMPAT_H
#define GGL_ANDROID_OPENSSL_COMPAT_H

#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>

// OpenSSL 3.x renamed these accessors
#ifndef EVP_MD_get_block_size
#define EVP_MD_get_block_size EVP_MD_block_size
#endif

#ifndef EVP_MD_get_size
#define EVP_MD_get_size EVP_MD_size
#endif

// kTLS not available in OpenSSL 1.1.1
#ifndef SSL_OP_ENABLE_KTLS
#define SSL_OP_ENABLE_KTLS 0
#endif

static inline int BIO_get_ktls_send(BIO *b) { (void) b; return 0; }
static inline int BIO_get_ktls_recv(BIO *b) { (void) b; return 0; }

// SSL_CTX_load_verify_file is OpenSSL 3.x — use 1.1.1 equivalent
#define SSL_CTX_load_verify_file(ctx, file) \
    SSL_CTX_load_verify_locations(ctx, file, NULL)

#endif
