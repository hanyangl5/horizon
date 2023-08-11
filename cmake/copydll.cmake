macro(copy_dxil proj_name dst_dir)

    add_custom_command(TARGET ${proj_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${DXC_LIB_PATH}/bin/x64/dxil.dll ${dst_dir}
    )
    add_custom_command(TARGET ${proj_name} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/third_party/assimp/bin/Debug/ 
        ${BIN_DIR}assimp-${ASSIMP_MSVC_VERSION}-mt.dll VERBATIM)
endmacro()