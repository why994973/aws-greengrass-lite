# aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)
project(aws-greengrass-lite C ASM)

#
# Options
#

option(BUILD_TESTING "Build tests")
option(BUILD_EXAMPLES "Build examples")

option(ENABLE_WERROR "Compile warnings as errors")

set(GG_LOG_LEVEL CACHE STRING "GG log level")
# Backwards compat
if(GGL_LOG_LEVEL AND NOT GG_LOG_LEVEL)
  set(GG_LOG_LEVEL
      "${GGL_LOG_LEVEL}"
      CACHE STRING "" FORCE)
endif()

option(GG_USE_SYSTEMD "Use systemd for process management" ON)

set(GG_PLATFORM
    "LINUX"
    CACHE STRING "Target platform: LINUX, ANDROID")

if(GG_PLATFORM STREQUAL "ANDROID")
  set(GG_USE_SYSTEMD
      OFF
      CACHE BOOL "" FORCE)
  add_compile_definitions(GG_PLATFORM_ANDROID=1)
else()
  add_compile_definitions(GG_PLATFORM_ANDROID=0)
endif()

option(GGL_SYSTEMD_SERVICE_BUILD "Install GGL as a set of systemd services"
       TRUE)
set(GGL_SYSTEMD_SYSTEM_DIR
    "lib/systemd/system"
    CACHE STRING "Install directory for systemd unit files")
set(GGL_SYSTEMD_SYSTEM_USER
    "ggcore"
    CACHE STRING "Core service user")
set(GGL_SYSTEMD_SYSTEM_GROUP
    "ggcore"
    CACHE STRING "Core service group")

#
# Misc
#

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(BUILD_TESTING)
  include(CTest)
endif()

include(GNUInstallDirs)

# Put outputs in build/bin and build/lib
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")

# Fix for CMake stripping pkg-config includes from compile command
unset(CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES)
#
# Compiler options
#

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(WARNING "CMAKE_BUILD_TYPE not set, using MinSizeRel.")
  set(CMAKE_BUILD_TYPE MinSizeRel)
endif()

# Enable a compiler option for compile and link
macro(add_cflags)
  add_compile_options(${ARGN})
  add_link_options(${ARGN})
endmacro()

include(CheckCCompilerFlag)
include(CheckLinkerFlag)

# Enable a compiler/linker option if supported with a condition
macro(try_add_cflag_if name cond option)
  check_c_compiler_flag("${option}" compiler_has_${name})
  if(compiler_has_${name})
    add_cflags("$<${cond}:${option}>")
  endif()
endmacro()

# Enable a compiler/linker option if supported
macro(try_add_cflag name option)
  try_add_cflag_if(${name} 1 ${option})
endmacro()

# Enable a linker option if supported with a condition
macro(try_add_link_option_if name cond option)
  check_linker_flag(C "${option}" linker_has_${name})
  if(linker_has_${name})
    add_link_options("$<${cond}:${option}>")
  endif()
endmacro()

# Enable a linker option if supported
macro(try_add_link_option name option)
  try_add_link_option_if(${name} 1 ${option})
endmacro()

# Clear CMake defaults
set(CMAKE_C_FLAGS_DEBUG "")
set(CMAKE_C_FLAGS_RELEASE "")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "")
set(CMAKE_C_FLAGS_MINSIZEREL "")

set(CMAKE_COLOR_DIAGNOSTICS ON)

add_cflags($<$<CONFIG:Debug>:-O0>)
add_cflags($<$<CONFIG:Release,RelWithDebInfo>:-O3>)
try_add_cflag_if(Oz $<CONFIG:MinSizeRel> -Oz)
if(NOT compiler_has_Oz)
  add_cflags($<$<CONFIG:MinSizeRel>:-Os>)
endif()

add_cflags($<$<CONFIG:Debug,RelWithDebInfo>:-ggdb3>)

add_cflags($<$<NOT:$<CONFIG:Debug>>:-fPIE>)

# Following flags are required
add_cflags(-pthread -fno-strict-aliasing)

# Following flags are optional
add_cflags(-std=gnu11 -fvisibility=hidden -fno-semantic-interposition
           -fno-common)

