# Runtime files that executables using Dawn need next to them.
#
# On Windows, Dawn's Direct3D backends compile shaders with d3dcompiler_47.dll and, unless Dawn was built with
# DAWN_FORCE_SYSTEM_COMPONENT_LOAD, only look for it next to the executable. Applications that use Dawn ship the
# DLL; Carbon's examples and tests copy it from the Windows SDK (or, failing that, from the system directory).

if(WIN32 AND NOT DEFINED CARBON_D3DCOMPILER_DLL)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64|aarch64")
        set(CARBON_RUNTIME_ARCHITECTURE arm64)
    else()
        set(CARBON_RUNTIME_ARCHITECTURE x64)
    endif()
    find_file(CARBON_D3DCOMPILER_DLL d3dcompiler_47.dll
        PATHS
            "$ENV{WindowsSdkDir}/Redist/D3D/${CARBON_RUNTIME_ARCHITECTURE}"
            "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/Redist/D3D/${CARBON_RUNTIME_ARCHITECTURE}"
            "$ENV{SystemRoot}/System32"
        NO_DEFAULT_PATH
        DOC "d3dcompiler_47.dll copied next to Carbon's examples and tests"
    )
    if(NOT CARBON_D3DCOMPILER_DLL)
        message(WARNING
            "Carbon: d3dcompiler_47.dll was not found. Examples and GPU tests may fail to create a device; "
            "copy the DLL next to the executables or set CARBON_D3DCOMPILER_DLL.")
    endif()
endif()

# Copies the runtime files next to TARGET's executable after it is built.
function(carbon_copy_dawn_runtime TARGET)
    if(WIN32 AND CARBON_D3DCOMPILER_DLL)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${CARBON_D3DCOMPILER_DLL}" "$<TARGET_FILE_DIR:${TARGET}>"
            VERBATIM
        )
    endif()
endfunction()
