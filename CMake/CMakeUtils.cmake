function(horizon_configure_unity target)
    set_target_properties(${target} PROPERTIES
        UNITY_BUILD "${HORIZON_UNITY_BUILD}"
        UNITY_BUILD_BATCH_SIZE "${HORIZON_UNITY_BUILD_BATCH_SIZE}"
    )
    if(HORIZON_UNITY_BUILD)
        # Keep IMemory.h's malloc/new macros from leaking into the next source.
        target_compile_definitions(${target} PRIVATE HORIZON_UNITY_BUILD=1)
    endif()
endfunction()
