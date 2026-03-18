// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include "svcmgr_backend.h"
#include <service_manager.h>
#include <gg/error.h>
#include <gg/log.h>
#include <gg/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define S6_SVC_DIR "/run/service/"
#define S6_CMD_BUF_SIZE 512

static GgError s6_start(GgBuffer name) {
    char cmd[S6_CMD_BUF_SIZE];
    int ret = snprintf(
        cmd, sizeof(cmd), "s6-svc -u " S6_SVC_DIR "%.*s",
        (int) name.len, (char *) name.data
    );
    if (ret < 0 || (size_t) ret >= sizeof(cmd)) {
        return GG_ERR_RANGE;
    }
    GG_LOGD("s6 backend: %s", cmd);
    return system(cmd) == 0 ? GG_ERR_OK : GG_ERR_FAILURE;
}

static GgError s6_stop(GgBuffer name) {
    char cmd[S6_CMD_BUF_SIZE];
    int ret = snprintf(
        cmd, sizeof(cmd), "s6-svc -d " S6_SVC_DIR "%.*s",
        (int) name.len, (char *) name.data
    );
    if (ret < 0 || (size_t) ret >= sizeof(cmd)) {
        return GG_ERR_RANGE;
    }
    GG_LOGD("s6 backend: %s", cmd);
    return system(cmd) == 0 ? GG_ERR_OK : GG_ERR_FAILURE;
}

static GgError s6_status(GgBuffer name, SvcMgrState *state) {
    char cmd[S6_CMD_BUF_SIZE];
    int ret = snprintf(
        cmd, sizeof(cmd),
        "s6-svstat -o up,ready " S6_SVC_DIR "%.*s 2>/dev/null",
        (int) name.len, (char *) name.data
    );
    if (ret < 0 || (size_t) ret >= sizeof(cmd)) {
        return GG_ERR_RANGE;
    }

    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        *state = SVCMGR_STATE_UNKNOWN;
        return GG_ERR_FAILURE;
    }

    char output[64] = { 0 };
    if (fgets(output, sizeof(output), fp) == NULL) {
        pclose(fp);
        *state = SVCMGR_STATE_UNKNOWN;
        return GG_ERR_OK;
    }
    int exit_status = pclose(fp);

    if (exit_status != 0) {
        *state = SVCMGR_STATE_STOPPED;
        return GG_ERR_OK;
    }

    // s6-svstat -o up,ready outputs "true true", "true false", "false false"
    if (strncmp(output, "true true", 9) == 0) {
        *state = SVCMGR_STATE_RUNNING;
    } else if (strncmp(output, "true false", 10) == 0) {
        *state = SVCMGR_STATE_STARTING;
    } else {
        *state = SVCMGR_STATE_STOPPED;
    }

    return GG_ERR_OK;
}

static GgError s6_register_service(
    GgBuffer name, GgBuffer exec_path, GgBuffer *args, size_t args_len
) {
    (void) args;
    (void) args_len;

    char svc_dir[S6_CMD_BUF_SIZE];
    int ret = snprintf(
        svc_dir, sizeof(svc_dir), S6_SVC_DIR "%.*s",
        (int) name.len, (char *) name.data
    );
    if (ret < 0 || (size_t) ret >= sizeof(svc_dir)) {
        return GG_ERR_RANGE;
    }

    // Create service directory
    if (mkdir(svc_dir, 0755) != 0) {
        GG_LOGW("Service dir may already exist: %s", svc_dir);
    }

    // Write run script
    char run_path[S6_CMD_BUF_SIZE];
    snprintf(run_path, sizeof(run_path), "%s/run", svc_dir);
    FILE *fp = fopen(run_path, "w");
    if (fp == NULL) {
        return GG_ERR_FAILURE;
    }
    fprintf(
        fp,
        "#!/bin/bash\nexec 2>&1\nexec %.*s\n",
        (int) exec_path.len, (char *) exec_path.data
    );
    fclose(fp);
    chmod(run_path, 0755);

    // Write notification-fd
    char nfd_path[S6_CMD_BUF_SIZE];
    snprintf(nfd_path, sizeof(nfd_path), "%s/notification-fd", svc_dir);
    fp = fopen(nfd_path, "w");
    if (fp == NULL) {
        return GG_ERR_FAILURE;
    }
    fprintf(fp, "3\n");
    fclose(fp);

    // Signal s6-svscan to rescan
    (void) !system("s6-svscanctl -a " S6_SVC_DIR);

    GG_LOGI(
        "Registered s6 service %.*s", (int) name.len, (char *) name.data
    );
    return GG_ERR_OK;
}

static GgError s6_unregister_service(GgBuffer name) {
    // Stop first, then remove directory
    (void) s6_stop(name);

    char cmd[S6_CMD_BUF_SIZE];
    int ret = snprintf(
        cmd, sizeof(cmd), "rm -rf " S6_SVC_DIR "%.*s",
        (int) name.len, (char *) name.data
    );
    if (ret < 0 || (size_t) ret >= sizeof(cmd)) {
        return GG_ERR_RANGE;
    }
    (void) !system(cmd);
    return GG_ERR_OK;
}

SvcMgrBackend s6_backend = {
    .start = s6_start,
    .stop = s6_stop,
    .status = s6_status,
    .register_service = s6_register_service,
    .unregister_service = s6_unregister_service,
};

void svcmgr_init_s6_backend(void) {
    svcmgr_set_backend(&s6_backend);
}
