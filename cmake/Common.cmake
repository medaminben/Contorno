# Common project settings. Include early from the root CMakeLists.txt.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# No C++20 modules are used; module dependency scanning only slows builds and
# triggers a Ninja dyndep assertion when source lists change.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

include(GNUInstallDirs)
# Override buildkit's create_library, create_application and generate_libraries.
include(ContornoTargets)
include(ContornoScaffold)
# The loader ran before project(), where Coverage has no compiler and does
# nothing; this second include configures it.
include(Coverage)

if(BUILD_VERBOSE_OUTPUT)
    set(CMAKE_VERBOSE_MAKEFILE ON)
    dump_cmake_vars(configLogStart.log)
endif()

if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
        message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
    else()
        message(WARNING "ENABLE_CLANG_TIDY is ON but clang-tidy was not found")
    endif()
endif()

if(BUILD_TESTING)
    message(STATUS "BUILD_TESTING: ${BUILD_TESTING}")
    include(CTest)
    enable_testing()
endif()

if(BUILD_VISION)
    include(OpenCVSupport)
endif()
if(BUILD_VISION AND BUILD_VISION_AI)
    include(TorchSupport)
endif()
if(BUILD_VERBOSE_OUTPUT)
    dump_cmake_vars(configLogEnd.log)
endif()
