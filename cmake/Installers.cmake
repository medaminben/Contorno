# Installers of the Indicator: a Debian package on Linux (cpack -G DEB in the
# build directory) and an Inno Setup installer on Windows (target
# Indicator_installer). Both carry the program, its libraries, the IS-Net
# model, the manual and the article the manual links to. The sample photos,
# the sources and the development files (headers, CMake package, import
# libraries) are not installed: they are in the repository.
#
# The installer presets (deb, windows-installer) set the library directory and
# the RPATH; CONTORNO_ISNET_MODEL names the exported model to ship.
if(NOT TARGET Indicator)
    return()
endif()

set(CPACK_PACKAGE_NAME contorno-indicator)
set(CPACK_PACKAGE_VENDOR "Mohamed Lamine Ben Hassine")
set(CPACK_PACKAGE_CONTACT "Mohamed Lamine Ben Hassine <mohamed.ben-hassine@mailfence.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Measures the surface of a flat animal hide from one photograph")
set(CPACK_PACKAGE_HOMEPAGE_URL https://github.com/medaminben/Contorno)
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)
# Debian sorts a ~ suffix before the release: 1.2.0~beta.1 < 1.2.0
string(REPLACE "-" "~" CPACK_PACKAGE_VERSION "${CONTORNO_VERSION_FULL}")
set(CPACK_GENERATOR DEB)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION graphics)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS usr/${CMAKE_INSTALL_LIBDIR})
# the Help key opens the manual in a Chromium --app window
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS chromium)
set(CPACK_CONTORNO_LIBDIR ${CMAKE_INSTALL_LIBDIR})
set(CPACK_PRE_BUILD_SCRIPTS ${CMAKE_CURRENT_LIST_DIR}/InstallerPrune.cmake)
include(CPack)

if(WIN32)
    # the MSVC runtime, for machines without the Visual C++ redistributable
    include(InstallRequiredSystemLibraries)
    set(_runtime_dirs "$<TARGET_FILE_DIR:Indicator>" "${CMAKE_BINARY_DIR}/bin")
    if(CONTORNO_TORCH_ROOT)
        list(APPEND _runtime_dirs "${CONTORNO_TORCH_ROOT}/lib")
    endif()
    foreach(_vcpkg IN ITEMS "${VCPKG_INSTALLED_DIR}" "${_VCPKG_INSTALLED_DIR}")
        if(_vcpkg AND VCPKG_TARGET_TRIPLET)
            list(APPEND _runtime_dirs "${_vcpkg}/${VCPKG_TARGET_TRIPLET}/bin")
        endif()
    endforeach()
    install(CODE "set(CONTORNO_RUNTIME_EXE \"$<TARGET_FILE:Indicator>\")
set(CONTORNO_RUNTIME_DIRS \"${_runtime_dirs}\")")
    install(SCRIPT ${CMAKE_CURRENT_LIST_DIR}/InstallRuntime.cmake)

    buildkit_add_stage_target(Indicator)
    buildkit_add_inno_installer(Indicator
        ISS ${CMAKE_SOURCE_DIR}/applications/Indicator/resources/indicator.iss
        VERSION ${CONTORNO_VERSION_FULL})
endif()
