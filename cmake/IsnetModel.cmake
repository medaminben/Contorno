# contorno_add_isnet_model(<out_var>)
#
# Defines the custom target `isnet_model` (not part of ALL) that exports IS-Net
# to TorchScript at ${PROJECT_BUILD_OUTPUT}/models/isnet-general-use.pt and sets
# <out_var> to that path. Sets <out_var> empty when Python with venv is missing.
set(_ISNET_MODEL_CMAKE_DIR ${CMAKE_CURRENT_LIST_DIR})

function(contorno_add_isnet_model out_var)
    set(${out_var} "" PARENT_SCOPE)

    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(NOT Python3_Interpreter_FOUND)
        message(STATUS "IS-Net model: python3 not found, AI model target disabled")
        return()
    endif()
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -c "import venv, ensurepip"
        RESULT_VARIABLE _venv_rc
        OUTPUT_QUIET ERROR_QUIET)
    if(NOT _venv_rc EQUAL 0)
        message(STATUS "IS-Net model: python3 venv/ensurepip missing, AI model target disabled")
        return()
    endif()

    set(_root ${_ISNET_MODEL_CMAKE_DIR}/..)
    cmake_path(NORMAL_PATH _root OUTPUT_VARIABLE _root)
    set(_script ${_root}/docs/articles/contorno-area-measurement/scripts/model/export_isnet.py)
    set(_reqs ${_root}/docs/articles/contorno-area-measurement/scripts/model/requirements.txt)
    set(_out ${PROJECT_BUILD_OUTPUT}/models/isnet-general-use.pt)
    set(_check ${_root}/docs/samples/leather/calf_black.jpg)  # traced and eager model must agree on it

    add_custom_command(
        OUTPUT ${_out}
        COMMAND ${CMAKE_COMMAND}
            -DPYTHON=${Python3_EXECUTABLE}
            -DVENV_DIR=${CMAKE_BINARY_DIR}/models/venv
            -DWORK_DIR=${CMAKE_BINARY_DIR}/models/work
            -DSCRIPT=${_script}
            -DREQUIREMENTS=${_reqs}
            -DOUT=${_out}
            -DCHECK=${_check}
            -P ${_ISNET_MODEL_CMAKE_DIR}/RunIsnetExport.cmake
        DEPENDS ${_script} ${_reqs} ${_check} ${_ISNET_MODEL_CMAKE_DIR}/RunIsnetExport.cmake
        COMMENT "Exporting IS-Net to TorchScript (first run downloads about 1 GB)"
        VERBATIM)
    add_custom_target(isnet_model DEPENDS ${_out})
    set(${out_var} ${_out} PARENT_SCOPE)
endfunction()
