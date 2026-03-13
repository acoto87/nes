#define NOB_IMPLEMENTATION
#define NOB_EXPERIMENTAL_DELETE_OLD
#include "nob.h"

static bool nob_read_entire_dir_recursively(const char* parent, Nob_File_Paths* file_paths, int idx)
{
    if (!nob_read_entire_dir(parent, file_paths)) {
        return false;
    }

    int count = file_paths->count;

    for (int i = idx; i < count; i++) {
        if (strcmp(file_paths->items[i], ".") == 0) continue;
        if (strcmp(file_paths->items[i], "..") == 0) continue;

        // NOTE: Here I'm leaking the allocation for each file name on the nob_read_entire_dir call
        // but I'm don't care that much because that alloc is done on the temp static arena
        file_paths->items[i] = nob_temp_sprintf("%s/%s", parent, file_paths->items[i]);

        Nob_File_Type file_type = nob_get_file_type(file_paths->items[i]);
        if (file_type == NOB_FILE_DIRECTORY) {
            if (!nob_read_entire_dir_recursively(file_paths->items[i], file_paths, file_paths->count)) {
                return false;
            }
        }
    }

    return true;
}

static bool clear_folder(const char* parent, Nob_File_Paths *file_paths)
{
    if (!nob_read_entire_dir_recursively(parent, file_paths, 0)) {
        return false;
    }

    for (int i = 0; i < file_paths->count; i++) {
        Nob_File_Type file_type = nob_get_file_type(file_paths->items[i]);
        if (file_type == NOB_FILE_REGULAR) {
            if (!nob_delete_file(file_paths->items[i])) {
                return false;
            }
        }
    }

    file_paths->count = 0;

    return true;
}

int main(int argc, char** argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    if (!nob_mkdir_if_not_exists("build")) return 1;
    if (!nob_mkdir_if_not_exists("external/cimgui/build")) return 1;

    Nob_Cmd cmd = {0};
    Nob_File_Paths file_paths = {0};

    nob_log(NOB_INFO, "Compiling and linking...");

    int build_release = 0;

    if (argc > 1) {
        // Shift the first argument (the executable path) so
        // that the next arguments are the actual command-line arguments
        nob_shift_args(&argc, &argv);

        while (argc > 0) {
            const char* arg = nob_shift_args(&argc, &argv);
            if (arg) {
                if (strcmp(arg, "--build") == 0 || strcmp(arg, "-b") == 0) {
                    if (argc == 0) {
                        nob_log(NOB_ERROR, "missing build mode after %s", arg);
                        return 1;
                    }
                    const char* build_mode_value = nob_shift_args(&argc, &argv);
                    if (!build_mode_value) {
                        nob_log(NOB_ERROR, "missing build mode after %s", arg);
                        return 1;
                    }
                    if (strcasecmp(build_mode_value, "debug") == 0) {
                        build_release = 0;
                    } else if (strcasecmp(build_mode_value, "release") == 0) {
                        build_release = 1;
                    } else {
                        nob_log(NOB_ERROR, "unknown build mode: %s. Valid options are 'debug' or 'release'.",
                                build_mode_value);
                        return 1;
                    }
                } else {
                    nob_log(NOB_WARNING, "ignoring unknown argument: %s", arg);
                }
            }
        }
    }

    const char* cimgui_src_files[] = {
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

    int rebuild_cimgui = !nob_file_exists("external/cimgui/build/cimgui.dll");
    if (rebuild_cimgui || build_release) {
        if (!clear_folder("external/cimgui/build", &file_paths)) {
            return 1;
        }

        nob_log(NOB_INFO, "Compiling cimgui.dll...");
        nob_cmd_append(&cmd, "g++", "-shared");
        if (build_release) {
            nob_cmd_append(&cmd, "-O2");
        } else {
            nob_cmd_append(&cmd, "-O0", "-g");
        }
        nob_cmd_append(&cmd, "-Iexternal/SDL2/x86_64-w64-mingw32/include");
        nob_cmd_append(&cmd, "-Iexternal/SDL2/x86_64-w64-mingw32/include/SDL2");
        nob_cmd_append(&cmd, "-Iexternal/cimgui/imgui");
        nob_cmd_append(&cmd, "-Iexternal/cimgui/imgui/backends");
        nob_cmd_append(&cmd, "-Iexternal/cimgui");
        nob_cmd_append(&cmd, "-DCIMGUI_USE_SDL2", "-DCIMGUI_USE_OPENGL3");
        nob_cmd_append(&cmd, "-DIMGUI_IMPL_API=extern \"C\" __declspec(dllexport)");

        for (size_t i = 0; i < NOB_ARRAY_LEN(cimgui_src_files); ++i) {
            nob_cmd_append(&cmd, cimgui_src_files[i]);
        }

        nob_cmd_append(&cmd, "-Lexternal/SDL2/x86_64-w64-mingw32/lib", "-lSDL2", "-lopengl32");
        nob_cmd_append(&cmd, "-o", "external/cimgui/build/cimgui.dll", "-Wl,--out-implib,external/cimgui/build/libcimgui.dll.a");

        if (!nob_cmd_run_sync(cmd)) {
            return 1;
        }
    }

    cmd.count = 0;

    if (!clear_folder("build", &file_paths)) {
        return 1;
    }

    nob_log(NOB_INFO, "Compiling nes.exe...");
    nob_cmd_append(&cmd, "gcc");
    if (build_release) {
        nob_cmd_append(&cmd, "-O2");
    } else {
        nob_cmd_append(&cmd, "-O0", "-g");
    }
    nob_cmd_append(&cmd, "-Wall", "-Wno-narrowing", "-Wno-missing-braces", "--pedantic");
    nob_cmd_append(&cmd, "-Iexternal/SDL2/x86_64-w64-mingw32/include");
    nob_cmd_append(&cmd, "-Iexternal/SDL2/x86_64-w64-mingw32/include/SDL2");
    nob_cmd_append(&cmd, "-Iexternal/cimgui/imgui");
    nob_cmd_append(&cmd, "-Iexternal/cimgui/imgui/backends");
    nob_cmd_append(&cmd, "-Iexternal/cimgui");
    nob_cmd_append(&cmd, "-DCIMGUI_USE_SDL2", "-DCIMGUI_USE_OPENGL3");
    nob_cmd_append(&cmd, "-DIMGUI_IMPL_API=extern __declspec(dllimport)", "-DCIMGUI_NO_EXPORT");
    nob_cmd_append(&cmd, "src/main.c");
    nob_cmd_append(&cmd, "-Lexternal/cimgui/build", "-lcimgui");
    nob_cmd_append(&cmd, "-Lexternal/SDL2/x86_64-w64-mingw32/lib", "-lSDL2");
    nob_cmd_append(&cmd, "-lopengl32");
    nob_cmd_append(&cmd, "-o", "build/nes.exe");

    if (!nob_cmd_run_sync(cmd) ||
        !nob_copy_file("external/SDL2/x86_64-w64-mingw32/bin/SDL2.dll", "build/SDL2.dll") ||
        !nob_copy_file("external/cimgui/build/cimgui.dll", "build/cimgui.dll")) {
        return 1;
    }

    nob_log(NOB_INFO, "Built build/nes.exe");

    return 0;
}
