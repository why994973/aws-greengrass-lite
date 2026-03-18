# aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0

if(GG_USE_SYSTEMD)
  set(GGIPC_AUTH_PLATFORM_LIBS PkgConfig::libsystemd)
else()
  set(GGIPC_AUTH_PLATFORM_LIBS)
endif()

ggl_init_module(ggipc-auth LIBS gg-sdk ${GGIPC_AUTH_PLATFORM_LIBS})
