# Contorno Indicator: the outline-and-area desktop application, on wxWidgets,
# built on the Vision and Inspection libraries. Skipped without wxWidgets.
if(NOT TARGET Contorno::Inspection)
    return()
endif()

# wxWidgets is optional: without it this app and its tests are skipped.
if(TARGET wx::core AND TARGET wx::base)
    set(wxWidgets_AVAILABLE TRUE)
else()
    dev_dependency(wxWidgets VERSION 3.2 OPTIONAL COMPONENTS core base)
endif()
if(NOT wxWidgets_AVAILABLE)
    return()
endif()
if(TARGET wx::core AND TARGET wx::base)
    set(INDICATOR_WX_LIBS wx::core wx::base)       # wxWidgets' CMake package
else()
    set(INDICATOR_WX_LIBS wxWidgets::wxWidgets)    # CMake's FindwxWidgets (wx-config)
endif()

# The toolkit-free model. The seven string tables live in strings/ and compile
# against this model's Language.h.
add_library(indicator_model STATIC
    src/model/Analyser.cpp
    src/model/CameraMessage.cpp
    src/model/DataFolder.cpp
    src/model/ImageSize.cpp
    src/model/IndicatorSettings.cpp
    src/model/Inspection.cpp
    src/model/Language.cpp
    src/model/LibraryText.cpp
    src/model/Measure.cpp
    src/model/ManualLocation.cpp
    src/model/Message.cpp
    src/model/ModelPath.cpp
    src/model/SettingsStore.cpp
    strings/indicator_en.cpp strings/indicator_de.cpp
    strings/indicator_es.cpp strings/indicator_it.cpp
    strings/indicator_fr.cpp strings/indicator_nl.cpp
    strings/indicator_ar.cpp
)
target_include_directories(indicator_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(indicator_model PUBLIC Contorno::Tools Contorno::Vision Contorno::Inspection)
target_compile_features(indicator_model PUBLIC cxx_std_20)

# wx's own texts (the file dialog's), embedded when the machine has the catalogues.
include(cmake/WxCatalogue.cmake)
indicator_embed_wx_catalogue(INDICATOR_WX_CATALOGUE_CPP)

# The window icon and the panel's wordmark, compiled into the library. A runtime path
# is invisible once the process is not started from the source tree, and Wayland does
# not ask the window for a file.
set(INDICATOR_ICON_CPP ${CMAKE_CURRENT_BINARY_DIR}/indicator_icon_data.cpp)
set(INDICATOR_ICON_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/contorno-indicator-dark.png
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/contorno-indicator-light.png
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/contorno-indicator-dark.svg
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/contorno-indicator-light.svg
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/contorno-indicator-wordmark.svg)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${INDICATOR_ICON_FILES})
function(indicator_icon_array path symbol)
    file(READ "${path}" _hex HEX)
    file(SIZE "${path}" _len)
    string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1," _bytes "${_hex}")
    # a trailing 0 so the SVG can be read as a C string; PNG uses _len and ignores it
    # extern: a namespace-scope const has internal linkage and the linker would not see it
    set(_piece "extern unsigned char const ${symbol}[] = {${_bytes}0};\nextern unsigned int const ${symbol}_len = ${_len};\n" PARENT_SCOPE)
endfunction()
indicator_icon_array(${CMAKE_CURRENT_SOURCE_DIR}/resources/contorno-indicator-dark.png indicator_icon_dark_png)
set(_icon_cpp "${_piece}")
indicator_icon_array(${CMAKE_CURRENT_SOURCE_DIR}/resources/contorno-indicator-light.png indicator_icon_light_png)
string(APPEND _icon_cpp "${_piece}")
indicator_icon_array(${CMAKE_CURRENT_SOURCE_DIR}/resources/contorno-indicator-dark.svg indicator_icon_dark_svg)
string(APPEND _icon_cpp "${_piece}")
indicator_icon_array(${CMAKE_CURRENT_SOURCE_DIR}/resources/contorno-indicator-light.svg indicator_icon_light_svg)
string(APPEND _icon_cpp "${_piece}")
indicator_icon_array(${CMAKE_CURRENT_SOURCE_DIR}/resources/contorno-indicator-wordmark.svg indicator_wordmark_svg)
string(APPEND _icon_cpp "${_piece}")
file(WRITE ${INDICATOR_ICON_CPP} "${_icon_cpp}")

