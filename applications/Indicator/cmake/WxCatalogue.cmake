# wxWidgets' own message catalogue, embedded in the executable.
#
# Some of the text on screen is wxWidgets', not this application's: the generic
# file dialog's buttons and column headers come out of wxstd-3.2.mo (package
# wx3.2-i18n on Debian). A Windows or macOS install has no catalogue on disk to
# find at run time, so the languages the application speaks are compiled in.
# English needs none: it is what the sources are written in.
#
# indicator_embed_wx_catalogue(<out_var>) sets <out_var> to a generated .cpp
# holding the bytes of every catalogue found, or to "" with a warning when
# there is none (wx's own strings then stay English).
#
# INDICATOR_WX_LOCALE_DIR is the locale directory searched first:
# <dir>/<lang>/LC_MESSAGES/wxstd-3.2.mo, else wxstd.mo. A language not found
# there is downloaded as wxWidgets' own locale/<lang>.po, from the tag of the
# wxWidgets version in use, into the build tree and compiled with msgfmt.
# That is skipped (with a warning) when offline (BUILDKIT_OFFLINE or
# FETCHCONTENT_FULLY_DISCONNECTED, as buildkit reads them) or without msgfmt;
# a failed download stops the rest (one warning). The v3.2.8 files are
# checked against pinned SHA-256 hashes.
set(INDICATOR_WX_LOCALE_DIR "/usr/share/locale" CACHE PATH
    "Locale directory holding wx's wxstd-3.2.mo catalogues, which are embedded in Indicator")

