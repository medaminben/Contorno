# CPack pre-build script (CPACK_PRE_BUILD_SCRIPTS): removes the development
# files from the staged tree before it is packaged.
set(_root "${CPACK_TEMPORARY_INSTALL_DIRECTORY}${CPACK_PACKAGING_INSTALL_PREFIX}")
set(_lib "${_root}/${CPACK_CONTORNO_LIBDIR}")
file(GLOB _static "${_lib}/*.a")
# the libraries' unversioned links: only a linker uses them, the program loads libX.so.1
file(GLOB _candidates "${_lib}/*.so")
set(_namelinks "")
foreach(_link IN LISTS _candidates)
    if(IS_SYMLINK "${_link}")
        list(APPEND _namelinks "${_link}")
    endif()
endforeach()
file(REMOVE_RECURSE "${_root}/include" "${_lib}/cmake" "${_lib}/pkgconfig" ${_static} ${_namelinks})
