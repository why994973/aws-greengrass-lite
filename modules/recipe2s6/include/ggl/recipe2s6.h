// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef GGL_RECIPE2S6_H
#define GGL_RECIPE2S6_H

#include <gg/arena.h>
#include <gg/error.h>
#include <gg/types.h>
#include <limits.h>

typedef struct {
    GgBuffer component_name;
    GgBuffer component_version;
    char recipe_runner_path[PATH_MAX];
    const char *user;
    const char *group;
    char root_dir[PATH_MAX];
    char service_dir[PATH_MAX];
    int root_path_fd;
} Recipe2S6Args;

/// Generate an s6 service directory from a component recipe.
///
/// Creates the service directory structure under service_dir:
///   {service_dir}/ggl.{ComponentName}/
///     run              - executable run script
///     notification-fd  - contains "3"
///     env/             - environment variables (envdir format)
///
/// @param[in] args Recipe2S6 arguments
/// @param[in] alloc allocator for recipe parsing
/// @param[out] recipe_obj parsed recipe object
/// @param[out] component_name component name from recipe
/// @return GG_ERR_OK on success
GgError recipe2s6_generate(
    Recipe2S6Args *args,
    GgArena *alloc,
    GgObject *recipe_obj,
    GgObject **component_name
);

#endif
