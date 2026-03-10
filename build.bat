@echo off
setlocal

set "ROOT=%~dp0"
set "BUILD_DIR=%ROOT%build"
set "SDL_ROOT=%ROOT%external\SDL2\x86_64-w64-mingw32"
set "SDL_INCLUDE=%SDL_ROOT%\include"
set "SDL_LIB=%SDL_ROOT%\lib"
set "SDL_BIN=%SDL_ROOT%\bin"

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

gcc -std=c99 -O0 -g -Wall -Wno-narrowing -I"%SDL_INCLUDE%" ^
    "%ROOT%src\main.c" ^
    "%ROOT%src\nes.c" ^
    "%ROOT%src\cpu.c" ^
    "%ROOT%src\cpu_io.c" ^
    "%ROOT%src\cpu_debug.c" ^
    "%ROOT%src\ppu.c" ^
    "%ROOT%src\ppu_debug.c" ^
    "%ROOT%src\apu.c" ^
    "%ROOT%src\apu_tables.c" ^
    "%ROOT%src\controller.c" ^
    "%ROOT%src\gui.c" ^
    "%ROOT%src\mapper0.c" ^
    "%ROOT%src\mapper1.c" ^
    "%ROOT%src\mapper2.c" ^
    "%ROOT%src\mapper3.c" ^
    "%ROOT%src\mapper66.c" ^
    -L"%SDL_LIB%" -lSDL2 -lopengl32 -o "%BUILD_DIR%\nes.exe"
if errorlevel 1 exit /b %errorlevel%

copy /Y "%SDL_BIN%\SDL2.dll" "%BUILD_DIR%\SDL2.dll" >nul
if errorlevel 1 exit /b %errorlevel%

echo Built "%BUILD_DIR%\nes.exe"
