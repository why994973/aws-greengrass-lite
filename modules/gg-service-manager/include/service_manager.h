// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef GG_SERVICE_MANAGER_H
#define GG_SERVICE_MANAGER_H

//! Service manager daemon - platform-agnostic process lifecycle management

#include <gg/error.h>

/// Pluggable backend interface (defined in svcmgr_backend.h).
typedef struct SvcMgrBackend SvcMgrBackend;

/// Set the active backend for the service manager.
void svcmgr_set_backend(SvcMgrBackend *backend);

/// Initialize the s6 backend (call before run_gg_service_manager).
void svcmgr_init_s6_backend(void);

/// Run the service manager daemon (blocks).
GgError run_gg_service_manager(void);

#endif
