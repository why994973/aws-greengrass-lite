// OpenSSL 3.x http.h stub for 1.1.1 — proxy functions not available
// SPDX-License-Identifier: Apache-2.0

#ifndef OPENSSL_HTTP_H_COMPAT
#define OPENSSL_HTTP_H_COMPAT

#include <openssl/bio.h>

// Stub: no proxy support on OpenSSL 1.1.1
static inline const char *OSSL_HTTP_adapt_proxy(
    const char *proxy, const char *no_proxy, const char *server, int use_ssl
) {
    (void) proxy;
    (void) no_proxy;
    (void) server;
    (void) use_ssl;
    return NULL;
}

static inline int OSSL_HTTP_proxy_connect(
    BIO *bio, const char *server, const char *port, const char *proxyuser,
    const char *proxypass, int timeout, BIO *bio_err, const char *prog
) {
    (void) bio;
    (void) server;
    (void) port;
    (void) proxyuser;
    (void) proxypass;
    (void) timeout;
    (void) bio_err;
    (void) prog;
    return 0;
}

#endif
