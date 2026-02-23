function(configure_compile_options target)
    target_compile_options(${PROJECT_NAME} PRIVATE -DNOMINMAX)
    
    # Check for Clang first (including clang-cl on Windows)
    # clang-cl sets MSVC=TRUE but CMAKE_CXX_COMPILER_ID is "Clang"
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Clang compiler (including clang-cl with MSVC-like command-line)
        if(MSVC)
            # clang-cl: Use MSVC-compatible flags but with Clang-specific options
            target_compile_options(${target} PRIVATE 
                /MP 
                /permissive 
                /w14640 
                /W3 
                /WX 
                /external:anglebrackets 
                /external:W0 
                /GR- 
                /utf-8 
                /wd4100 
                /wd4101 
                /wd4189
                -fms-extensions
                -Wno-language-extension-token
            )
        else()
            # Regular Clang (GNU-like command-line)
            target_compile_options(${target} PRIVATE 
                -Wall 
                -Wextra 
                -Werror 
                -Wshadow 
                -pedantic 
                -fms-extensions 
                -Wno-language-extension-token 
                -Wno-switch 
                -Wno-missing-field-initializers 
                -Wno-unused-variable 
                -Wno-unused-parameter 
                -Wno-unused-function
            )
            if(ANDROID)
                target_compile_options(${target} PRIVATE -Wno-gnu-anonymous-struct -Wno-nested-anon-types)
            endif()
        endif()
    elseif(MSVC)
        # True MSVC compiler
        target_compile_options(${target} PRIVATE 
            /MP 
            /permissive 
            /w14640 
            /W3 
            /WX 
            /external:anglebrackets 
            /external:W0 
            /GR- 
            /utf-8 
            /wd4100 
            /wd4101 
            /wd4189
        )
    endif()
endfunction()