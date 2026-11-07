# OpenCV for the Vision library (the only Contorno library allowed to use it).
#
# dev_dependency() uses an installed OpenCV when find_package finds one;
# otherwise, if CONTORNO_OPENCV_FETCH is ON and offline mode is off, it
# downloads the pinned release and builds only the modules Vision needs (core,
# imgproc, imgcodecs) with bundled image codecs.
#
# Result: CONTORNO_OPENCV_LIBRARIES, the targets to link against.

set(CONTORNO_OPENCV_VERSION "4.10.0" CACHE STRING "OpenCV release fetched when none is installed")
set(CONTORNO_OPENCV_SHA256
    "b2171af5be6b26f7a06b1229948bbb2bdaa74fcf5cd097e0af6378fce50a6eb9"
    CACHE STRING "SHA-256 of the fetched OpenCV source archive")
option(CONTORNO_OPENCV_FETCH "Fetch and build OpenCV when find_package fails" ON)

set(_contorno_opencv_modules core imgproc imgcodecs)

set(_contorno_opencv_fetch)
if(CONTORNO_OPENCV_FETCH)
    # Build only what Vision needs, and nothing that downloads more at
    # configure time (IPP, ADE) or pulls in system GUI/video libraries.
    set(_contorno_opencv_fetch
        FETCH_URL    https://github.com/opencv/opencv/archive/refs/tags/${CONTORNO_OPENCV_VERSION}.tar.gz
        FETCH_SHA256 ${CONTORNO_OPENCV_SHA256}
        FETCH_CACHE  BUILD_LIST=core,imgproc,imgcodecs)
    foreach(_off
            BUILD_TESTS BUILD_PERF_TESTS BUILD_EXAMPLES BUILD_DOCS BUILD_opencv_apps
            BUILD_opencv_python2 BUILD_opencv_python3 BUILD_JAVA BUILD_opencv_java
            BUILD_PROTOBUF WITH_PROTOBUF WITH_IPP WITH_ITT WITH_ADE WITH_OPENCL
            WITH_FFMPEG WITH_GSTREAMER WITH_GTK WITH_QT WITH_V4L WITH_VTK WITH_EIGEN
            WITH_LAPACK WITH_OPENEXR WITH_WEBP WITH_OPENJPEG WITH_JASPER WITH_AVIF
            WITH_1394 WITH_CUDA)
        list(APPEND _contorno_opencv_fetch ${_off}=OFF)
    endforeach()
    foreach(_on BUILD_ZLIB BUILD_PNG BUILD_JPEG BUILD_TIFF)
        list(APPEND _contorno_opencv_fetch ${_on}=ON)
    endforeach()
endif()

set(_contorno_opencv_hint "Debian: apt install libopencv-dev; or set OpenCV_DIR, or -DBUILD_VISION=OFF")
if(NOT CONTORNO_OPENCV_FETCH)
    string(APPEND _contorno_opencv_hint "; or -DCONTORNO_OPENCV_FETCH=ON")
endif()
dev_dependency(OpenCV VERSION 4 REQUIRED
    COMPONENTS ${_contorno_opencv_modules}
    ${_contorno_opencv_fetch}
    INSTALL_HINT "${_contorno_opencv_hint}")

if(OpenCV_FOUND)
    set(CONTORNO_OPENCV_LIBRARIES ${OpenCV_LIBS})
    return()
endif()

# In-tree OpenCV targets do not carry their include directories; collect them
# in one interface target so Vision links a single name either way.
FetchContent_GetProperties(OpenCV)
add_library(contorno_opencv INTERFACE)
target_include_directories(contorno_opencv SYSTEM INTERFACE
    ${OPENCV_CONFIG_FILE_INCLUDE_DIR}
    ${opencv_SOURCE_DIR}/include
)
foreach(_module IN LISTS _contorno_opencv_modules)
    target_include_directories(contorno_opencv SYSTEM INTERFACE
        ${OPENCV_MODULE_opencv_${_module}_LOCATION}/include)
    target_link_libraries(contorno_opencv INTERFACE opencv_${_module})
endforeach()

set(CONTORNO_OPENCV_LIBRARIES contorno_opencv)
