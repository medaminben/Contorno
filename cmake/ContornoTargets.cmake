# Contorno's own create_library and create_application. buildkit's BuildUtils
# defines both too; this file is included after buildkit and overrides them,
# because Contorno needs three things buildkit's versions lack:
#   - header-only (INTERFACE) libraries, e.g. Core;
#   - the header layout <Lib>/<Lib>_export.h with the macro <LIB>_API.
# The WX_ui entry is buildkit's: it is passed on to its create_application,
# which CMake keeps under the name _create_application.
# check_project_name, add_project and get_raw_target_name come from buildkit.

# ---------------------------------------------------------------------------
# create_library — shared/static library with export header + optional tests;
# header-only (INTERFACE) when LIB_FILES holds no source file.
#
# Public headers sit in include/<LIB_NAME>/ and are included as
# <LIB_NAME/Header.h>; the export header is <LIB_NAME/LIB_NAME_export.h> with
# the macro <LIB_NAME>_API. Installed headers go to include/CMAKE_ROOT_NAME/.
# ---------------------------------------------------------------------------
function(create_library)
    set(options)
    set(single_value_args LIB_NAME TEST_DISCOVER)
    set(list_args LIB_FILES PRIVATE_DEPENDENCIES PUBLIC_DEPENDENCIES
                  TEST_SOURCES TEST_DEPENDENCIES LIB_RSC)
    cmake_parse_arguments(PARSE_ARGV 0 parameter
        "${options}" "${single_value_args}" "${list_args}")

    foreach(arg IN LISTS parameter_UNPARSED_ARGUMENTS)
        message(WARNING "unparsed argument: ${arg}")
    endforeach()

    if(DEFINED CMAKE_ROOT_NAME AND NOT CMAKE_ROOT_NAME STREQUAL "")
        set(LIBRARY_NAME ${CMAKE_ROOT_NAME}::${parameter_LIB_NAME})
        get_raw_target_name(${LIBRARY_NAME} LIBRARY_NAME_RAW)
    else()
        set(LIBRARY_NAME ${parameter_LIB_NAME})
        set(LIBRARY_NAME_RAW ${LIBRARY_NAME})
    endif()

    set(_sources ${parameter_LIB_FILES})
    list(FILTER _sources INCLUDE REGEX "\\.(c|cc|cpp|cxx)$")
    set(_include_install ${CMAKE_INSTALL_INCLUDEDIR}/${CMAKE_ROOT_NAME})

    if(NOT _sources)
        add_library(${LIBRARY_NAME_RAW} INTERFACE ${parameter_LIB_FILES})
        add_library(${LIBRARY_NAME} ALIAS ${LIBRARY_NAME_RAW})
        target_include_directories(${LIBRARY_NAME_RAW}
            INTERFACE
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                $<INSTALL_INTERFACE:${_include_install}>
        )
        target_link_libraries(${LIBRARY_NAME_RAW} INTERFACE ${parameter_PUBLIC_DEPENDENCIES})
        set_target_properties(${LIBRARY_NAME_RAW} PROPERTIES EXPORT_NAME "${parameter_LIB_NAME}")
        install(TARGETS ${LIBRARY_NAME_RAW} EXPORT ${CMAKE_ROOT_NAME}Targets)
    else()
        add_library(${LIBRARY_NAME_RAW} ${parameter_LIB_FILES})
        add_library(${LIBRARY_NAME} ALIAS ${LIBRARY_NAME_RAW})

        include(GenerateExportHeader)
        string(TOUPPER ${parameter_LIB_NAME} LIB_FLAG)
        generate_export_header(${LIBRARY_NAME_RAW}
            EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/${parameter_LIB_NAME}/${parameter_LIB_NAME}_export.h"
            EXPORT_MACRO_NAME ${LIB_FLAG}_API
            BASE_NAME ${LIB_FLAG}
            DEFINE_NO_DEPRECATED
        )

        target_include_directories(${LIBRARY_NAME_RAW}
            PUBLIC
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
                $<INSTALL_INTERFACE:${_include_install}>
        )

        target_link_libraries(${LIBRARY_NAME_RAW}
            PRIVATE ${parameter_PRIVATE_DEPENDENCIES}
            PUBLIC  ${parameter_PUBLIC_DEPENDENCIES}
        )

        buildkit_set_warnings(${LIBRARY_NAME_RAW})

        set_target_properties(${LIBRARY_NAME_RAW} PROPERTIES
            OUTPUT_NAME "${CMAKE_ROOT_NAME}${parameter_LIB_NAME}"
            EXPORT_NAME "${parameter_LIB_NAME}"
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR}
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN ON
        )

        install(TARGETS ${LIBRARY_NAME_RAW}
            EXPORT ${CMAKE_ROOT_NAME}Targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
        install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/
            DESTINATION ${_include_install}
        )
    endif()

    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${_include_install}
    )

    if(BUILD_TESTING AND parameter_TEST_SOURCES)
        set(TEST_NAME "test_${LIBRARY_NAME_RAW}")
        set(GTEST_DEPENDENCIES GTest::gtest GTest::gmock GTest::gmock_main)

        build_gtest_executable(
            NAME ${TEST_NAME}
            SRC  ${parameter_TEST_SOURCES}
            DEPENDS
                ${parameter_TEST_DEPENDENCIES}
                ${GTEST_DEPENDENCIES}
                ${LIBRARY_NAME}
            DISCOVER ${parameter_TEST_DISCOVER}
        )
        if(DEFINED parameter_LIB_RSC)
            foreach(item IN LISTS parameter_LIB_RSC)
                set(_rsc "${item}")
                if(NOT IS_ABSOLUTE "${_rsc}")
                    set(_rsc "${CMAKE_CURRENT_SOURCE_DIR}/${item}")
                endif()
                if(EXISTS "${_rsc}")
                    file(COPY ${_rsc}
                        DESTINATION ${PROJECT_BUILD_OUTPUT}/data/${LIBRARY_NAME_RAW})
                endif()
            endforeach()
        endif()
    endif()
