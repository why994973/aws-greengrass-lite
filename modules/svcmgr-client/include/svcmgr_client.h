// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef SVCMGR_CLIENT_H
#define SVCMGR_CLIENT_H

//! Client library for communicating with gg-service-manager daemon

#include <gg/error.h>
#include <gg/types.h>

/// Query the lifecycle state of a component.
/// state buffer will be populated with a state string (e.g. "RUNNING").
GgError svcmgr_get_status(GgBuffer component_name, GgBuffer *state);

/// Request service manager to start a component.
GgError svcmgr_start(GgBuffer component_name);

/// Request service manager to stop a component.
GgError svcmgr_stop(GgBuffer component_name);

/// Register a new component with the service manager.
GgError svcmgr_register(GgBuffer component_name, GgBuffer exec_path);

/// Notify service manager that this daemon is ready.
GgError svcmgr_notify_ready(const char *self_name);

#endif
