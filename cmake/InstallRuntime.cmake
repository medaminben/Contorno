# Install-time script (Windows): copies the DLLs the Indicator loads, its own,
# LibTorch's, OpenCV's and wxWidgets', next to it. Windows' own DLLs stay out.
# Set by Installers.cmake: CONTORNO_RUNTIME_EXE, CONTORNO_RUNTIME_DIRS.
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${CONTORNO_RUNTIME_EXE}"
    RESOLVED_DEPENDENCIES_VAR _deps
    UNRESOLVED_DEPENDENCIES_VAR _missing
    DIRECTORIES ${CONTORNO_RUNTIME_DIRS}
    PRE_EXCLUDE_REGEXES "^api-ms-" "^ext-ms-"
    POST_EXCLUDE_REGEXES "[/\\\\][Ss]ystem32[/\\\\]")
if(_missing)
    message(WARNING "Indicator: DLLs not found: ${_missing}")
endif()
file(INSTALL ${_deps} DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
