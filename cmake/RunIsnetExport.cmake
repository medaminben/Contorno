# Run with: cmake -DPYTHON= -DVENV_DIR= -DWORK_DIR= -DSCRIPT= -DREQUIREMENTS= -DOUT= -DCHECK= -P RunIsnetExport.cmake
# CHECK: a photo the traced model must match the eager one on (the script's --check).
# Never fails: on any error, a failed check included, it warns and leaves OUT
# uncreated so the next build retries.
function(_isnet_fail what)
    message(WARNING "IS-Net export failed (${what}); AI tests will be skipped")
    file(REMOVE ${OUT})
    return()
endfunction()

set(_vpy ${VENV_DIR}/bin/python)
if(WIN32)
    set(_vpy ${VENV_DIR}/Scripts/python.exe)
endif()

file(MAKE_DIRECTORY ${WORK_DIR})
if(NOT EXISTS ${_vpy})
    execute_process(COMMAND ${PYTHON} -m venv ${VENV_DIR} RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        _isnet_fail("creating venv")
        return()
    endif()
endif()

execute_process(COMMAND ${_vpy} -m pip install -q -r ${REQUIREMENTS} RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    _isnet_fail("pip install")
    return()
endif()

execute_process(COMMAND ${_vpy} -I ${SCRIPT} --out ${OUT} --work ${WORK_DIR} --check ${CHECK} RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0 OR NOT EXISTS ${OUT})
    # the script writes OUT before its check: a failed check leaves no model either
    _isnet_fail("export script or its check")
    return()
endif()
