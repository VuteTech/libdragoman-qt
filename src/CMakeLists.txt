# SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
# SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(DragomanQt)
add_library(DragomanQt::DragomanQt ALIAS DragomanQt)
target_sources(DragomanQt PRIVATE
    dragomanclient.cpp
    dragomanclient.h
    dragomantypes.cpp
    dragomantypes.h
    languagenames.cpp
    languagenames.h)
ecm_qt_declare_logging_category(DragomanQt
    HEADER dragoman_debug.h
    IDENTIFIER DRAGOMAN_LOG
    CATEGORY_NAME dev.l10n_bg.dragoman.qt
    DESCRIPTION "Qt client library for the Dragomand daemon"
    EXPORT DRAGOMANQT)
ecm_generate_export_header(DragomanQt
    BASE_NAME DragomanQt
    VERSION ${DRAGOMANQT_RELEASE_VERSION}
    USE_VERSION_HEADER dragomanqt_version.h
    DEPRECATED_BASE_VERSION 0)
set_target_properties(DragomanQt PROPERTIES
    OUTPUT_NAME dragoman-qt
    VERSION ${DRAGOMANQT_RELEASE_VERSION}
    SOVERSION ${DRAGOMANQT_SOVERSION}
    EXPORT_NAME DragomanQt)
target_compile_definitions(DragomanQt PRIVATE TRANSLATION_DOMAIN="libdragoman-qt")
target_include_directories(DragomanQt
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR};${CMAKE_CURRENT_BINARY_DIR}>"
    INTERFACE
        "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR}/DragomanQt>")
target_link_libraries(DragomanQt
    PUBLIC
        Qt6::Core
        Qt6::DBus
    PRIVATE
        KF6::I18n)

install(TARGETS DragomanQt EXPORT DragomanQtTargets ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES
    dragomanclient.h
    dragomantypes.h
    languagenames.h
    ${CMAKE_CURRENT_BINARY_DIR}/dragomanqt_export.h
    ${CMAKE_CURRENT_BINARY_DIR}/dragomanqt_version.h
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/DragomanQt
    COMPONENT Devel)
install(FILES ../testing/fakedaemon.h DESTINATION ${KDE_INSTALL_INCLUDEDIR}/DragomanQt/testing COMPONENT Devel)
ecm_qt_install_logging_categories(EXPORT DRAGOMANQT FILE libdragoman-qt.categories DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR})
