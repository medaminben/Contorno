cmake_minimum_required(VERSION 3.29)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/buildkit.cmake)

# Fallback 1.0.0 without a vX.Y.Z tag.
buildkit_git_version(${CMAKE_CURRENT_SOURCE_DIR} CONTORNO_VERSION CONTORNO_VERSION_FULL FALLBACK 1.0.0)

# Project names are only relevant for humans; camel case is okay.
project(Contorno VERSION ${CONTORNO_VERSION} LANGUAGES CXX)

# Global project name used by library aliases (Contorno::Core)
set(CMAKE_ROOT_NAME ${PROJECT_NAME})

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
option(BUILD_SHARED_LIBS     "Build project libraries as shared libraries" ON)
option(BUILD_APPS            "Build applications"                          ON)
option(BUILD_WX_UI           "Build wxWidgets based applications"          ON)
option(BUILD_VISION          "Build the OpenCV-based Vision library"       ON)
option(BUILD_VISION_AI       "Build the LibTorch-based object segmentation in Vision" ON)
option(BUILD_VERBOSE_OUTPUT  "Build with verbose output"                   OFF)
option(BUILD_TESTING         "Build tests"                                 ON)
option(ENABLE_COVERAGE       "Enable code coverage instrumentation"        OFF)
option(ENABLE_CLANG_TIDY     "Run clang-tidy during compilation"           OFF)
option(GENERATE_SCAFFOLD     "Generate missing libraries from templates"   ON)

# The loader put buildkit's modules on the path; Contorno's come first.
list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(Common)

# Libraries to generate / include. Add new names here for scaffold generation.
set(CONTORNO_LIBRARIES
    Core
    Pot
    Tools
    Vision
    Inspection
)

set(libraries_dir    ${CMAKE_SOURCE_DIR}/libraries)
set(applications_dir ${CMAKE_SOURCE_DIR}/applications)
set(template_dir     ${CMAKE_SOURCE_DIR}/cmake/template)

if(GENERATE_SCAFFOLD)
    # Only libraries without a directory under libraries/ are generated.
    generate_libraries(
        LIBRARIES_LIST     ${CONTORNO_LIBRARIES}
        LIBRARIES_LOCATION ${libraries_dir}
        APPS_LOCATION      ${applications_dir}
        TEMPLATE_LOCATION  ${template_dir}
    )
endif()

if(EXISTS "${libraries_dir}/CMakeLists.txt")
    add_subdirectory(${libraries_dir})
endif()

if(BUILD_APPS AND EXISTS "${applications_dir}/CMakeLists.txt")
    add_subdirectory(${applications_dir})
endif()

include(InstallConfig)
include(Installers)

if(BUILD_VERBOSE_OUTPUT)
    dump_cmake_vars(BuildLogEnd.log)
endif()
