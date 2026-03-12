#define NOB_IMPLEMENTATION
#include "nob.h"

int main(int argc, char** argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    if (!nob_mkdir_if_not_exists("build")) return 1;

    Nob_Cmd cmd = {0};

    nob_log(NOB_INFO, "Compiling and linking...");

    nob_cmd_append(&cmd, "g++");
    nob_cmd_append(&cmd, "-O0", "-g", "-Wall", "-Wno-narrowing", "-Wno-missing-braces");
    nob_cmd_append(&cmd, "-Iexternal/SDL2/x86_64-w64-mingw32/include");
    nob_cmd_append(&cmd, "-Iexternal/SDL2/x86_64-w64-mingw32/include/SDL2");
    nob_cmd_append(&cmd, "-Iexternal/cimgui/imgui");
    nob_cmd_append(&cmd, "-Iexternal/cimgui/imgui/backends");
    nob_cmd_append(&cmd, "-Iexternal/cimgui");

    nob_cmd_append(&cmd, "-DCIMGUI_USE_SDL2", "-DCIMGUI_USE_OPENGL3", "-DIMGUI_IMPL_API=extern \"C\"");

    const char* src_files[] = {
        "src/main.c",
        "src/ui.c",
        "src/nes.c",
        "src/cpu.c",
        "src/cpu_io.c",
        "src/cpu_debug.c",
        "src/ppu.c",
        "src/ppu_debug.c",
        "src/apu.c",
        "src/apu_tables.c",
        "src/controller.c",
        "src/gui.c",
        "src/mapper0.c",
        "src/mapper1.c",
        "src/mapper2.c",
        "src/mapper3.c",
        "src/mapper66.c",
        "external/cimgui/cimgui.cpp",
        "external/cimgui/cimgui_impl.cpp",
        "external/cimgui/imgui/imgui.cpp",
        "external/cimgui/imgui/imgui_demo.cpp",
        "external/cimgui/imgui/imgui_draw.cpp",
        "external/cimgui/imgui/imgui_tables.cpp",
        "external/cimgui/imgui/imgui_widgets.cpp",
        "external/cimgui/imgui/backends/imgui_impl_sdl2.cpp",
        "external/cimgui/imgui/backends/imgui_impl_opengl3.cpp",
    };

    for (size_t i = 0; i < NOB_ARRAY_LEN(src_files); ++i) {
        nob_cmd_append(&cmd, src_files[i]);
    }

    nob_cmd_append(&cmd, "-Lexternal/SDL2/x86_64-w64-mingw32/lib", "-lSDL2", "-lopengl32");
    nob_cmd_append(&cmd, "-o", "build/nes.exe");

    if (!nob_cmd_run_sync(cmd)) return 1;

    if (!nob_copy_file("external/SDL2/x86_64-w64-mingw32/bin/SDL2.dll", "build/SDL2.dll")) return 1;

    nob_log(NOB_INFO, "Built build/nes.exe");

    return 0;
}
