project(Core VERSION 1.1.0 LANGUAGES CXX)

set(TARGET_NAME Core)

set(TST_DISCOVER ON)
###############################################################
# Manage project files
###############################################################
set(TARGET_HEADERS
    # Public headers belong in include/<library>/; other components
    # and applications include them as #include <Library/Header.h>
    include/${TARGET_NAME}/${TARGET_NAME}.h
    include/${TARGET_NAME}/Problem.h
)

# Header-only: no sources, so create_library makes an INTERFACE library.
set(TARGET_SOURCES
)

set(TARGET_RSC
    # resources
    #test/data/${TARGET_NAME}TestData.txt
)

##dependencies
#private dependencies
set(PRIVATE_DEPS
)
#public dependencies
set(PUBLIC_DEPS
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
    test/src/test_${TARGET_NAME}.cpp
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
