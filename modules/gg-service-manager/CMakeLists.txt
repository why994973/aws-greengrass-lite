# aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0

if(NOT GG_USE_SYSTEMD)
  ggl_init_module(
    gg-service-manager
    LIBS gg-sdk
         ggl-common
         core-bus
         ggl-socket-server
         ggl-process)

  # ggl_init_module globs src/*.c which picks up both backends.
  # Remove the one we don't want.
  if(GG_PLATFORM STREQUAL "ANDROID")
    get_target_property(_srcs gg-service-manager SOURCES)
    list(FILTER _srcs EXCLUDE REGEX "s6_backend\\.c$")
    set_target_properties(gg-service-manager PROPERTIES SOURCES "${_srcs}")
  else()
    get_target_property(_srcs gg-service-manager SOURCES)
    list(FILTER _srcs EXCLUDE REGEX "android_backend\\.c$")
    set_target_properties(gg-service-manager PROPERTIES SOURCES "${_srcs}")
  endif()
endif()
