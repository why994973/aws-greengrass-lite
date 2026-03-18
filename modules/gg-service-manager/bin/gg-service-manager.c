// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include <service_manager.h>
#include <gg/error.h>
#include <ggl/nucleus/init.h>

int main(void) {
    ggl_nucleus_init();
    svcmgr_init_s6_backend();
    GgError ret = run_gg_service_manager();
    if (ret != GG_ERR_OK) {
        return 1;
    }
}
