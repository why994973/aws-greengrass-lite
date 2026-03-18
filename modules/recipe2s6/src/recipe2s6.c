// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <fcntl.h>
#include <gg/buffer.h>
#include <gg/cleanup.h>
#include <gg/error.h>
#include <gg/file.h>
#include <gg/log.h>
#include <gg/map.h>
#include <gg/object.h>
#include <gg/vector.h>
#include <ggl/recipe.h>
#include <ggl/recipe2s6.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_PATH_LEN 4096
#define MAX_RUN_SCRIPT_LEN 4096

static GgError make_dir(const char *path) {
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        GG_LOGE("Failed to create directory: %s", path);
        return GG_ERR_FAILURE;
    }
    return GG_ERR_OK;
}

static GgError write_file(const char *path, GgBuffer content, mode_t mode) {
    int fd = -1;
    GgError ret
        = gg_file_open(gg_buffer_from_null_term((char *) path),
                       O_WRONLY | O_CREAT | O_TRUNC, mode, &fd);
    GG_CLEANUP(cleanup_close, fd);
    if (ret != GG_ERR_OK) {
        GG_LOGE("Failed to open file: %s", path);
        return ret;
    }
    return gg_file_write(fd, content);
}

static GgError generate_run_script(
    Recipe2S6Args *args,
    GgBuffer component_name,
    GgBuffer component_version,
    bool is_root,
    GgMap set_env,
    GgBuffer phase,
    char *svc_dir
) {
    static uint8_t script_buf[MAX_RUN_SCRIPT_LEN];
    GgByteVec script = { .buf = { .data = script_buf, .len = 0 },
                         .capacity = MAX_RUN_SCRIPT_LEN };

    GgError ret = gg_byte_vec_append(&script, GG_STR("#!/bin/bash\n"));
    gg_byte_vec_chain_append(
        &ret, &script, GG_STR("export GGL_ROOT_PATH=")
    );
    gg_byte_vec_chain_append(
        &ret, &script, gg_buffer_from_null_term(args->root_dir)
    );
    gg_byte_vec_chain_append(&ret, &script, GG_STR("\n"));
    gg_byte_vec_chain_append(
        &ret, &script,
        GG_STR("export AWS_GG_NUCLEUS_DOMAIN_SOCKET_FILEPATH_FOR_COMPONENT=")
    );
    gg_byte_vec_chain_append(
        &ret, &script, gg_buffer_from_null_term(args->root_dir)
    );
    gg_byte_vec_chain_append(&ret, &script, GG_STR("/gg-ipc.socket\n"));

    // Write setenv entries from recipe
    if (set_env.len > 0) {
        GG_MAP_FOREACH(kv, set_env) {
            if (gg_obj_type(*gg_kv_val(kv)) == GG_TYPE_BUF) {
                gg_byte_vec_chain_append(&ret, &script, GG_STR("export "));
                gg_byte_vec_chain_append(&ret, &script, gg_kv_key(*kv));
                gg_byte_vec_chain_append(&ret, &script, GG_STR("="));
                gg_byte_vec_chain_append(
                    &ret, &script, gg_obj_into_buf(*gg_kv_val(kv))
                );
                gg_byte_vec_chain_append(&ret, &script, GG_STR("\n"));
            }
        }
    }

    gg_byte_vec_chain_append(&ret, &script, GG_STR("exec 2>&1\n"));

    // Working directory
    gg_byte_vec_chain_append(&ret, &script, GG_STR("mkdir -p "));
    gg_byte_vec_chain_append(
        &ret, &script, gg_buffer_from_null_term(args->root_dir)
    );
    gg_byte_vec_chain_append(&ret, &script, GG_STR("/work/"));
    gg_byte_vec_chain_append(&ret, &script, component_name);
    gg_byte_vec_chain_append(&ret, &script, GG_STR("\ncd "));
    gg_byte_vec_chain_append(
        &ret, &script, gg_buffer_from_null_term(args->root_dir)
    );
    gg_byte_vec_chain_append(&ret, &script, GG_STR("/work/"));
    gg_byte_vec_chain_append(&ret, &script, component_name);
    gg_byte_vec_chain_append(&ret, &script, GG_STR("\n"));

    // Exec line: optionally drop privileges, then run recipe-runner
    if (!is_root && args->user != NULL) {
        gg_byte_vec_chain_append(
            &ret, &script, GG_STR("exec s6-setuidgid ")
        );
        gg_byte_vec_chain_append(
            &ret, &script, gg_buffer_from_null_term((char *) args->user)
        );
        gg_byte_vec_chain_append(&ret, &script, GG_STR(" "));
    } else {
        gg_byte_vec_chain_append(&ret, &script, GG_STR("exec "));
    }

    gg_byte_vec_chain_append(
        &ret, &script, gg_buffer_from_null_term(args->recipe_runner_path)
    );
    gg_byte_vec_chain_append(&ret, &script, GG_STR(" -n "));
    gg_byte_vec_chain_append(&ret, &script, component_name);
    gg_byte_vec_chain_append(&ret, &script, GG_STR(" -v "));
    gg_byte_vec_chain_append(&ret, &script, component_version);
    gg_byte_vec_chain_append(&ret, &script, GG_STR(" -p "));
    gg_byte_vec_chain_append(&ret, &script, phase);
    gg_byte_vec_chain_append(&ret, &script, GG_STR("\n"));

    if (ret != GG_ERR_OK) {
        GG_LOGE("Failed to build run script");
        return ret;
    }

    char run_path[MAX_PATH_LEN];
    snprintf(run_path, sizeof(run_path), "%s/run", svc_dir);
    return write_file(run_path, script.buf, 0755);
}