# This stops GCC from generating unstrippable inline DWARF debug info
add_cflags(-fno-unwind-tables -fno-asynchronous-unwind-tables)

try_add_cflag(stack-protector-strong -fstack-protector-strong)
try_add_cflag(strict-flex-arrays -fstrict-flex-arrays=3)
try_add_cflag(zero-call-used-regs -fzero-call-used-regs=used-gpr)
try_add_cflag(stack-clash-protection -fstack-clash-protection)
try_add_cflag(cf-protection -fcf-protection=full)
try_add_cflag(branch-protection -mbranch-protection=standard)

try_add_cflag(macro-prefix-map "-fmacro-prefix-map=${CMAKE_CURRENT_LIST_DIR}/=")

add_cflags($<$<CONFIG:Debug>:-fno-omit-frame-pointer>)
try_add_cflag_if(trivial-auto-var-init-pattern $<CONFIG:Debug>
                 -ftrivial-auto-var-init=pattern)

add_cflags($<$<CONFIG:Debug>:-fsanitize=undefined,address>)

add_cflags($<$<NOT:$<CONFIG:Debug>>:-fno-delete-null-pointer-checks>)
add_cflags($<$<NOT:$<CONFIG:Debug>>:-fwrapv>)
try_add_cflag_if(trivial-auto-var-init-zero $<NOT:$<CONFIG:Debug>>
                 -ftrivial-auto-var-init=zero)
try_add_cflag_if(zero-init-padding-bits-all $<NOT:$<CONFIG:Debug>>
                 -fzero-init-padding-bits=all)

try_add_cflag_if(function-sections $<NOT:$<CONFIG:Debug>> -ffunction-sections)
try_add_cflag_if(data-sections $<NOT:$<CONFIG:Debug>> -fdata-sections)

if(ENABLE_WERROR)
  add_cflags(-Werror)
endif()

add_cflags(
  -Wall
  -Wextra
  -Wwrite-strings
  -Wno-missing-braces
  -Wvla
  -Wshadow
  -Wformat
  -Wformat=2
  -Wmissing-prototypes
  -Wstrict-prototypes
  -Wold-style-definition
  -Wunused
  -Wundef
  -Wconversion
  -Wsign-conversion
  -Wimplicit-fallthrough
  -Wredundant-decls
  -Wdate-time
  -Wstack-protector)

try_add_cflag(Wenum-int-mismatch -Wenum-int-mismatch)
try_add_cflag(Wtrampolines -Wtrampolines)
try_add_cflag(Wbidi-chars -Wbidi-chars=any,ucn)

add_cflags(-Werror=format-security -Werror=implicit
           -Werror=incompatible-pointer-types -Werror=int-conversion)

add_compile_definitions(_GNU_SOURCE)
# build ggl-http with obsoleted cURL code removed
add_compile_definitions(CURL_NO_OLDIES)

add_compile_definitions($<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=3>)

file(READ version GGL_VERSION)
string(STRIP "${GGL_VERSION}" GGL_VERSION)

option(GGL_RELEASE "Build as release version without git hash" OFF)

if(NOT GGL_RELEASE)
  execute_process(
    COMMAND git rev-parse --short=7 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(GIT_COMMIT)
    set(GGL_VERSION "${GGL_VERSION}-${GIT_COMMIT}")
  endif()
endif()

add_compile_definitions("GGL_VERSION=\"${GGL_VERSION}\"")

add_link_options($<$<NOT:$<CONFIG:Debug>>:-pie>)

add_link_options(
  LINKER:-z,relro,-z,now,-z,noexecstack,-z,nodlopen LINKER:--as-needed
  LINKER:--no-copy-dt-needed-entries LINKER:--enable-new-dtags,--hash-style=gnu)
add_link_options($<$<CONFIG:Debug,RelWithDebInfo>:LINKER:--build-id>)
try_add_link_option(compress-debug-sections-zlib
                    LINKER:--compress-debug-sections=zlib)

# TODO: investigate misaligned pointers when this is enabled for 32-bit arm
# try_add_link_option_if(gc-sections $<NOT:$<CONFIG:Debug>>
# LINKER:--gc-sections)

try_add_link_option_if(strip-all $<CONFIG:Release,MinSizeRel> LINKER:-s)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG FALSE)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

