// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include "svcmgr_backend.h"
#include <service_manager.h>
#include <gg/buffer.h>
#include <gg/error.h>
#include <gg/log.h>
#include <gg/map.h>
#include <gg/object.h>
#include <gg/types.h>
#include <ggl/core_bus/server.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define SVCMGR_MAX_SERVICES 64
#define SVCMGR_NAME_MAX 128

typedef struct {
    char name[SVCMGR_NAME_MAX];
    size_t name_len;
    SvcMgrState state;
    bool in_use;
} ServiceEntry;

static ServiceEntry service_table[SVCMGR_MAX_SERVICES];
static pthread_mutex_t table_mtx = PTHREAD_MUTEX_INITIALIZER;

static SvcMgrBackend *active_backend;

GgBuffer svcmgr_state_to_str(SvcMgrState state) {
    switch (state) {
    case SVCMGR_STATE_RUNNING:
        return GG_STR("RUNNING");
    case SVCMGR_STATE_STARTING:
        return GG_STR("STARTING");
    case SVCMGR_STATE_STOPPED:
        return GG_STR("STOPPED");
    case SVCMGR_STATE_ERRORED:
        return GG_STR("ERRORED");
    default:
        return GG_STR("UNKNOWN");
    }
}

static ServiceEntry *find_service(GgBuffer name) {
    for (size_t i = 0; i < SVCMGR_MAX_SERVICES; i++) {
        if (service_table[i].in_use && service_table[i].name_len == name.len
            && memcmp(service_table[i].name, name.data, name.len) == 0) {
            return &service_table[i];
        }
    }
    return NULL;
}

static ServiceEntry *alloc_service(GgBuffer name) {
    for (size_t i = 0; i < SVCMGR_MAX_SERVICES; i++) {
        if (!service_table[i].in_use) {
            memcpy(service_table[i].name, name.data, name.len);
            service_table[i].name_len = name.len;
            service_table[i].state = SVCMGR_STATE_STOPPED;
            service_table[i].in_use = true;
            return &service_table[i];
        }
    }
    return NULL;
}

static GgError handle_start(void *ctx, GgMap params, uint32_t handle) {
    (void) ctx;
    GgObject *name_obj;
    GgError ret = gg_map_validate(
        params,
        GG_MAP_SCHEMA(
            { GG_STR("name"), GG_REQUIRED, GG_TYPE_BUF, &name_obj }
        )
    );
    if (ret != GG_ERR_OK) {
        return GG_ERR_INVALID;
    }
    GgBuffer name = gg_obj_into_buf(*name_obj);

    if (active_backend == NULL || active_backend->start == NULL) {
        return GG_ERR_UNSUPPORTED;
    }

    ret = active_backend->start(name);
    if (ret != GG_ERR_OK) {
        return ret;
    }

    pthread_mutex_lock(&table_mtx);
    ServiceEntry *entry = find_service(name);
    if (entry != NULL) {
        entry->state = SVCMGR_STATE_STARTING;
    }
    pthread_mutex_unlock(&table_mtx);

    GG_LOGI(
        "Started service %.*s", (int) name.len, (char *) name.data
    );
    ggl_respond(handle, GG_OBJ_NULL);
    return GG_ERR_OK;
}

static GgError handle_stop(void *ctx, GgMap params, uint32_t handle) {
    (void) ctx;
    GgObject *name_obj;
    GgError ret = gg_map_validate(
        params,
        GG_MAP_SCHEMA(
            { GG_STR("name"), GG_REQUIRED, GG_TYPE_BUF, &name_obj }
        )
    );
    if (ret != GG_ERR_OK) {
        return GG_ERR_INVALID;
    }
    GgBuffer name = gg_obj_into_buf(*name_obj);

    if (active_backend == NULL || active_backend->stop == NULL) {
        return GG_ERR_UNSUPPORTED;
    }

    ret = active_backend->stop(name);
    if (ret != GG_ERR_OK) {
        return ret;
    }

    pthread_mutex_lock(&table_mtx);
    ServiceEntry *entry = find_service(name);
    if (entry != NULL) {
        entry->state = SVCMGR_STATE_STOPPED;
    }
    pthread_mutex_unlock(&table_mtx);

    GG_LOGI(
        "Stopped service %.*s", (int) name.len, (char *) name.data
    );
    ggl_respond(handle, GG_OBJ_NULL);
    return GG_ERR_OK;
}

