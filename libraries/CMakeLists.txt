# Library roots. Add new libraries via CONTORNO_LIBRARIES in the top-level
# CMakeLists.txt (GENERATE_SCAFFOLD) or by hand here.
add_subdirectory(Core)
add_subdirectory(Pot)
add_subdirectory(Tools)
if(BUILD_VISION)
    add_subdirectory(Vision)
    # the inspection zip: Tools for the archive, Vision for the analysis it holds
    add_subdirectory(Inspection)
endif()
