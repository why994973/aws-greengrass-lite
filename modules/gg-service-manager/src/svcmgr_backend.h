// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef SVCMGR_BACKEND_H
#define SVCMGR_BACKEND_H

//! Pluggable backend interface for service manager

#include <gg/error.h>
#include <gg/types.h>

/// Service state as reported by the backend.
typedef enum {
    SVCMGR_STATE_UNKNOWN = 0,
    SVCMGR_STATE_RUNNING,
    SVCMGR_STATE_STARTING,
    SVCMGR_STATE_STOPPED,
    SVCMGR_STATE_ERRORED,
} SvcMgrState;

/// Backend interface for process lifecycle management.
typedef struct SvcMgrBackend {
    GgError (*start)(GgBuffer name);
    GgError (*stop)(GgBuffer name);
    GgError (*status)(GgBuffer name, SvcMgrState *state);
    GgError (*register_service)(
        GgBuffer name, GgBuffer exec_path, GgBuffer *args, size_t args_len
    );
    GgError (*unregister_service)(GgBuffer name);
} SvcMgrBackend;

/// Convert state enum to string.
GgBuffer svcmgr_state_to_str(SvcMgrState state);

#endif