# The wx UI: the main window, theme, drawn keys, themed dialogs, icon and settings file.
add_library(indicator_ui STATIC
    ${INDICATOR_WX_CATALOGUE_CPP}
    ${INDICATOR_ICON_CPP}
    src/ui/AppIcon.cpp
    src/ui/Catalogue.cpp
    src/ui/CameraDialog.cpp
    src/ui/FileDialog.cpp
    src/ui/FlagIcons.cpp
    src/ui/Fonts.cpp
    src/ui/ControlPanel.cpp
    src/ui/Key.cpp
    src/ui/Localise.cpp
    src/ui/MainWindow.cpp
    src/ui/MainWindowFiles.cpp
    src/ui/MessageLog.cpp
    src/ui/NumberField.cpp
    src/ui/Paint.cpp
    src/ui/PhotoFile.cpp
    src/ui/PhotoView.cpp
    src/ui/ProgressBar.cpp
    src/ui/SegmentDisplay.cpp
    src/ui/Theme.cpp
    src/ui/WxConfigStore.cpp
)
target_include_directories(indicator_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(INDICATOR_WX_CATALOGUE_CPP)
    target_compile_definitions(indicator_ui PRIVATE INDICATOR_WX_CATALOGUES)
endif()
target_link_libraries(indicator_ui PUBLIC indicator_model ${INDICATOR_WX_LIBS})
if(WIN32)
    # the dark title bar (DWM) and the dark styles of the native fields (UxTheme)
    target_link_libraries(indicator_ui PRIVATE dwmapi uxtheme)
endif()
target_compile_features(indicator_ui PUBLIC cxx_std_20)
# GTK's own style classes carry the fields' frame and selection colours, which wx cannot set.
if(UNIX AND NOT APPLE)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(INDICATOR_GTK3 IMPORTED_TARGET gtk+-3.0)
    endif()
    if(TARGET PkgConfig::INDICATOR_GTK3)
        target_link_libraries(indicator_ui PUBLIC PkgConfig::INDICATOR_GTK3)
        target_compile_definitions(indicator_ui PUBLIC INDICATOR_GTK3)
    endif()
endif()
target_compile_definitions(indicator_ui PRIVATE
    INDICATOR_MANUAL_DIR="${CMAKE_SOURCE_DIR}/docs/manual"
    # where the build put the IS-Net model (empty without AI) and the samples the file dialogs start in;
    # an installed model is found first at run time
    INDICATOR_DEFAULT_MODEL="${CONTORNO_ISNET_MODEL}"
    INDICATOR_SAMPLES_DIR="${CMAKE_SOURCE_DIR}/docs/samples")

create_application(
    NAME         Indicator
    ENTRY        WX_ui
    SOURCES      src/main.cpp
    DEPENDENCIES indicator_ui
)
target_compile_definitions(Indicator PRIVATE INDICATOR_VERSION="${CONTORNO_VERSION_FULL}")
# the file the user runs: the package's name, not a bare "Indicator" in /usr/bin
set_target_properties(Indicator PROPERTIES OUTPUT_NAME contorno-indicator)

if(BUILD_TESTING AND TARGET Indicator)
    build_gtest_executable(
        NAME test_indicator_model
        SRC  test/src/model/test_Layers.cpp test/src/model/test_Language.cpp
             test/src/model/test_LibraryText.cpp test/src/model/test_CameraMessage.cpp
             test/src/model/test_Measure.cpp
             test/src/model/test_ModelPath.cpp test/src/model/test_ManualLocation.cpp
             test/src/model/test_DataFolder.cpp
             test/src/model/test_ImageSize.cpp test/src/model/test_Inspection.cpp
             test/src/model/test_Analyser.cpp test/src/model/test_Settings.cpp test/src/model/test_Utf8Path.cpp test/src/model/test_TempDir.cpp
             test/src/model/TestStrings.cpp
        DEPENDS indicator_model GTest::gtest_main
        DISCOVER ON
    )
    target_include_directories(test_indicator_model PRIVATE test/src)
    target_compile_definitions(test_indicator_model PRIVATE
                               INDICATOR_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src"
                               INDICATOR_SAMPLES_DIR="${CMAKE_SOURCE_DIR}/docs/samples"
                               INDICATOR_DEFAULT_MODEL="${CONTORNO_ISNET_MODEL}"
                               CONTORNO_LIBRARIES_DIR="${CMAKE_SOURCE_DIR}/libraries")
endif()

if(BUILD_TESTING AND TARGET Indicator)
    # Needs a display (ctest sets GDK_BACKEND=x11; the main sets it for a direct run).
    build_gtest_executable(
        NAME test_indicator
        SRC  test/src/ui/test_main.cpp test/src/ui/WxTestApp.cpp
             test/src/ui/test_Theme.cpp test/src/ui/test_AppIcon.cpp test/src/ui/test_FileDialog.cpp
             test/src/ui/test_SegmentDisplay.cpp test/src/ui/test_MessageLog.cpp
             test/src/ui/test_Language.cpp test/src/ui/test_Panel.cpp
             test/src/ui/test_SetUpDialog.cpp test/src/ui/test_Window.cpp test/src/ui/test_Fonts.cpp
             test/src/ui/test_Catalogue.cpp
        DEPENDS indicator_ui GTest::gtest
        DISCOVER ON
    )
    target_include_directories(test_indicator PRIVATE test/src)
    target_compile_definitions(test_indicator PRIVATE INDICATOR_SAMPLES_DIR="${CMAKE_SOURCE_DIR}/docs/samples"
                               INDICATOR_MANUAL_DIR="${CMAKE_SOURCE_DIR}/docs/manual"
                               INDICATOR_DEFAULT_MODEL="${CONTORNO_ISNET_MODEL}")
    add_test(NAME indicator_self_test
             COMMAND Indicator --self-test ${CMAKE_SOURCE_DIR}/docs/samples/leather/calf_black.jpg
                     ${CMAKE_BINARY_DIR}/indicator_self_test)
    # 77: the AI outline is unavailable (no LibTorch or no model): reported as skipped. A display is needed.
    set_tests_properties(indicator_self_test PROPERTIES ENVIRONMENT "GDK_BACKEND=x11" TIMEOUT 300
                         SKIP_RETURN_CODE 77)
    # The user manual: every page in every language, its links, and every label it
    # quotes shown by the application in that language; the pages built from the sources.
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME indicator_manual COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/docs/manual/build_manual.py --check)
        add_test(NAME samples_sources COMMAND Python3::Interpreter -I ${CMAKE_SOURCE_DIR}/docs/samples/check_sources.py)
    endif()
