-- NorseConditions - one "only when..." language for the Norse frameworks.
-- include/NorseManifest.hpp is the library (header-only; the frameworks include it). Targets:
--   conditions_test : the offline test (no game) - xmake build conditions_test && xmake run conditions_test
--   main          : the UE4SS mod that writes the full report to NorseManifest.log (needs ../NorsePatcher for UE4SS)
set_project("NorseConditions")
local version = "0.1.0"
set_version(version)
set_xmakever("2.8.0")

add_rules("mode.release")
set_defaultmode("release")
set_languages("c++20")
set_runtimes("MD")
add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX", "UNICODE", "_UNICODE")
add_cxflags("/EHsc", "/W4", "/permissive-", "/utf-8", {force = true})
add_cxflags({"/d1trimfile:$(projectdir)"}, {force = true, expand = false})

target("conditions_test")
    set_kind("binary")
    add_files("tools/conditions_test.cpp")
    add_includedirs("include")
    set_default(false)

target("main")
    set_kind("shared")
    add_files("src/*.cpp")
    add_includedirs("include", "../NorsePatcher/include", "../NorseManifest/include", "../NorseEvents/include")
    add_defines("NC_VERSION=\"" .. version .. "\"")
    add_linkdirs("../NorsePatcher/build/ue4ss")
    add_links("UE4SS", "user32")
    set_symbols("debug")
    set_optimize("faster")
    add_shflags("/PDBALTPATH:%_PDB%", {force = true})
    add_shflags("/DEBUG", "/OPT:REF", "/OPT:ICF", {force = true})