static GgError generate_notification_fd(char *svc_dir) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/notification-fd", svc_dir);
    return write_file(path, GG_STR("3\n"), 0644);
}

GgError recipe2s6_generate(
    Recipe2S6Args *args,
    GgArena *alloc,
    GgObject *recipe_obj,
    GgObject **component_name
) {
    *component_name = NULL;

    GgError ret = ggl_recipe_get_from_file(
        args->root_path_fd,
        args->component_name,
        args->component_version,
        alloc,
        recipe_obj
    );
    if (ret != GG_ERR_OK) {
        GG_LOGE("No recipe found");
        return ret;
    }

    GgMap recipe_map = gg_obj_into_map(*recipe_obj);

    if (!gg_map_get(recipe_map, GG_STR("ComponentName"), component_name)) {
        GG_LOGE("ComponentName not found in recipe");
        return GG_ERR_INVALID;
    }
    if (gg_obj_type(**component_name) != GG_TYPE_BUF) {
        return GG_ERR_INVALID;
    }

    GgBuffer comp_name = gg_obj_into_buf(**component_name);

    GgObject *version_obj;
    if (!gg_map_get(recipe_map, GG_STR("ComponentVersion"), &version_obj)) {
        GG_LOGE("ComponentVersion not found in recipe");
        return GG_ERR_INVALID;
    }
    GgBuffer comp_version = gg_obj_into_buf(*version_obj);

    // Build service directory path: {service_dir}/ggl.{ComponentName}
    char svc_dir[MAX_PATH_LEN];
    snprintf(
        svc_dir,
        sizeof(svc_dir),
        "%s/ggl.%.*s",
        args->service_dir,
        (int) comp_name.len,
        comp_name.data
    );

    ret = make_dir(svc_dir);
    if (ret != GG_ERR_OK) {
        return ret;
    }

    // Select lifecycle and extract script
    GgMap selected_lifecycle = { 0 };
    ret = select_linux_lifecycle(recipe_map, &selected_lifecycle);
    if (ret != GG_ERR_OK) {
        GG_LOGE("Failed to select lifecycle");
        return ret;
    }

    // Try run phase first, then startup
    bool is_root = false;
    GgBuffer selected_script = { 0 };
    GgMap set_env = { 0 };
    GgBuffer timeout = { 0 };

    GgBuffer phase = GG_STR("run");
    ret = fetch_script_section(
        selected_lifecycle, phase, &is_root, &selected_script, &set_env,
        &timeout
    );
    if (ret == GG_ERR_NOENTRY) {
        phase = GG_STR("startup");
        ret = fetch_script_section(
            selected_lifecycle, phase, &is_root, &selected_script, &set_env,
            &timeout
        );
    }
    if (ret != GG_ERR_OK) {
        GG_LOGE("No run or startup phase found in recipe");
        return ret;
    }

    ret = generate_run_script(
        args, comp_name, comp_version, is_root, set_env, phase, svc_dir
    );
    if (ret != GG_ERR_OK) {
        return ret;
    }

    ret = generate_notification_fd(svc_dir);
    if (ret != GG_ERR_OK) {
        return ret;
    }

    GG_LOGI(
        "Generated s6 service directory for %.*s at %s",
        (int) comp_name.len,
        comp_name.data,
        svc_dir
    );

    return GG_ERR_OK;
}
