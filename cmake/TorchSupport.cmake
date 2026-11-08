# LibTorch for the AI segmentation in the Vision library.
#
# Uses an installed LibTorch when find_package finds one (default hint:
# /opt/libtorch); otherwise one in <build>/_deps/libtorch, which, if missing,
# is downloaded (pinned CPU release) when CONTORNO_TORCH_FETCH is ON and
# offline mode is off.
# Configure never fails here: without LibTorch it warns and Vision is built
# without AI segmentation.
#
# Result: CONTORNO_WITH_TORCH (ON/OFF) and CONTORNO_TORCH_LIBRARIES, the
# targets to link against (empty without LibTorch).

option(CONTORNO_TORCH_FETCH "Download LibTorch when find_package fails" ON)
set(CONTORNO_TORCH_VERSION "2.12.0" CACHE STRING "LibTorch release fetched when none is installed")
if(WIN32)
    set(_contorno_torch_archive "libtorch-win-shared-with-deps-2.12.0%2Bcpu.zip")
    set(_contorno_torch_sha256 "8f8f018ca1cf372158916192dc1e97219983c047cdcd7cd9b11d21e64a2e93bf")
else()
    set(_contorno_torch_archive "libtorch-shared-with-deps-2.12.0%2Bcpu.zip")
    set(_contorno_torch_sha256 "220bdabe192da7b6ae4b07e8735094b9ae542cc5743fc129c0a85315c03e34bc")
endif()
set(CONTORNO_TORCH_URL
    "https://download.pytorch.org/libtorch/cpu/${_contorno_torch_archive}"
    CACHE STRING "URL of the fetched LibTorch archive")
set(CONTORNO_TORCH_SHA256 "${_contorno_torch_sha256}"
    CACHE STRING "SHA-256 of the fetched LibTorch archive")

find_package(Torch QUIET HINTS /opt/libtorch)

# buildkit's offline switches, as dev_dependency() reads them: BUILDKIT_OFFLINE
# or FETCHCONTENT_FULLY_DISCONNECTED, as a variable, cache entry or
# environment variable.
if(BUILDKIT_OFFLINE OR FETCHCONTENT_FULLY_DISCONNECTED
   OR "$CACHE{FETCHCONTENT_FULLY_DISCONNECTED}"
   OR "$ENV{BUILDKIT_OFFLINE}" OR "$ENV{FETCHCONTENT_FULLY_DISCONNECTED}")
    set(_contorno_torch_offline ON)
else()
    set(_contorno_torch_offline OFF)
endif()

set(_contorno_torch_deps ${CMAKE_BINARY_DIR}/_deps)
set(_contorno_torch_tree ${_contorno_torch_deps}/libtorch)

# Only the download depends on fetching being allowed: a LibTorch already
# extracted into the build tree (earlier download or pre-populated _deps) is
# used offline too.
if(NOT Torch_FOUND AND NOT EXISTS ${_contorno_torch_tree}/share/cmake/Torch
   AND CONTORNO_TORCH_FETCH AND NOT _contorno_torch_offline)
    set(_contorno_torch_zip ${_contorno_torch_deps}/libtorch.zip)
    set(_contorno_torch_tmp ${_contorno_torch_deps}/libtorch-extract)
    message(STATUS "LibTorch not found: downloading LibTorch ${CONTORNO_TORCH_VERSION}")
    # The hash is checked by hand: EXPECTED_HASH turns a mismatch into a
    # fatal error in some CMake versions, and configure must not fail. No
    # total TIMEOUT, which would cut a slow but working download; a stalled
    # connection ends after INACTIVITY_TIMEOUT.
    file(DOWNLOAD ${CONTORNO_TORCH_URL} ${_contorno_torch_zip}
        STATUS _contorno_torch_status TLS_VERIFY ON INACTIVITY_TIMEOUT 60)
    list(GET _contorno_torch_status 0 _contorno_torch_code)
    list(GET _contorno_torch_status 1 _contorno_torch_reason)
    if(_contorno_torch_code EQUAL 0)
        file(SHA256 ${_contorno_torch_zip} _contorno_torch_hash)
        if(NOT _contorno_torch_hash STREQUAL CONTORNO_TORCH_SHA256)
            set(_contorno_torch_code 1)
            set(_contorno_torch_reason "SHA-256 mismatch (${_contorno_torch_hash})")
        endif()
    endif()
    if(_contorno_torch_code EQUAL 0)
        # Extract into a scratch directory and move the tree into place only
        # when complete, so a failed extraction never leaves a partial tree
        # that later configures would take for a usable one. tar runs as a
        # child process: its failure cannot abort configure.
        file(REMOVE_RECURSE ${_contorno_torch_tmp})
        file(MAKE_DIRECTORY ${_contorno_torch_tmp})
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xf ${_contorno_torch_zip}
            WORKING_DIRECTORY ${_contorno_torch_tmp}
            RESULT_VARIABLE _contorno_torch_result
            ERROR_VARIABLE _contorno_torch_error
            OUTPUT_QUIET)
        if(_contorno_torch_result EQUAL 0
           AND EXISTS ${_contorno_torch_tmp}/libtorch/share/cmake/Torch)
            file(REMOVE_RECURSE ${_contorno_torch_tree})
            file(RENAME ${_contorno_torch_tmp}/libtorch ${_contorno_torch_tree})
        else()
            string(STRIP "${_contorno_torch_error}" _contorno_torch_error)
            set(_contorno_torch_code 1)
            set(_contorno_torch_reason
                "extraction failed (${_contorno_torch_result}) ${_contorno_torch_error}")
        endif()
        file(REMOVE_RECURSE ${_contorno_torch_tmp})
    endif()
    file(REMOVE ${_contorno_torch_zip})
    if(NOT _contorno_torch_code EQUAL 0)
        message(WARNING "LibTorch download failed: ${_contorno_torch_reason}")
    endif()
endif()

if(NOT Torch_FOUND AND EXISTS ${_contorno_torch_tree}/share/cmake/Torch)
    find_package(Torch QUIET HINTS ${_contorno_torch_tree} NO_DEFAULT_PATH)
endif()

if(Torch_FOUND)
    set(CONTORNO_WITH_TORCH ON)
    set(CONTORNO_TORCH_LIBRARIES ${TORCH_LIBRARIES})
    # Torch_DIR is <libtorch>/share/cmake/Torch.
    cmake_path(GET Torch_DIR PARENT_PATH _contorno_torch_root)
    cmake_path(GET _contorno_torch_root PARENT_PATH _contorno_torch_root)
    cmake_path(GET _contorno_torch_root PARENT_PATH CONTORNO_TORCH_ROOT)
    message(STATUS "LibTorch ${Torch_VERSION} found: ${Torch_DIR}")
else()
    set(CONTORNO_WITH_TORCH OFF)
    set(CONTORNO_TORCH_LIBRARIES)
    set(CONTORNO_TORCH_ROOT)
    message(WARNING "LibTorch not available: Vision is built without AI segmentation")
endif()