endif()

# Platform integration: desktop entry, icons, and the installed model, manual and articles.
if(TARGET Indicator)
    if(CONTORNO_ISNET_MODEL)
        install(FILES ${CONTORNO_ISNET_MODEL} DESTINATION ${CMAKE_INSTALL_DATADIR}/contorno/models OPTIONAL)
    endif()
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/docs/manual/ DESTINATION ${CMAKE_INSTALL_DATADIR}/contorno/manual
            PATTERN "content" EXCLUDE
            PATTERN "template.html" EXCLUDE
            PATTERN "build_manual.py" EXCLUDE)
    set(_article ${CMAKE_SOURCE_DIR}/docs/articles/contorno-area-measurement)
    install(FILES ${_article}/contorno-area-measurement.html ${_article}/article.css
            DESTINATION ${CMAKE_INSTALL_DATADIR}/contorno/articles/contorno-area-measurement)
    install(DIRECTORY ${_article}/figures DESTINATION ${CMAKE_INSTALL_DATADIR}/contorno/articles/contorno-area-measurement)
    set(_paper ${CMAKE_SOURCE_DIR}/docs/articles/contorno-indicator)
    install(FILES ${_paper}/contorno-indicator.html ${_paper}/paper.css DESTINATION ${CMAKE_INSTALL_DATADIR}/contorno/articles/contorno-indicator)
    install(DIRECTORY ${_paper}/figures DESTINATION ${CMAKE_INSTALL_DATADIR}/contorno/articles/contorno-indicator)
    # the photos the file dialogs open first, with their sources (one is CC BY: the credit travels with it)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/docs/samples/ DESTINATION ${CMAKE_INSTALL_DATADIR}/contorno/samples
            PATTERN "check_sources.py" EXCLUDE)
endif()

if(TARGET Indicator AND WIN32)
    # the executable's icon (wx.rc brings wx's own resources)
    target_sources(Indicator PRIVATE resources/indicator.rc)
