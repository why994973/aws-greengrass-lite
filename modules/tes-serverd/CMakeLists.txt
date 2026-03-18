# aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0

if(GG_USE_SYSTEMD)
  set(TES_SERVERD_PLATFORM_LIBS PkgConfig::libsystemd)
else()
  set(TES_SERVERD_PLATFORM_LIBS)
endif()

ggl_init_module(
  tes-serverd
  LIBS gg-sdk
       ggl-common
       ggl-http
       core-bus
       core-bus-gg-config
       PkgConfig::libevent
       ${TES_SERVERD_PLATFORM_LIBS})
