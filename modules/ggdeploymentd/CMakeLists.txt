# aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0

if(GG_USE_SYSTEMD)
  set(GGDEPLOYMENTD_PLATFORM_LIBS PkgConfig::libsystemd)
else()
  set(GGDEPLOYMENTD_PLATFORM_LIBS svcmgr-client)
endif()

ggl_init_module(
  ggdeploymentd
  LIBS gg-sdk
       ggl-constants
       ggl-common
       core-bus
       ggl-docker-client
       ggl-http
       ggl-uri
       ggl-json
       ggl-proxy-environment
       ggl-recipe
       ggl-semver
       ggl-yaml
       ggl-zip
       ggl-process
       aws-iot-call
       core-bus-gghealthd
       core-bus-gg-config
       core-bus-aws-iot-mqtt
       core-bus-sub-response
       recipe2unit
       PkgConfig::uuid
       ${GGDEPLOYMENTD_PLATFORM_LIBS})