# Additional optimization flags that GCC disables due to compile time cost
try_add_cflag_if(fipa-pta $<NOT:$<CONFIG:Debug>> -fipa-pta)
try_add_cflag_if(fdevirtualize-at-ltrans $<NOT:$<CONFIG:Debug>>
                 -fdevirtualize-at-ltrans)

set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "\$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")

set(ENV{SOURCE_DATE_EPOCH} "0")
set(ENV{ZERO_AR_DATE} "1")

#
# FetchContent deps
#

set(FETCHCONTENT_QUIET FALSE)
include(FetchContent)

file(READ fc_deps.json FC_DEPS_JSON)
string(JSON FC_DEPS_COUNT LENGTH "${FC_DEPS_JSON}")
math(EXPR FC_DEPS_INDEX_MAX "${FC_DEPS_COUNT} - 1")

foreach(index RANGE ${FC_DEPS_INDEX_MAX})
  string(JSON dep_name MEMBER "${FC_DEPS_JSON}" ${index})
  string(JSON dep_url GET "${FC_DEPS_JSON}" "${dep_name}" url)
  string(JSON dep_rev GET "${FC_DEPS_JSON}" "${dep_name}" rev)
  fetchcontent_declare(
    "${dep_name}"
    GIT_REPOSITORY "${dep_url}"
    GIT_TAG "${dep_rev}"
    SOURCE_SUBDIR nonexistent_dir)
  fetchcontent_makeavailable(${dep_name})
endforeach()

