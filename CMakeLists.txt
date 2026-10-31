# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.21)
# Link spdlog/fmt statically: SKSE's plugin folder has no spdlog.dll or fmt.dll, so the default
# x64-windows triplet builds a DLL that fails to load (error 126). Must be set before project().
set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "")
project(Pulse VERSION 1.0.1 LANGUAGES CXX)

# SE + AE (flatrim) only; no hooks, so the trampoline patch-safety fetch is unnecessary.
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
set(SKSE_SUPPORT_PATCH_SAFETY OFF CACHE BOOL "" FORCE)

# CommonLibSSE-NG (alandtse ng). Point COMMONLIB_DIR at an existing checkout.
set(COMMONLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/commonlibsse-ng" CACHE PATH "CommonLibSSE-NG source")
add_subdirectory(${COMMONLIB_DIR} commonlib)

# A plain source add_subdirectory does not define the helper.
if(NOT COMMAND add_commonlibsse_plugin)
    include(${COMMONLIB_DIR}/cmake/CommonLibSSE.cmake)
endif()

add_commonlibsse_plugin(${PROJECT_NAME} AUTHOR "nosimo" SOURCES src/plugin.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_compile_options(${PROJECT_NAME} PRIVATE /Zc:preprocessor /permissive- /EHsc /W4)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
# SkyrimNet C++ API headers from the SkyrimNet devkit (CppAPI/, MIT; see include/SkyrimNet/LICENSE).
target_include_directories(${PROJECT_NAME} PRIVATE include)
