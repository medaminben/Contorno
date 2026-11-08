project(Vision VERSION 1.0.0 LANGUAGES CXX)

set(TARGET_NAME Vision)

set(TST_DISCOVER ON)
###############################################################
# Manage project files
###############################################################
set(TARGET_HEADERS
    include/${TARGET_NAME}/${TARGET_NAME}.h
    include/${TARGET_NAME}/Camera.h
    include/${TARGET_NAME}/Image.h
    include/${TARGET_NAME}/Lens.h
    include/${TARGET_NAME}/Objects.h
    include/${TARGET_NAME}/Photo.h
    src/Ai_impl.h
    src/Camera_impl.h
    src/Segment_impl.h
    src/Objects_impl.h
)

set(TARGET_SOURCES
    src/Ai_impl.cpp
    src/Camera.cpp
    src/Camera_impl.cpp
    src/Image.cpp
    src/Lens.cpp
    src/Objects.cpp
    src/Objects_impl.cpp
    src/Photo.cpp
    src/Segment_impl.cpp
)

set(TARGET_RSC
    test/data/calf_black.jpg
    test/data/cowhide_stretched.jpg
)

##dependencies
#private dependencies
set(PRIVATE_DEPS
    ${CONTORNO_OPENCV_LIBRARIES}
    ${CONTORNO_TORCH_LIBRARIES}
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
    test/src/test_${TARGET_NAME}.cpp
    test/src/test_Objects.cpp
    test/src/test_Image.cpp
    test/src/test_Lens.cpp
    test/src/test_Photo.cpp
    test/src/Exif.h
    test/src/Geometry.h
)

set(TEST_DEPS
    ${CONTORNO_OPENCV_LIBRARIES}
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

if(CONTORNO_WITH_TORCH)
    target_compile_definitions(contorno_vision PRIVATE CONTORNO_WITH_TORCH=1)
    if(TARGET test_contorno_vision)
        target_compile_definitions(test_contorno_vision PRIVATE CONTORNO_TEST_WITH_TORCH=1)
    endif()
    # Installed: Vision finds the LibTorch libraries installed next to it.
    set_target_properties(contorno_vision PROPERTIES INSTALL_RPATH "$ORIGIN")
    install(DIRECTORY ${CONTORNO_TORCH_ROOT}/lib/
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        FILES_MATCHING PATTERN "*.so*"
        PATTERN "*test*" EXCLUDE
        PATTERN "libtorch_python*" EXCLUDE
        PATTERN "libbackend_with_compiler*" EXCLUDE
        PATTERN "libaoti_custom_ops*" EXCLUDE
        PATTERN "libshm" EXCLUDE
        PATTERN "libshm_windows" EXCLUDE
    )
endif()

# The IS-Net model of the AI tests (they skip when it is missing). The sample
# photos are read from docs/samples (licence unknown, so not copied into
# the test data).
if(CONTORNO_WITH_TORCH)
    include(IsnetModel)
    contorno_add_isnet_model(_isnet)
    set(CONTORNO_ISNET_MODEL "${_isnet}" CACHE FILEPATH "TorchScript IS-Net used by the Vision tests")
    if(TARGET isnet_model AND TARGET test_contorno_vision)
        add_dependencies(test_contorno_vision isnet_model)
    endif()
endif()
if(TARGET test_contorno_vision)
    target_compile_definitions(test_contorno_vision PRIVATE
        CONTORNO_ISNET_MODEL="${CONTORNO_ISNET_MODEL}"
        CONTORNO_SAMPLES_DIR="${CMAKE_SOURCE_DIR}/docs/samples"
    )
endif()
