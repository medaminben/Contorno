# CPack pre-build script (CPACK_PRE_BUILD_SCRIPTS): removes the development
# files from the staged tree before it is packaged.
set(_root "${CPACK_TEMPORARY_INSTALL_DIRECTORY}${CPACK_PACKAGING_INSTALL_PREFIX}")
set(_lib "${_root}/${CPACK_CONTORNO_LIBDIR}")
file(GLOB _static "${_lib}/*.a")
file(REMOVE_RECURSE "${_root}/include" "${_lib}/cmake" "${_lib}/pkgconfig" ${_static})
