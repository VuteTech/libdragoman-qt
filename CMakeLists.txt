# SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
# SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# libdragoman-qt: the Qt client library for the dragomand offline
# translation daemon, shared by Krakoman, the System Settings module, the
# Plasma widget and other Qt front ends.
#
#   cmake -S . -B build -G Ninja
#   cmake --build build
#   ctest --test-dir build
#   cmake --install build --prefix ~/.local
#
# Consumers: find_package(DragomanQt REQUIRED) and link DragomanQt::DragomanQt.

cmake_minimum_required(VERSION 3.24)

# The version lives in git tags only (vX.Y.Z). In order of precedence:
# -DDRAGOMANQT_VERSION=X.Y.Z, the .tarball-version file that the release
# tarball carries, `git describe` (X.Y.Z on a tag, X.Y.Z.N for N commits
# after it), and 0.0.0 outside any of these.
if(NOT DRAGOMANQT_VERSION)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.tarball-version")
        file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/.tarball-version" DRAGOMANQT_VERSION LIMIT_COUNT 1)
    else()
        execute_process(
            COMMAND git describe --tags --long --match "v[0-9]*"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE _described
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _describe_result)
        if(_describe_result EQUAL 0 AND _described MATCHES "^v([0-9]+\\.[0-9]+\\.[0-9]+)-([0-9]+)-g")
            if(CMAKE_MATCH_2 EQUAL 0)
                set(DRAGOMANQT_VERSION "${CMAKE_MATCH_1}")
            else()
                set(DRAGOMANQT_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")
            endif()
        else()
            set(DRAGOMANQT_VERSION "0.0.0")
        endif()
    endif()
endif()

project(libdragoman-qt VERSION ${DRAGOMANQT_VERSION} LANGUAGES CXX)

# The oldest supported distribution release (Debian 13) sets these.
set(QT_MIN_VERSION "6.8.0")
set(KF_MIN_VERSION "6.13.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

set(KDE_COMPILERSETTINGS_LEVEL 6.13)

include(KDEInstallDirs)
add_custom_target(fetch-translations)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDEClangFormat)
include(KDEGitCommitHooks)
include(CMakePackageConfigHelpers)
include(ECMDeprecationSettings)
include(ECMGenerateExportHeader)
include(ECMQtDeclareLoggingCategory)
include(ECMSetupVersion)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core DBus)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS I18n)

ecm_set_disabled_deprecation_versions(QT ${QT_MIN_VERSION} KF ${KF_MIN_VERSION})

# The API is young: the SO version is the minor version until 1.0.
set(DRAGOMANQT_SOVERSION ${PROJECT_VERSION_MINOR})
ecm_setup_version(${PROJECT_VERSION}
    VARIABLE_PREFIX DRAGOMANQT
    VERSION_HEADER ${CMAKE_CURRENT_BINARY_DIR}/src/dragomanqt_version.h
    PACKAGE_VERSION_FILE ${CMAKE_CURRENT_BINARY_DIR}/DragomanQtConfigVersion.cmake
    SOVERSION ${DRAGOMANQT_SOVERSION})

set(CMAKECONFIG_INSTALL_DIR "${KDE_INSTALL_CMAKEPACKAGEDIR}/DragomanQt")

add_subdirectory(src)

if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
    add_subdirectory(autotests)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/po")
    ki18n_install(po)
endif()

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/DragomanQtConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/DragomanQtConfig.cmake
    INSTALL_DESTINATION ${CMAKECONFIG_INSTALL_DIR})
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/DragomanQtConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/DragomanQtConfigVersion.cmake
    DESTINATION ${CMAKECONFIG_INSTALL_DIR})
install(EXPORT DragomanQtTargets
    DESTINATION ${CMAKECONFIG_INSTALL_DIR}
    FILE DragomanQtTargets.cmake
    NAMESPACE DragomanQt::)

# Consumers can also build against this build tree without installing:
# -DDragomanQt_DIR=<this build directory>.
export(EXPORT DragomanQtTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/buildtree/DragomanQtTargets.cmake
    NAMESPACE DragomanQt::)
configure_file(DragomanQtBuildTreeConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/buildtree/DragomanQtConfig.cmake @ONLY)
file(COPY ${CMAKE_CURRENT_BINARY_DIR}/DragomanQtConfigVersion.cmake DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/buildtree)

file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES src/*.cpp src/*.h autotests/*.cpp autotests/*.h testing/*.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
