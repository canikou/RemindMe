cmake_minimum_required(VERSION 3.20)

set(PROJECT_NAME RemindMe)
project(${PROJECT_NAME} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# ---- Qt (Widgets UI) ----
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Needed for Q_OBJECT in headers (MainWindow.h)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)

# ---- Sources ----
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

# (Optional but nice for VS Code / IDE browsing)
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/include/*.h"
    "${CMAKE_SOURCE_DIR}/include/*.hpp"
)

set(RESOURCE_FILES
    "${CMAKE_SOURCE_DIR}/resources/app.qrc"
)

if (WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/resources/icons/app.ico")
    list(APPEND RESOURCE_FILES "${CMAKE_SOURCE_DIR}/resources/app.rc")
endif()

# ---- Target ----
if (WIN32)
    add_executable(app WIN32 ${SOURCES} ${HEADERS} ${RESOURCE_FILES})
else()
    add_executable(app ${SOURCES} ${HEADERS} ${RESOURCE_FILES})
endif()

if (WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/resources/icons/app.ico")
    target_sources(app PRIVATE "${CMAKE_SOURCE_DIR}/resources/app.rc")
endif()

set_target_properties(app PROPERTIES OUTPUT_NAME "RemindMe")


target_include_directories(app PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(app PRIVATE Qt6::Widgets)

# Nice warnings
if (MSVC)
    target_compile_options(app PRIVATE /W4 /utf-8)
else()
    target_compile_options(app PRIVATE -Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()
