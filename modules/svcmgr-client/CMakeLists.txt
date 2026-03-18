# aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0

if(NOT GG_USE_SYSTEMD)
  ggl_init_module(
    svcmgr-client
    LIBS gg-sdk
         ggl-common
         core-bus)
endif()
