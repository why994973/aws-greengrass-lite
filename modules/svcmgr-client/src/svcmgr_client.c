// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include <svcmgr_client.h>
#include <gg/arena.h>
#include <gg/buffer.h>
#include <gg/error.h>
#include <gg/log.h>
#include <gg/map.h>
#include <gg/object.h>
#include <gg/types.h>
#include <ggl/core_bus/client.h>
#include <string.h>

#define SVCMGR_INTERFACE GG_STR("gg_service_manager")

GgError svcmgr_get_status(GgBuffer component_name, GgBuffer *state) {
    static uint8_t resp_mem[256] = { 0 };
    GgArena resp_alloc = gg_arena_init(GG_BUF(resp_mem));

    GgObject result;
    GgError method_error;
    GgError ret = ggl_call(
        SVCMGR_INTERFACE,
        GG_STR("status"),
        GG_MAP(gg_kv(GG_STR("name"), gg_obj_buf(component_name))),
        &method_error,
        &resp_alloc,
        &result
    );
    if (ret != GG_ERR_OK) {
        if (ret == GG_ERR_REMOTE) {
            return method_error;
        }
        return ret;
    }

    if (gg_obj_type(result) != GG_TYPE_MAP) {
        return GG_ERR_INVALID;
    }
    GgMap result_map = gg_obj_into_map(result);

    GgObject *state_obj;
    if (!gg_map_get(result_map, GG_STR("state"), &state_obj)) {
        return GG_ERR_NOENTRY;
    }
    if (gg_obj_type(*state_obj) != GG_TYPE_BUF) {
        return GG_ERR_INVALID;
    }
    *state = gg_obj_into_buf(*state_obj);
    return GG_ERR_OK;
}

GgError svcmgr_start(GgBuffer component_name) {
    GgError method_error;
    GgError ret = ggl_call(
        SVCMGR_INTERFACE,
        GG_STR("start"),
        GG_MAP(gg_kv(GG_STR("name"), gg_obj_buf(component_name))),
        &method_error,
        NULL,
        NULL
    );
    if (ret == GG_ERR_REMOTE) {
        return method_error;
    }
    return ret;
}

GgError svcmgr_stop(GgBuffer component_name) {
    GgError method_error;
    GgError ret = ggl_call(
        SVCMGR_INTERFACE,
        GG_STR("stop"),
        GG_MAP(gg_kv(GG_STR("name"), gg_obj_buf(component_name))),
        &method_error,
        NULL,
        NULL
    );
    if (ret == GG_ERR_REMOTE) {
        return method_error;
    }
    return ret;
}

GgError svcmgr_register(GgBuffer component_name, GgBuffer exec_path) {
    GgError method_error;
    GgError ret = ggl_call(
        SVCMGR_INTERFACE,
        GG_STR("register"),
        GG_MAP(
            gg_kv(GG_STR("name"), gg_obj_buf(component_name)),
            gg_kv(GG_STR("exec"), gg_obj_buf(exec_path))
        ),
        &method_error,
        NULL,
        NULL
    );
    if (ret == GG_ERR_REMOTE) {
        return method_error;
    }
    return ret;
}

GgError svcmgr_notify_ready(const char *self_name) {
    GgBuffer name = {
        .data = (uint8_t *) self_name,
        .len = strlen(self_name),
    };
    GgError method_error;
    GgError ret = ggl_call(
        SVCMGR_INTERFACE,
        GG_STR("notify_ready"),
        GG_MAP(gg_kv(GG_STR("name"), gg_obj_buf(name))),
        &method_error,
        NULL,
        NULL
    );
    if (ret == GG_ERR_REMOTE) {
        return method_error;
    }
    if (ret != GG_ERR_OK) {
        GG_LOGW("Failed to notify service manager of readiness: %d", ret);
    }
    return ret;
}
