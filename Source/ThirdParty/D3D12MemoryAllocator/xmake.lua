target("d3d12ma-xmake")
    set_kind("static")
    add_rules("mode.release")
    set_languages("cxx14")
    add_includedirs("include", {public = true})
    add_files(
    "src/D3D12MemAlloc.cpp"
    )
    add_links("d3d12.lib",
        "dxgi.lib",
        "dxguid.lib")