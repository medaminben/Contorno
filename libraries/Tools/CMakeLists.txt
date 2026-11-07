project(Tools VERSION 1.1.0 LANGUAGES CXX)

set(TARGET_NAME Tools)

set(TST_DISCOVER ON)
###############################################################
# Manage project files
###############################################################
set(TARGET_HEADERS
    # private headers belong to "src"
    # Public headers belong in include/<library>/; other components
    # and applications include them as #include <Library/Header.h>
    include/${TARGET_NAME}/${TARGET_NAME}.h
    include/${TARGET_NAME}/Json.h
    include/${TARGET_NAME}/Dxf.h
    include/${TARGET_NAME}/Zip.h
    src/Json_impl.h
    src/Zip_impl.h
)

set(TARGET_SOURCES
    # Source files belong in "src",
    # they will never been seen or used by other components,
    src/Json.cpp
    src/Json_impl.cpp
    src/Dxf.cpp
    src/Zip.cpp
    src/Zip_impl.cpp
)

set(TARGET_RSC
    # resources
    test/data/python_stored.zip
    test/data/python_deflated.zip
)

##dependencies
#private dependencies
set(PRIVATE_DEPS
)
#public dependencies
set(PUBLIC_DEPS
    ${CMAKE_ROOT_NAME}::Core
    ${CMAKE_ROOT_NAME}::Pot
)
set(TARGET_FILES
    ${TARGET_HEADERS}
    ${TARGET_SOURCES}
)
###############################################################
# Manage testing files
###############################################################
set(TST_SOURCES
    # C++ files containing test cases should start with test_
    # to identify them more easily.
    test/src/test_Json.cpp
    test/src/test_Dxf.cpp
    test/src/test_Zip.cpp
)

# Test dependencies other than the actual library
set(TEST_DEPS
)
###############################################################
# build
###############################################################
create_library(
    LIB_NAME             ${TARGET_NAME}
    LIB_FILES            ${TARGET_FILES}
    PRIVATE_DEPENDENCIES ${PRIVATE_DEPS}
    PUBLIC_DEPENDENCIES  ${PUBLIC_DEPS}
    TEST_DISCOVER        ${TST_DISCOVER}
    TEST_SOURCES         ${TST_SOURCES}
    TEST_DEPENDENCIES    ${TEST_DEPS}
    LIB_RSC              ${TARGET_RSC}
)