#
# System deps
#
if(GG_PLATFORM STREQUAL "ANDROID")
  # Brazil-provided deps from LIB_BUILDROOT — use static .a files
  find_library(OPENSSL_SSL_LIBRARY libssl.a PATHS "${LIB_BUILDROOT}/lib" NO_DEFAULT_PATH REQUIRED)
  find_library(OPENSSL_CRYPTO_LIBRARY libcrypto.a PATHS "${LIB_BUILDROOT}/lib" NO_DEFAULT_PATH REQUIRED)
  find_path(OPENSSL_INCLUDE_DIR openssl/ssl.h PATHS "${LIB_BUILDROOT}/include" NO_DEFAULT_PATH REQUIRED)
  add_library(PkgConfig::openssl INTERFACE IMPORTED)
  target_link_libraries(PkgConfig::openssl INTERFACE ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
  target_include_directories(PkgConfig::openssl INTERFACE ${OPENSSL_INCLUDE_DIR})

  find_library(CURL_LIBRARY libcurl.a PATHS "${LIB_BUILDROOT}/lib" NO_DEFAULT_PATH REQUIRED)
  find_path(CURL_INCLUDE_DIR curl/curl.h PATHS "${LIB_BUILDROOT}/include" NO_DEFAULT_PATH REQUIRED)
  add_library(PkgConfig::libcurl INTERFACE IMPORTED)
  target_link_libraries(PkgConfig::libcurl INTERFACE ${CURL_LIBRARY} PkgConfig::openssl)
  target_include_directories(PkgConfig::libcurl INTERFACE ${CURL_INCLUDE_DIR})

  find_library(ZLIB_LIBRARY libz.a PATHS "${LIB_BUILDROOT}/lib" NO_DEFAULT_PATH REQUIRED)
  find_path(ZLIB_INCLUDE_DIR zlib.h PATHS "${LIB_BUILDROOT}/include" NO_DEFAULT_PATH REQUIRED)
  add_library(PkgConfig::zlib INTERFACE IMPORTED)
  target_link_libraries(PkgConfig::zlib INTERFACE ${ZLIB_LIBRARY})
  target_include_directories(PkgConfig::zlib INTERFACE ${ZLIB_INCLUDE_DIR})
  # Ensure zlib is available globally for transitive deps (libzip, curl)
  link_libraries(${ZLIB_LIBRARY})

  # c-ares (curl dependency)
  find_library(CARES_LIBRARY libcares.a PATHS "${LIB_BUILDROOT}/lib" NO_DEFAULT_PATH)
  if(CARES_LIBRARY)
    target_link_libraries(PkgConfig::libcurl INTERFACE ${CARES_LIBRARY})
  endif()

  # FetchContent for deps without IhmDevice packages — using local sources
  include(FetchContent)

  # sqlite3 - bundled amalgamation (no network needed)
  add_library(sqlite3_lib STATIC ${CMAKE_CURRENT_SOURCE_DIR}/compat/sqlite3.c)
  target_include_directories(sqlite3_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/compat)
  target_compile_options(sqlite3_lib PRIVATE -w)
  add_library(PkgConfig::sqlite3 ALIAS sqlite3_lib)

  FetchContent_Declare(yaml_src SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/compat/libyaml)
  FetchContent_MakeAvailable(yaml_src)
  add_library(PkgConfig::yaml ALIAS yaml)

  set(EVENT__DISABLE_TESTS ON CACHE BOOL "" FORCE)
  set(EVENT__DISABLE_SAMPLES ON CACHE BOOL "" FORCE)
  set(EVENT__DISABLE_BENCHMARK ON CACHE BOOL "" FORCE)
  set(EVENT__LIBRARY_TYPE STATIC CACHE STRING "" FORCE)
  # Android Bionic has pthreads built-in — pre-satisfy FindThreads
  set(CMAKE_THREAD_LIBS_INIT "" CACHE STRING "" FORCE)
  set(CMAKE_HAVE_THREADS_LIBRARY TRUE CACHE BOOL "" FORCE)
  set(CMAKE_USE_PTHREADS_INIT TRUE CACHE BOOL "" FORCE)
  set(Threads_FOUND TRUE CACHE BOOL "" FORCE)
  set(CMAKE_HAVE_LIBC_PTHREAD TRUE CACHE BOOL "" FORCE)
  FetchContent_Declare(libevent_src SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/compat/libevent)
  FetchContent_MakeAvailable(libevent_src)
  add_library(PkgConfig::libevent INTERFACE IMPORTED)
  target_link_libraries(PkgConfig::libevent INTERFACE event_static)

  set(URIPARSER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(URIPARSER_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(URIPARSER_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(uriparser_src SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/compat/uriparser)
  FetchContent_MakeAvailable(uriparser_src)
  add_library(PkgConfig::liburiparser ALIAS uriparser)

  set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
  set(BUILD_REGRESS OFF CACHE BOOL "" FORCE)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_DOC OFF CACHE BOOL "" FORCE)
  set(ZLIB_LIBRARY ${ZLIB_LIBRARY} CACHE FILEPATH "" FORCE)
  set(ZLIB_INCLUDE_DIR ${ZLIB_INCLUDE_DIR} CACHE PATH "" FORCE)
  FetchContent_Declare(libzip_src SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/compat/libzip)
  FetchContent_MakeAvailable(libzip_src)
  add_library(PkgConfig::libzip INTERFACE IMPORTED)
  target_link_libraries(PkgConfig::libzip INTERFACE zip ${ZLIB_LIBRARY})

  # libuuid - minimal implementation for Android
  # Android doesn't have util-linux; provide a minimal uuid_generate wrapper
  add_library(uuid_lib STATIC ${CMAKE_CURRENT_SOURCE_DIR}/compat/android_uuid.c)
  target_include_directories(uuid_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/compat)
  add_library(PkgConfig::uuid ALIAS uuid_lib)

  # argp - Android Bionic doesn't have argp.h
  FetchContent_Declare(argp_src SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/compat/argp-standalone)
  FetchContent_MakeAvailable(argp_src)
  # Suppress warnings in third-party argp code
  target_compile_options(argp-standalone PRIVATE -w)
  # Alias so -largp finds it
  add_library(argp ALIAS argp-standalone)
  # Make argp.h available to the whole project
  include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/compat/argp-standalone/include/argp-standalone)

  # OpenSSL compat shims (types.h -> ossl_typ.h for 1.1.1)
  include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/compat)
  # OpenSSL 3.x function renames + Bionic libc compat (strverscmp, etc.)
  # Use CMAKE_C_FLAGS to avoid applying to .S assembly files
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -include ${CMAKE_CURRENT_SOURCE_DIR}/compat/ggl_android_compat.h")

  add_compile_definitions(GG_USE_SYSTEMD=0)

else()
  find_package(PkgConfig REQUIRED)

  pkg_search_module(openssl REQUIRED IMPORTED_TARGET openssl)
  pkg_search_module(libcurl REQUIRED IMPORTED_TARGET libcurl>=7.68)
  pkg_search_module(sqlite3 REQUIRED IMPORTED_TARGET sqlite3)
  pkg_search_module(yaml REQUIRED IMPORTED_TARGET yaml-0.1)
  if(GG_USE_SYSTEMD)
    pkg_search_module(libsystemd REQUIRED IMPORTED_TARGET libsystemd)
    add_compile_definitions(GG_USE_SYSTEMD=1)
  else()
    add_compile_definitions(GG_USE_SYSTEMD=0)
  endif()
  pkg_search_module(uuid REQUIRED IMPORTED_TARGET uuid)
  pkg_search_module(libevent REQUIRED IMPORTED_TARGET libevent)
  pkg_search_module(liburiparser REQUIRED IMPORTED_TARGET liburiparser)
  pkg_search_module(libzip REQUIRED IMPORTED_TARGET libzip)
endif()

include(CheckCSourceCompiles)

check_c_source_compiles(
  "
  #include <argp.h>
  int main(void) { argp_parse(0, 0, 0, 0, 0, 0); }
  "
  has_argp)

#
# Add components
#

add_subdirectory(${gg_sdk_SOURCE_DIR} gg-sdk)
target_include_directories(gg-sdk INTERFACE ${gg_sdk_SOURCE_DIR}/priv_include)

# Common setup for a GGL module
function(ggl_init_module name)
  cmake_parse_arguments(PARSE_ARGV 1 COMP_ARG "" "SRCDIR" "INCDIRS;LIBS")
  if("${COMP_ARG_SRCDIR}" STREQUAL "")
    set(COMP_ARG_SRCDIR src)
  endif()
  if("${COMP_ARG_INCDIRS}" STREQUAL "")
    set(COMP_ARG_INCDIRS include)
  endif()

  file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${COMP_ARG_SRCDIR}/*.c"
       "${COMP_ARG_SRCDIR}/*.S")
  list(LENGTH SRCS SRCS_LEN)

  foreach(src ${SRCS})
    set_property(
      SOURCE ${src}
      APPEND_STRING
      PROPERTY COMPILE_FLAGS "-frandom-seed=${src}")
  endforeach()

  # TODO: Allow modules without src to be built, e.g. ggl-constants can delete
  # src/dummy.c
  if(NOT SRCS_LEN EQUAL 0)
    add_library(${name} STATIC ${SRCS})
    target_compile_definitions(${name} PRIVATE "GG_MODULE=(\"${name}\")")
    target_include_directories(${name} PUBLIC ${COMP_ARG_INCDIRS})
    target_link_libraries(${name} PRIVATE ${COMP_ARG_LIBS})
  endif()

  if(GG_USE_SYSTEMD AND GGL_SYSTEMD_SERVICE_BUILD)
    file(GLOB_RECURSE SERVICES CONFIGURE_DEPENDS
         "${CMAKE_CURRENT_LIST_DIR}/unit/*.in")

    foreach(service ${SERVICES})
      get_filename_component(service_filename ${service} NAME_WLE)
      configure_file(${service} ${CMAKE_CURRENT_BINARY_DIR}/${service_filename}
                     @ONLY)
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${service_filename}
              DESTINATION ${GGL_SYSTEMD_SYSTEM_DIR})
    endforeach()

  endif()

  file(GLOB BINS CONFIGURE_DEPENDS "bin/*.c")
  foreach(bin ${BINS})
    set_property(
      SOURCE ${bin}
      APPEND_STRING
      PROPERTY COMPILE_FLAGS "-frandom-seed=${bin}")

    get_filename_component(bin_name ${bin} NAME_WLE)
    add_executable(${bin_name}-bin ${bin})
    target_compile_definitions(${bin_name}-bin
                               PRIVATE "GG_MODULE=(\"${name}\")")
    if(NOT SRCS_LEN EQUAL 0)
      target_link_libraries(${bin_name}-bin PRIVATE ${name})
    endif()
    target_link_libraries(${bin_name}-bin PRIVATE ${COMP_ARG_LIBS})
    if(NOT has_argp)
      target_link_libraries(${bin_name}-bin PRIVATE argp)
    endif()
    set_target_properties(${bin_name}-bin PROPERTIES OUTPUT_NAME ${bin_name})
    install(TARGETS ${bin_name}-bin)
  endforeach()
endfunction()

file(GLOB MODULES CONFIGURE_DEPENDS "modules/*")
foreach(module ${MODULES})
  add_subdirectory(${module})
endforeach()

if(GG_USE_SYSTEMD AND GGL_SYSTEMD_SERVICE_BUILD)
  add_custom_target(
    greengrass-lite.target ALL
    COMMAND cp "${CMAKE_SOURCE_DIR}/misc/systemd/greengrass-lite.target"
            "${CMAKE_BINARY_DIR}/misc/greengrass-lite.target"
    COMMENT "Nucleus systemd target")
  install(FILES "${CMAKE_BINARY_DIR}/misc/greengrass-lite.target"
          DESTINATION ${GGL_SYSTEMD_SYSTEM_DIR})
  configure_file("${CMAKE_SOURCE_DIR}/misc/systemd/greengrass_dir.conf.in"
                 "${CMAKE_CURRENT_BINARY_DIR}/greengrass_dir.conf" @ONLY)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/greengrass_dir.conf"
          DESTINATION lib/tmpfiles.d)
endif()

if(BUILD_EXAMPLES)
  file(GLOB TEST_MODULES CONFIGURE_DEPENDS "test_modules/*")
  foreach(test_module ${TEST_MODULES})
    add_subdirectory(${test_module})
  endforeach()
endif()

#
# CPACK deb package generation
#

file(READ "${CMAKE_SOURCE_DIR}/version" VERSION_STRING)
string(STRIP "${VERSION_STRING}" VERSION_STRING)

set(CPACK_PACKAGE_VERSION "${VERSION_STRING}")

string(REGEX REPLACE "^([0-9]+)\\.[0-9]+\\.[0-9]+$" "\\1"
                     CPACK_PACKAGE_VERSION_MAJOR "${VERSION_STRING}")
string(REGEX REPLACE "^[0-9]+\\.([0-9]+)\\.[0-9]+$" "\\1"
                     CPACK_PACKAGE_VERSION_MINOR "${VERSION_STRING}")
string(REGEX REPLACE "^[0-9]+\\.[0-9]+\\.([0-9]+)$" "\\1"
                     CPACK_PACKAGE_VERSION_PATCH "${VERSION_STRING}")

set(CPACK_GENERATOR DEB)

set(CPACK_DEBIAN_PACKAGE_RELEASE "${LINUX_DISTRO_STRING}")

set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "all")

set(CPACK_PACKAGE_CONTACT
    "https://github.com/aws-greengrass/aws-greengrass-lite")

set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)

set(GGL_DEB_BASE_DEPS
    "cgroup-tools (>= 2.0), \
    libcurl4 | libcurl4t64 | libcurl3-gnutls (>= 7.68), \
    libevent-2.1-7 | libevent-2.1-7t64 | libevent-2.1-6 (>= 2.1.12), \
    libsqlite3-0 (>= 3.37.2), \
    libssl3 | libssl3t64 (>= 3.0), \
    liburiparser1 (>= 0.9.6), \
    libuuid1 (>= 2.37.2), \
    libyaml-0-2 (>= 0.2.2)")

if(GG_USE_SYSTEMD)
  set(CPACK_DEBIAN_PACKAGE_DEPENDS
      "${GGL_DEB_BASE_DEPS}, libsystemd0 (>= 249)")
else()
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "${GGL_DEB_BASE_DEPS}")
endif()

set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    ${CMAKE_CURRENT_SOURCE_DIR}/debian/postinst
    ${CMAKE_CURRENT_SOURCE_DIR}/debian/postrm
    ${CMAKE_CURRENT_SOURCE_DIR}/debian/conffiles)

include(CPack)

#
# Uninstall target
#

if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_SOURCE_DIR}/misc/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/misc/cmake_uninstall.cmake" IMMEDIATE @ONLY)

  add_custom_target(
    uninstall
    COMMAND ${CMAKE_COMMAND} -P
            ${CMAKE_CURRENT_BINARY_DIR}/misc/cmake_uninstall.cmake
    COMMENT "Uninstall target")
endif()
