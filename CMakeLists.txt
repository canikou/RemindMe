# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.21)

# ---- Edit this ONE variable per project ----
set(APP_NAME "RemindMe")
# --------------------------------------------
project(${APP_NAME} VERSION 1.2.2 LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ProjectOptions)
include(ClangTools)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)

option(ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(ENABLE_SANITIZERS "Enable sanitizers for Debug builds (GCC/Clang)" ON)
option(ENABLE_IPO "Enable interprocedural optimization in Release builds" ON)
option(ENABLE_CLANG_TOOLS "Enable clang-format and clang-tidy helper targets" ON)

# Qt is optional in configuration, but this app expects Qt6 Widgets in normal usage.
option(ENABLE_QT "Enable Qt6 Widgets UI (optional)" ON)
if (NOT WIN32)
    set(ENABLE_QT OFF)
endif()

if (WIN32)
    set(CONVENTIONS_POWERSHELL "powershell")
else()
    find_program(CONVENTIONS_POWERSHELL NAMES pwsh powershell)
endif()

if (CONVENTIONS_POWERSHELL)
    add_custom_target(conventions
        COMMAND ${CONVENTIONS_POWERSHELL} -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check-conventions.ps1"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Running project convention checks"
        VERBATIM
    )
endif()

set(CORE_TARGET "${APP_NAME}Core")
set(TEST_TARGET "${APP_NAME}_tests")

file(GLOB_RECURSE PROJECT_FORMAT_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
)

file(GLOB_RECURSE PROJECT_LINT_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
list(REMOVE_ITEM CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

add_library(${CORE_TARGET}
    ${CORE_SOURCES}
    ${HEADERS}
)
add_library(${APP_NAME}::Core ALIAS ${CORE_TARGET})

target_include_directories(${CORE_TARGET}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(${CORE_TARGET} PUBLIC cxx_std_23)
project_apply_default_build_options(${CORE_TARGET})

if (ENABLE_QT)
    find_package(Qt6 QUIET COMPONENTS Widgets Network)
else()
    set(Qt6_FOUND OFF)
endif()

if (Qt6_FOUND)
    target_link_libraries(${CORE_TARGET} PUBLIC Qt6::Widgets Qt6::Network)
    target_compile_definitions(${CORE_TARGET} PUBLIC REMINDME_HAS_QT=1)
    message(STATUS "Qt6 found: building GUI executable (${APP_NAME}).")
else()
    target_compile_definitions(${CORE_TARGET} PUBLIC REMINDME_HAS_QT=0)
    message(STATUS "Qt6 not found (or disabled): GUI executable will NOT be built. Core + tests only.")
endif()

if (Qt6_FOUND)
    add_executable(${APP_NAME} WIN32
        src/main.cpp
    )
    target_link_libraries(${APP_NAME} PRIVATE ${CORE_TARGET})
    target_compile_features(${APP_NAME} PRIVATE cxx_std_23)
    project_apply_default_build_options(${APP_NAME})

    set_target_properties(${APP_NAME} PROPERTIES
        OUTPUT_NAME "$<IF:$<CONFIG:Release>,${APP_NAME},app>"
    )

    if (WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
        target_sources(${APP_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
    endif()

    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.qrc")
        target_sources(${APP_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.qrc")
    endif()
endif()

if (BUILD_TESTING)
    add_executable(${TEST_TARGET}
        tests/core_tests.cpp
    )
    target_link_libraries(${TEST_TARGET} PRIVATE ${CORE_TARGET})
    target_compile_features(${TEST_TARGET} PRIVATE cxx_std_23)
    project_apply_default_build_options(${TEST_TARGET})

    if (TARGET conventions)
        add_dependencies(${TEST_TARGET} conventions)
    endif()

    add_test(NAME ${APP_NAME}.core COMMAND ${TEST_TARGET})
endif()

project_add_clang_tool_targets(
    FORMAT_SOURCES ${PROJECT_FORMAT_SOURCES}
    LINT_SOURCES ${PROJECT_LINT_SOURCES}
)

install(TARGETS ${CORE_TARGET}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
if (TARGET ${APP_NAME})
    install(TARGETS ${APP_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
