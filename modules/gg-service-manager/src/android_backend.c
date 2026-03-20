// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include "svcmgr_backend.h"
#include <service_manager.h>
#include <gg/error.h>
#include <gg/log.h>
#include <gg/types.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_CHILDREN 64
#define NAME_MAX_LEN 128
#define PATH_MAX_LEN 512
#define STOP_TIMEOUT_S 5
#define MAX_RESTART_COUNT 5
#define RESTART_WINDOW_S 60
#define RESTART_BACKOFF_BASE_S 1
#define RESTART_BACKOFF_MAX_S 30

typedef struct {
    char name[NAME_MAX_LEN];
    size_t name_len;
    char exec_path[PATH_MAX_LEN];
    pid_t pid;
    bool active;
    bool auto_restart;
    int restart_count;
    time_t first_crash_time;
} ChildEntry;

static ChildEntry children[MAX_CHILDREN];
static pthread_mutex_t children_mtx = PTHREAD_MUTEX_INITIALIZER;

static ChildEntry *find_child(GgBuffer name) {
    for (size_t i = 0; i < MAX_CHILDREN; i++) {
        if (children[i].active && children[i].name_len == name.len
            && memcmp(children[i].name, name.data, name.len) == 0) {
            return &children[i];
        }
    }
    return NULL;
}

static ChildEntry *alloc_child(void) {
    for (size_t i = 0; i < MAX_CHILDREN; i++) {
        if (!children[i].active) {
            return &children[i];
        }
    }
    return NULL;
}

static GgError android_start(GgBuffer name) {
    pthread_mutex_lock(&children_mtx);
    ChildEntry *entry = find_child(name);
    if (entry == NULL) {
        pthread_mutex_unlock(&children_mtx);
        GG_LOGE(
            "Cannot start unknown service %.*s",
            (int) name.len,
            (char *) name.data
        );
        return GG_ERR_NOENTRY;
    }

    if (entry->pid > 0) {
        pthread_mutex_unlock(&children_mtx);
        GG_LOGW(
            "Service %.*s already running (pid %d)",
            (int) name.len,
            (char *) name.data,
            entry->pid
        );
        return GG_ERR_OK;
    }

    pid_t pid = fork();
    if (pid < 0) {
        pthread_mutex_unlock(&children_mtx);
        GG_LOGE("fork failed: %d", errno);
        return GG_ERR_FAILURE;
    }

    if (pid == 0) {
        // Child: exec the daemon
        char *argv[] = { entry->exec_path, NULL };
        execve(entry->exec_path, argv, environ);
        _exit(127);
    }

    entry->pid = pid;
    entry->auto_restart = true;
    pthread_mutex_unlock(&children_mtx);

    GG_LOGI(
        "Started %.*s (pid %d)",
        (int) name.len,
        (char *) name.data,
        pid
    );
    return GG_ERR_OK;
}

static GgError android_stop(GgBuffer name) {
    pthread_mutex_lock(&children_mtx);
    ChildEntry *entry = find_child(name);
    if (entry == NULL || entry->pid <= 0) {
        pthread_mutex_unlock(&children_mtx);
        return GG_ERR_OK;
    }

    pid_t pid = entry->pid;
    entry->auto_restart = false;
    pthread_mutex_unlock(&children_mtx);

    kill(pid, SIGTERM);

    for (int i = 0; i < STOP_TIMEOUT_S * 10; i++) {
        int status;
        pid_t ret = waitpid(pid, &status, WNOHANG);
        if (ret == pid || ret < 0) {
            pthread_mutex_lock(&children_mtx);
            entry->pid = 0;
            pthread_mutex_unlock(&children_mtx);
            GG_LOGI(
                "Stopped %.*s", (int) name.len, (char *) name.data
            );
            return GG_ERR_OK;
        }
        usleep(100000); // 100ms
    }

    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);

    pthread_mutex_lock(&children_mtx);
    entry->pid = 0;
    pthread_mutex_unlock(&children_mtx);

    GG_LOGW(
        "Killed %.*s (SIGKILL)", (int) name.len, (char *) name.data
    );
    return GG_ERR_OK;
}

static GgError android_status(GgBuffer name, SvcMgrState *state) {
    pthread_mutex_lock(&children_mtx);
    ChildEntry *entry = find_child(name);
    if (entry == NULL) {
        pthread_mutex_unlock(&children_mtx);
        *state = SVCMGR_STATE_UNKNOWN;
        return GG_ERR_OK;
    }

    if (entry->pid <= 0) {
        *state = SVCMGR_STATE_STOPPED;
    } else {
        // Check if process is alive
        int ret = kill(entry->pid, 0);
        if (ret == 0) {
            *state = SVCMGR_STATE_RUNNING;
        } else {
            *state = SVCMGR_STATE_STOPPED;
            entry->pid = 0;
        }
    }

    pthread_mutex_unlock(&children_mtx);
    return GG_ERR_OK;
}

