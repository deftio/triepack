# CompilerWarnings.cmake
# Applies strict warnings to a target.

function(triepack_set_warnings target)
    target_compile_options(${target} PRIVATE
        # GCC / Clang (C and C++)
        $<$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
            -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wshadow
            -Wformat=2
            -Wunused
            -Wnull-dereference
            -Wdouble-promotion
        >
        # C-only warnings
        $<$<AND:$<COMPILE_LANGUAGE:C>,$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>>>:
            -Wstrict-prototypes
            -Wmissing-prototypes
        >
        # GCC-specific
        $<$<OR:$<C_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:GNU>>:
            -Wlogical-op
            -Wduplicated-cond
        >
        # MSVC
        $<$<OR:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>:
            /W4 /WX
        >
    )
endfunction()