static GgError handle_status(void *ctx, GgMap params, uint32_t handle) {
    (void) ctx;
    GgObject *name_obj;
    GgError ret = gg_map_validate(
        params,
        GG_MAP_SCHEMA(
            { GG_STR("name"), GG_REQUIRED, GG_TYPE_BUF, &name_obj }
        )
    );
    if (ret != GG_ERR_OK) {
        return GG_ERR_INVALID;
    }
    GgBuffer name = gg_obj_into_buf(*name_obj);

    SvcMgrState state = SVCMGR_STATE_UNKNOWN;

    // Try backend first for live status
    if (active_backend != NULL && active_backend->status != NULL) {
        ret = active_backend->status(name, &state);
        if (ret == GG_ERR_OK) {
            // Update table with live state
            pthread_mutex_lock(&table_mtx);
            ServiceEntry *entry = find_service(name);
            if (entry != NULL) {
                entry->state = state;
            }
            pthread_mutex_unlock(&table_mtx);
        }
    }

    // Fall back to table if backend didn't provide state
    if (state == SVCMGR_STATE_UNKNOWN) {
        pthread_mutex_lock(&table_mtx);
        ServiceEntry *entry = find_service(name);
        if (entry != NULL) {
            state = entry->state;
        }
        pthread_mutex_unlock(&table_mtx);
    }

    GgBuffer state_str = svcmgr_state_to_str(state);
    ggl_respond(
        handle,
        gg_obj_map(GG_MAP(
            gg_kv(GG_STR("name"), gg_obj_buf(name)),
            gg_kv(GG_STR("state"), gg_obj_buf(state_str))
        ))
    );
    return GG_ERR_OK;
}

static GgError handle_register(void *ctx, GgMap params, uint32_t handle) {
    (void) ctx;
    GgObject *name_obj;
    GgObject *exec_obj;
    GgError ret = gg_map_validate(
        params,
        GG_MAP_SCHEMA(
            { GG_STR("name"), GG_REQUIRED, GG_TYPE_BUF, &name_obj },
            { GG_STR("exec"), GG_REQUIRED, GG_TYPE_BUF, &exec_obj },
        )
    );
    if (ret != GG_ERR_OK) {
        return GG_ERR_INVALID;
    }
    GgBuffer name = gg_obj_into_buf(*name_obj);
    GgBuffer exec_path = gg_obj_into_buf(*exec_obj);

    if (name.len >= SVCMGR_NAME_MAX) {
        return GG_ERR_RANGE;
    }

    if (active_backend != NULL && active_backend->register_service != NULL) {
        ret = active_backend->register_service(name, exec_path, NULL, 0);
        if (ret != GG_ERR_OK) {
            return ret;
        }
    }

    pthread_mutex_lock(&table_mtx);
    ServiceEntry *entry = find_service(name);
    if (entry == NULL) {
        entry = alloc_service(name);
    }
    pthread_mutex_unlock(&table_mtx);

    if (entry == NULL) {
        GG_LOGE("Service table full.");
        return GG_ERR_NOMEM;
    }

    GG_LOGI(
        "Registered service %.*s", (int) name.len, (char *) name.data
    );
    ggl_respond(handle, GG_OBJ_NULL);
    return GG_ERR_OK;
}

static GgError handle_notify_ready(void *ctx, GgMap params, uint32_t handle) {
    (void) ctx;
    GgObject *name_obj;
    GgError ret = gg_map_validate(
        params,
        GG_MAP_SCHEMA(
            { GG_STR("name"), GG_REQUIRED, GG_TYPE_BUF, &name_obj }
        )
    );
    if (ret != GG_ERR_OK) {
        return GG_ERR_INVALID;
    }
    GgBuffer name = gg_obj_into_buf(*name_obj);

    pthread_mutex_lock(&table_mtx);
    ServiceEntry *entry = find_service(name);
    if (entry == NULL) {
        entry = alloc_service(name);
    }
    if (entry != NULL) {
        entry->state = SVCMGR_STATE_RUNNING;
    }
    pthread_mutex_unlock(&table_mtx);

    GG_LOGI(
        "Service %.*s is ready", (int) name.len, (char *) name.data
    );
    ggl_respond(handle, GG_OBJ_NULL);
    return GG_ERR_OK;
}

static GgError handle_list(void *ctx, GgMap params, uint32_t handle) {
    (void) ctx;
    (void) params;

    // Return count of registered services for simplicity
    size_t count = 0;
    pthread_mutex_lock(&table_mtx);
    for (size_t i = 0; i < SVCMGR_MAX_SERVICES; i++) {
        if (service_table[i].in_use) {
            count++;
        }
    }
    pthread_mutex_unlock(&table_mtx);

    ggl_respond(handle, gg_obj_i64((int64_t) count));
    return GG_ERR_OK;
}

void svcmgr_set_backend(SvcMgrBackend *backend) {
    active_backend = backend;
}

GgError run_gg_service_manager(void) {
    GG_LOGI("Service manager starting.");

    static GglRpcMethodDesc handlers[] = {
        { GG_STR("start"), false, handle_start, NULL },
        { GG_STR("stop"), false, handle_stop, NULL },
        { GG_STR("status"), false, handle_status, NULL },
        { GG_STR("register"), false, handle_register, NULL },
        { GG_STR("notify_ready"), false, handle_notify_ready, NULL },
        { GG_STR("list"), false, handle_list, NULL },
    };
    static const size_t HANDLERS_LEN = sizeof(handlers) / sizeof(handlers[0]);

    GgError ret = ggl_listen(
        GG_STR("gg_service_manager"), handlers, HANDLERS_LEN
    );
    GG_LOGE("Exiting with error %u.", (unsigned) ret);
    return GG_ERR_FAILURE;
}