elseif(TARGET Indicator AND APPLE)
    set(INDICATOR_ICNS ${CMAKE_CURRENT_SOURCE_DIR}/resources/indicator.icns)
    set_source_files_properties(${INDICATOR_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(Indicator PRIVATE ${INDICATOR_ICNS})
    set_target_properties(Indicator PROPERTIES MACOSX_BUNDLE_ICON_FILE indicator.icns)
elseif(TARGET Indicator AND UNIX)
    # Linux desktops take the launcher and taskbar icon from the desktop entry.
    # KDE Wayland paints both the taskbar and the title-bar icon from this entry
    # (Icon= names the theme, the file name is the Wayland app id).
    install(FILES resources/contorno-indicator.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    # the C in the text colour of the panel or title bar on KDE, black elsewhere
    install(FILES resources/contorno-indicator.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
    install(FILES resources/contorno-indicator-symbolic.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/symbolic/apps)
    # PNG sizes for desktops that do not take the scalable SVG.
    find_program(RSVG_CONVERT rsvg-convert)
    set(INDICATOR_ICON_SIZES 16 24 32 48 64 128 256)
    set(INDICATOR_ICON_PNGS "")
    if(RSVG_CONVERT)
        foreach(s ${INDICATOR_ICON_SIZES})
            set(_png ${CMAKE_CURRENT_BINARY_DIR}/icons/hicolor/${s}x${s}/apps/contorno-indicator.png)
            add_custom_command(
                OUTPUT ${_png}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/icons/hicolor/${s}x${s}/apps
                COMMAND ${RSVG_CONVERT} -w ${s} -h ${s}
                        ${CMAKE_CURRENT_SOURCE_DIR}/resources/contorno-indicator.svg -o ${_png}
                DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/resources/contorno-indicator.svg
                VERBATIM)
            list(APPEND INDICATOR_ICON_PNGS ${_png})
            install(FILES ${_png} DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${s}x${s}/apps)
        endforeach()
    endif()
    if(INDICATOR_ICON_PNGS)
        add_custom_target(Indicator_icons DEPENDS ${INDICATOR_ICON_PNGS})
    else()
        add_custom_target(Indicator_icons)
    endif()
    # Registers the build tree's Indicator for the current user (~/.local/share),
    # so Wayland desktops show its icon without an install. Building Indicator
    # does this; undo by deleting the copied files.
    set(INDICATOR_USER_DATA "$ENV{HOME}/.local/share")
    file(READ resources/contorno-indicator.desktop INDICATOR_DESKTOP)
    string(REPLACE "Exec=contorno-indicator" "Exec=$<TARGET_FILE:Indicator>" INDICATOR_DESKTOP "${INDICATOR_DESKTOP}")
    file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/contorno-indicator.desktop CONTENT "${INDICATOR_DESKTOP}")
    set(INDICATOR_DESKTOP_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E make_directory ${INDICATOR_USER_DATA}/applications
                ${INDICATOR_USER_DATA}/icons/hicolor/scalable/apps
                ${INDICATOR_USER_DATA}/icons/hicolor/symbolic/apps
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/contorno-indicator.desktop
                ${INDICATOR_USER_DATA}/applications/contorno-indicator.desktop
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/resources/contorno-indicator.svg
                ${INDICATOR_USER_DATA}/icons/hicolor/scalable/apps/contorno-indicator.svg
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/resources/contorno-indicator-symbolic.svg
                ${INDICATOR_USER_DATA}/icons/hicolor/symbolic/apps/contorno-indicator-symbolic.svg)
    if(RSVG_CONVERT)
        foreach(s ${INDICATOR_ICON_SIZES})
            list(APPEND INDICATOR_DESKTOP_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E make_directory ${INDICATOR_USER_DATA}/icons/hicolor/${s}x${s}/apps
                COMMAND ${CMAKE_COMMAND} -E copy
                        ${CMAKE_CURRENT_BINARY_DIR}/icons/hicolor/${s}x${s}/apps/contorno-indicator.png
                        ${INDICATOR_USER_DATA}/icons/hicolor/${s}x${s}/apps/contorno-indicator.png)
        endforeach()
    endif()
    find_program(UPDATE_DESKTOP_DATABASE update-desktop-database)
    if(UPDATE_DESKTOP_DATABASE)
        list(APPEND INDICATOR_DESKTOP_COMMANDS
            COMMAND ${UPDATE_DESKTOP_DATABASE} ${INDICATOR_USER_DATA}/applications)
    endif()
    add_custom_target(Indicator_desktop_entry
        ${INDICATOR_DESKTOP_COMMANDS}
        DEPENDS Indicator_icons
        COMMENT "Registering Indicator's desktop entry and icon in ${INDICATOR_USER_DATA}"
        VERBATIM)
    add_dependencies(Indicator Indicator_desktop_entry)
endif()
