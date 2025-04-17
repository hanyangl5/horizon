-- cpu_features xmake configuration
target("cpu_features")
    -- default build type is release
    add_rules("mode.release")
    set_kind("static")
    -- add_files("src/*.c") 
    set_languages("c99")
    add_defines("STACK_LINE_READER_BUFFER_SIZE=1024")
    add_includedirs("include", "include/internal")
    if is_plat("linux", "macosx") then
        add_files(
            "src/hwcaps_linux_or_android.c",
            "/src/hwcaps_freebsd.c",
            "/src/hwcaps.c"
        )
    end

    add_files(
        "src/filesystem.c",
        "src/stack_line_reader.c",
        "src/string_view.c",
        "src/impl_*.c"
    )