# The version of the wxWidgets in use (major.minor.release), or "".
function(_indicator_wx_version out_variable)
    set(_version "")
    if(wxWidgets_CONFIG_EXECUTABLE)
        execute_process(COMMAND "${wxWidgets_CONFIG_EXECUTABLE}" --version-full
                        OUTPUT_VARIABLE _version OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    endif()
    if(NOT _version)
        set(_dirs "")
        if(TARGET wx::core)
            get_target_property(_dirs wx::core INTERFACE_INCLUDE_DIRECTORIES)
        endif()
        find_file(_wx_version_h wx/version.h HINTS ${_dirs} ${wxWidgets_INCLUDE_DIRS} NO_DEFAULT_PATH)
        if(_wx_version_h)
            file(STRINGS "${_wx_version_h}" _lines REGEX "^#define wxM(AJOR|INOR)_VERSION|^#define wxRELEASE_NUMBER")
            foreach(_line IN LISTS _lines)
                if(_line MATCHES "wxMAJOR_VERSION +([0-9]+)")
                    set(_major ${CMAKE_MATCH_1})
                elseif(_line MATCHES "wxMINOR_VERSION +([0-9]+)")
                    set(_minor ${CMAKE_MATCH_1})
                elseif(_line MATCHES "wxRELEASE_NUMBER +([0-9]+)")
                    set(_release ${CMAKE_MATCH_1})
                endif()
            endforeach()
            if(DEFINED _major AND DEFINED _minor AND DEFINED _release)
                set(_version "${_major}.${_minor}.${_release}")
            endif()
        endif()
    endif()
    # wx-config prints 3.2.8; a version with a suffix is cut to its numbers
    if(_version MATCHES "^([0-9]+\\.[0-9]+\\.[0-9]+)")
        set(_version "${CMAKE_MATCH_1}")
    else()
        set(_version "")
    endif()
    set(${out_variable} "${_version}" PARENT_SCOPE)
endfunction()

# SHA-256 of wxWidgets v3.2.8's locale/<lang>.po. Another version is downloaded
# unverified (with one warning); add its hashes here to verify it.
set(_INDICATOR_PO_SHA256_3.2.8_de c5d18f5b24f96839bcd2a16167a8d7991c3a03fc1f952bf1d327f1f6c1145f65)
set(_INDICATOR_PO_SHA256_3.2.8_es 43369b19d65e031e4f7bee5a8e67b7bb42b80f53af6d96b7ec8c2bb244bbb35a)
set(_INDICATOR_PO_SHA256_3.2.8_it 563ce9aa75bc0cb484bc6581aaca8c2a2e09e197080e084c4fd57403cf38410c)
set(_INDICATOR_PO_SHA256_3.2.8_fr b385bf06e9609e77c01fe2d966b9e397471bf754d874a32e55b1c09aaf291f51)
set(_INDICATOR_PO_SHA256_3.2.8_nl 7c8d4dc1467a785687a34bed4f335b2e47fd0f8937a719d6617124e7977517a1)
set(_INDICATOR_PO_SHA256_3.2.8_ar b6e35f783bb7e282ac2e26e724e7f0cffd11ca8105bf540d8f6ef6dad47631eb)

set(INDICATOR_WX_PO_BASE_URL "https://raw.githubusercontent.com/wxWidgets/wxWidgets" CACHE STRING
    "Where wx's locale/<lang>.po are downloaded from: <base>/v<wx version>/locale/<lang>.po")
mark_as_advanced(INDICATOR_WX_PO_BASE_URL)

# Warns once per configure; the later languages are then skipped silently.
function(_indicator_stop_downloads reason)
    get_property(_stopped GLOBAL PROPERTY INDICATOR_PO_STOPPED)
    if(NOT _stopped)
        set_property(GLOBAL PROPERTY INDICATOR_PO_STOPPED ON)
        message(WARNING "wx catalogue: ${reason}; the languages not found locally are not downloaded")
    endif()
endfunction()

# Downloads wxWidgets' locale/<lang>.po for the wx version in use and compiles
# it; sets <out_variable> to the .mo, or to "" on any failure or when the
# downloads are stopped. The first problem that applies to every language
# (offline, no msgfmt, unknown version, a failed download) stops them all.
function(_indicator_fetch_wx_catalogue lang out_variable)
    set(${out_variable} "" PARENT_SCOPE)
    _indicator_wx_version(_wx_version)
    if(NOT _wx_version)
        _indicator_stop_downloads("the wxWidgets version is unknown")
        return()
    endif()
    set(_dir "${CMAKE_CURRENT_BINARY_DIR}/wx_catalogue/${_wx_version}")
    set(_po "${_dir}/${lang}.po")
    set(_mo "${_dir}/${lang}/wxstd.mo")
    # compiled by an earlier configure: kept even when offline
    if(EXISTS "${_mo}")
        set(${out_variable} "${_mo}" PARENT_SCOPE)
        return()
    endif()
    get_property(_stopped GLOBAL PROPERTY INDICATOR_PO_STOPPED)
    if(_stopped)
        return()
    endif()
    if(BUILDKIT_OFFLINE OR FETCHCONTENT_FULLY_DISCONNECTED
       OR "$CACHE{FETCHCONTENT_FULLY_DISCONNECTED}"
       OR "$ENV{BUILDKIT_OFFLINE}" OR "$ENV{FETCHCONTENT_FULLY_DISCONNECTED}")
        _indicator_stop_downloads("offline")
        return()
    endif()
    find_program(INDICATOR_MSGFMT msgfmt)
    if(NOT INDICATOR_MSGFMT)
        _indicator_stop_downloads("msgfmt is missing (install gettext)")
        return()
    endif()
    if(NOT EXISTS "${_mo}")
        set(_expected "")
        if(DEFINED _INDICATOR_PO_SHA256_${_wx_version}_${lang})
            set(_expected "${_INDICATOR_PO_SHA256_${_wx_version}_${lang}}")
        else()
            get_property(_warned GLOBAL PROPERTY INDICATOR_PO_UNVERIFIED_WARNED)
            if(NOT _warned)
                set_property(GLOBAL PROPERTY INDICATOR_PO_UNVERIFIED_WARNED ON)
                message(WARNING "wx catalogue: no known checksums for wxWidgets ${_wx_version}; "
                                "the downloaded catalogues are unverified")
            endif()
        endif()
        set(_url "${INDICATOR_WX_PO_BASE_URL}/v${_wx_version}/locale/${lang}.po")
        file(DOWNLOAD "${_url}" "${_po}" STATUS _status TLS_VERIFY ON TIMEOUT 60)
        list(GET _status 0 _code)
        if(NOT _code EQUAL 0)
            list(GET _status 1 _text)
            file(REMOVE "${_po}")
            _indicator_stop_downloads("download of ${_url} failed (${_text})")
            return()
        endif()
        # checked here, not with EXPECTED_HASH: that makes a failed connection a fatal error
        if(_expected)
            file(SHA256 "${_po}" _actual)
            if(NOT _actual STREQUAL _expected)
                file(REMOVE "${_po}")
                _indicator_stop_downloads("${_url} does not match its pinned SHA-256 (${_actual})")
                return()
            endif()
        endif()
        file(MAKE_DIRECTORY "${_dir}/${lang}")
        # compiled beside, then renamed: an interrupted run leaves no .mo that would be trusted
        file(REMOVE "${_mo}.tmp")
        execute_process(COMMAND "${INDICATOR_MSGFMT}" -o "${_mo}.tmp" "${_po}"
                        RESULT_VARIABLE _result ERROR_VARIABLE _error)
        if(NOT _result EQUAL 0)
            file(REMOVE "${_mo}.tmp")
            message(WARNING "wx catalogue ${lang}: msgfmt failed: ${_error}")
            return()
        endif()
        file(RENAME "${_mo}.tmp" "${_mo}")
        message(STATUS "wx catalogue ${lang}: downloaded ${_url} and compiled it")
    endif()
    set(${out_variable} "${_mo}" PARENT_SCOPE)
endfunction()

function(indicator_embed_wx_catalogue out_variable)
    set(${out_variable} "" PARENT_SCOPE)
    set(_entries "")
    set(_arrays "")
    set(_count 0)
    foreach(_lang de es it fr nl ar)
        set(_mo "")
        foreach(_name wxstd-3.2.mo wxstd.mo)
            set(_candidate "${INDICATOR_WX_LOCALE_DIR}/${_lang}/LC_MESSAGES/${_name}")
            if(EXISTS "${_candidate}")
                set(_mo "${_candidate}")
                break()
            endif()
        endforeach()
        if(NOT _mo)
            _indicator_fetch_wx_catalogue(${_lang} _mo)
        endif()
        if(NOT _mo)
            continue()
        endif()
        # a changed catalogue re-runs CMake instead of leaving stale bytes in the executable
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_mo}")
        file(READ "${_mo}" _hex HEX)
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _bytes "${_hex}")
        string(LENGTH "${_hex}" _hex_length)
        math(EXPR _size "${_hex_length} / 2")
        string(APPEND _arrays "static const unsigned char bytes_${_lang}[] = {${_bytes}};\n")
        string(APPEND _entries "    {\"${_lang}\", bytes_${_lang}, ${_size}},\n")
        math(EXPR _count "${_count} + 1")
    endforeach()

    if(_count EQUAL 0)
        message(WARNING "wx catalogue: none found under ${INDICATOR_WX_LOCALE_DIR} or downloaded; "
                        "wx's own strings stay English (install wx3.2-i18n or set INDICATOR_WX_LOCALE_DIR)")
        return()
    endif()

    set(_cpp "${CMAKE_CURRENT_BINARY_DIR}/wx_catalogue/wx_catalogue.cpp")
    file(WRITE "${_cpp}.tmp"
         "// Generated by cmake/WxCatalogue.cmake: do not edit.\n"
         "#include <cstddef>\n"
         "namespace indicator {\n"
         "struct EmbeddedCatalogue {\n"
         "    char const* language;\n"
         "    unsigned char const* data;\n"
         "    std::size_t size;\n"
         "};\n"
         "namespace {\n${_arrays}}\n"
         "extern const EmbeddedCatalogue embedded_wx_catalogues[];\n"
         "extern const std::size_t embedded_wx_catalogue_count;\n"
         "const EmbeddedCatalogue embedded_wx_catalogues[] = {\n${_entries}};\n"
         "const std::size_t embedded_wx_catalogue_count = ${_count};\n"
         "}\n")
    # rewritten only when it changed, so an unchanged catalogue does not recompile
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_cpp}.tmp" "${_cpp}")
    message(STATUS "wx catalogue: embedding ${_count} language(s)")
    set(${out_variable} "${_cpp}" PARENT_SCOPE)
endfunction()