static GgError android_register_service(
    GgBuffer name, GgBuffer exec_path, GgBuffer *args, size_t args_len
) {
    (void) args;
    (void) args_len;

    if (name.len >= NAME_MAX_LEN || exec_path.len >= PATH_MAX_LEN) {
        return GG_ERR_RANGE;
    }

    pthread_mutex_lock(&children_mtx);
    ChildEntry *entry = find_child(name);
    if (entry == NULL) {
        entry = alloc_child();
    }
    if (entry == NULL) {
        pthread_mutex_unlock(&children_mtx);
        GG_LOGE("Child table full");
        return GG_ERR_NOMEM;
    }

    memcpy(entry->name, name.data, name.len);
    entry->name_len = name.len;
    memcpy(entry->exec_path, exec_path.data, exec_path.len);
    entry->exec_path[exec_path.len] = '\0';
    entry->active = true;
    entry->pid = 0;
    entry->auto_restart = false;
    entry->restart_count = 0;
    entry->first_crash_time = 0;
    pthread_mutex_unlock(&children_mtx);

    GG_LOGI(
        "Registered service %.*s -> %.*s",
        (int) name.len,
        (char *) name.data,
        (int) exec_path.len,
        (char *) exec_path.data
    );
    return GG_ERR_OK;
}

static GgError android_unregister_service(GgBuffer name) {
    android_stop(name);

    pthread_mutex_lock(&children_mtx);
    ChildEntry *entry = find_child(name);
    if (entry != NULL) {
        entry->active = false;
    }
    pthread_mutex_unlock(&children_mtx);
    return GG_ERR_OK;
}

static SvcMgrBackend android_backend = {
    .start = android_start,
    .stop = android_stop,
    .status = android_status,
    .register_service = android_register_service,
    .unregister_service = android_unregister_service,
};

// SIGCHLD reaper thread — auto-restarts crashed daemons
static void *reaper_thread(void *arg) {
    (void) arg;

    while (true) {
        int status;
        pid_t pid = waitpid(-1, &status, 0);
        if (pid <= 0) {
            sleep(1);
            continue;
        }

        pthread_mutex_lock(&children_mtx);
        for (size_t i = 0; i < MAX_CHILDREN; i++) {
            if (children[i].active && children[i].pid == pid) {
                int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                GG_LOGW(
                    "Service %.*s (pid %d) exited with code %d",
                    (int) children[i].name_len,
                    children[i].name,
                    pid,
                    code
                );
                children[i].pid = 0;

                if (children[i].auto_restart && code != 0) {
                    time_t now = time(NULL);
                    if (children[i].first_crash_time == 0
                        || (now - children[i].first_crash_time)
                            > RESTART_WINDOW_S) {
                        children[i].first_crash_time = now;
                        children[i].restart_count = 0;
                    }
                    children[i].restart_count++;

                    if (children[i].restart_count > MAX_RESTART_COUNT) {
                        GG_LOGE(
                            "Service %.*s crashed too many times, not "
                            "restarting",
                            (int) children[i].name_len,
                            children[i].name
                        );
                        children[i].auto_restart = false;
                    } else {
                        int delay = RESTART_BACKOFF_BASE_S
                            << (children[i].restart_count - 1);
                        if (delay > RESTART_BACKOFF_MAX_S) {
                            delay = RESTART_BACKOFF_MAX_S;
                        }
                        GG_LOGI(
                            "Restarting %.*s in %ds",
                            (int) children[i].name_len,
                            children[i].name,
                            delay
                        );
                        pthread_mutex_unlock(&children_mtx);
                        sleep((unsigned int) delay);
                        GgBuffer name = {
                            .data = (uint8_t *) children[i].name,
                            .len = children[i].name_len,
                        };
                        android_start(name);
                        goto next;
                    }
                }
                break;
            }
        }
        pthread_mutex_unlock(&children_mtx);
next:;
    }
    return NULL;
}

void svcmgr_init_android_backend(void) {
    svcmgr_set_backend(&android_backend);

    pthread_t tid;
    pthread_create(&tid, NULL, reaper_thread, NULL);
    pthread_detach(tid);
}