endfunction()

# ---------------------------------------------------------------------------
# create_application — console, or buildkit's wx executable
# ---------------------------------------------------------------------------
function(create_application)
    cmake_parse_arguments(PARSE_ARGV 0 wx_probe "" "ENTRY" "")
    if(wx_probe_ENTRY STREQUAL "WX_ui")
        _create_application(${ARGV})
        return()
    endif()

    # Empty keyword args (e.g. UI with no value) are allowed for app scaffolds.
    if(POLICY CMP0174)
        cmake_policy(SET CMP0174 NEW)
    endif()

    set(options)
    set(single_value_args NAME ENTRY UI)
    set(list_args HEADERS SOURCES RESOURCES BUILD_ARGS DEPENDENCIES)
    cmake_parse_arguments(PARSE_ARGV 0 app
        "${options}" "${single_value_args}" "${list_args}")

    foreach(arg IN LISTS app_UNPARSED_ARGUMENTS)
        message(WARNING "unparsed argument: ${arg}")
    endforeach()

    if(NOT DEFINED app_ENTRY OR app_ENTRY STREQUAL "")
        message(FATAL_ERROR "${app_NAME} is missing ENTRY [Console]")
    endif()

    set(PROJECT_FILES ${app_HEADERS} ${app_SOURCES} ${app_RESOURCES})
    if(DEFINED app_UI AND NOT app_UI STREQUAL "")
        list(APPEND PROJECT_FILES ${app_UI})
    endif()


    if(app_ENTRY STREQUAL "Console")
        add_executable(${app_NAME} ${app_BUILD_ARGS} ${PROJECT_FILES})
        if(DEFINED app_DEPENDENCIES AND NOT app_DEPENDENCIES STREQUAL "")
            target_link_libraries(${app_NAME} PRIVATE ${app_DEPENDENCIES})
        endif()
        buildkit_set_warnings(${app_NAME})
        install(TARGETS ${app_NAME}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    else()
        message(FATAL_ERROR "${app_NAME}: ENTRY must be Console or WX_ui, got '${app_ENTRY}'")
    endif()
endfunction()
