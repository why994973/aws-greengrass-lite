# aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0

if(GG_USE_SYSTEMD)
  set(GGHEALTHD_PLATFORM_LIBS PkgConfig::libsystemd)
else()
  set(GGHEALTHD_PLATFORM_LIBS svcmgr-client)
endif()

ggl_init_module(
  gghealthd
  LIBS gg-sdk
       ggl-common
       ggl-process
       ggl-constants
       core-bus
       ggl-socket-server
       core-bus-gg-config
       ${GGHEALTHD_PLATFORM_LIBS})
