# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.21)

# ---- Edit this ONE variable per project ----
set(APP_NAME "RemindMe")
# --------------------------------------------
project(${APP_NAME} VERSION 1.1.1 LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)

option(ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(ENABLE_SANITIZERS "Enable sanitizers for Debug builds (GCC/Clang)" ON)
option(ENABLE_IPO "Enable interprocedural optimization in Release builds" ON)

# Qt is optional in CI / non-GUI environments.
# Default ON on Windows for local dev convenience; OFF elsewhere.
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
target_include_directories(${CORE_TARGET} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(${CORE_TARGET} PUBLIC cxx_std_20)

# ---- Qt (optional) ----
if (ENABLE_QT)
    find_package(Qt6 QUIET COMPONENTS Widgets)
else()
    set(Qt6_FOUND OFF)
endif()

if (Qt6_FOUND)
    target_link_libraries(${CORE_TARGET} PUBLIC Qt6::Widgets)
    target_compile_definitions(${CORE_TARGET} PUBLIC REMINDME_HAS_QT=1)
    message(STATUS "Qt6 found: building GUI executable (${APP_NAME}).")
else()
    target_compile_definitions(${CORE_TARGET} PUBLIC REMINDME_HAS_QT=0)
    message(STATUS "Qt6 not found (or disabled): GUI executable will NOT be built. Core + tests only.")
endif()
# -----------------------

if (ENABLE_WARNINGS)
    if (MSVC)
        target_compile_options(${CORE_TARGET} PRIVATE /W4 /permissive- /Zc:__cplusplus /utf-8)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${CORE_TARGET} PRIVATE -Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8)
    endif()
endif()

set(SANITIZERS_ACTIVE OFF)
if (ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT WIN32)
    set(SANITIZERS_ACTIVE ON)
    target_compile_options(${CORE_TARGET} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${CORE_TARGET} PRIVATE -fsanitize=address,undefined)
endif()

# ---- App executable (GUI) ----
# Only build the app when Qt exists, because main.cpp is a Qt GUI entrypoint.
if (Qt6_FOUND)
    add_executable(${APP_NAME} WIN32
        src/main.cpp
    )
    target_link_libraries(${APP_NAME} PRIVATE ${CORE_TARGET})
    target_compile_features(${APP_NAME} PRIVATE cxx_std_20)

    # Stable dev binary name: Debug/Dev builds -> app(.exe), Release -> APP_NAME
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        set_target_properties(${APP_NAME} PROPERTIES OUTPUT_NAME "${APP_NAME}")
    else()
        set_target_properties(${APP_NAME} PROPERTIES OUTPUT_NAME "app")
    endif()

    if (WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
        target_sources(${APP_NAME} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc"
        )
    endif()

    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.qrc")
        target_sources(${APP_NAME} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.qrc"
        )
    endif()

    if (ENABLE_WARNINGS)
        if (MSVC)
            target_compile_options(${APP_NAME} PRIVATE /W4 /permissive- /Zc:__cplusplus /utf-8)
        elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${APP_NAME} PRIVATE -Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8)
        endif()
    endif()

    if (SANITIZERS_ACTIVE)
        target_compile_options(${APP_NAME} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${APP_NAME} PRIVATE -fsanitize=address,undefined)
    endif()
endif()
# ------------------------------

if (ENABLE_IPO AND CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if (ipo_supported)
        # Only apply IPO to targets that exist.
        if (TARGET ${CORE_TARGET})
            set_property(TARGET ${CORE_TARGET} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        endif()
        if (TARGET ${APP_NAME})
            set_property(TARGET ${APP_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        endif()
    endif()
endif()

if (BUILD_TESTING)
    add_executable(${APP_NAME}_tests
        tests/core_tests.cpp
    )
    target_link_libraries(${APP_NAME}_tests PRIVATE ${CORE_TARGET})
    target_compile_features(${APP_NAME}_tests PRIVATE cxx_std_20)

    if (ENABLE_WARNINGS)
        if (MSVC)
            target_compile_options(${APP_NAME}_tests PRIVATE /W4 /permissive- /Zc:__cplusplus /utf-8)
        elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${APP_NAME}_tests PRIVATE -Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8)
        endif()
    endif()

    if (SANITIZERS_ACTIVE)
        target_compile_options(${APP_NAME}_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${APP_NAME}_tests PRIVATE -fsanitize=address,undefined)
    endif()

    if (TARGET conventions)
        add_dependencies(${APP_NAME}_tests conventions)
    endif()

    add_test(NAME ${APP_NAME}.core COMMAND ${APP_NAME}_tests)
endif()

# Install: core always; app only if it exists.
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
