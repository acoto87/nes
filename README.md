# NES

A Nintendo Entertainment System emulator written in C.

This is an implementation of the NES console developed and manufactured by Nintendo. The UI is built with Dear ImGui (via cimgui) and rendered through OpenGL 3.3, using SDL2 for windowing, input, and audio.

## Features

- Accurate emulation of the 6502 CPU, PPU, and APU
- Audio output with individual channel control (Pulse 1, Pulse 2, Triangle, Noise, DMC)
- Save states written automatically alongside the loaded ROM (`.nsave` format)
- Keyboard and gamepad (SDL GameController) input support
- Integrated debugger with:
  - Step-by-step CPU execution and single-cycle stepping
  - Breakpoint support
  - CPU register viewer
  - Memory viewer
  - Pattern table, palette, sprite (OAM), and nametable viewers
  - Audio channel waveform inspection

## Supported Mappers

| Mapper | Name              |
|--------|-------------------|
| 000    | NROM              |
| 001    | MMC1 / SxROM      |
| 002    | UxROM             |
| 003    | CNROM             |
| 066    | GxROM / MxROM     |

## Libraries

| Library | Description |
|---------|-------------|
| [SDL2](https://www.libsdl.org/) | Windowing, input, audio |
| [cimgui](https://github.com/cimgui/cimgui) | C bindings for Dear ImGui (immediate-mode UI) |

## Build

The build system uses [nob](https://github.com/tsoding/nob.h) (a single-header C build tool).

**Prerequisites:**
- A GCC toolchain for Windows (e.g. [MinGW-w64](https://www.mingw-w64.org/)) with `g++` available on `PATH`.

**Steps:**

1. Compile the build tool (only needed once, or after `nob.c` changes):
   ```
   g++ -o nob.exe nob.c
   ```
2. Run the build:
   ```
   nob.exe
   ```

The output binary is `build/nes.exe`. `SDL2.dll` is copied next to it automatically.

## Controls

### Keyboard

| Key            | NES Button |
|----------------|------------|
| Arrow keys     | D-Pad      |
| `A`            | B          |
| `S`            | A          |
| `Space`        | Select     |
| `Enter`        | Start      |

Gamepad input is also supported via SDL's GameController API.

## Run

```
build\nes.exe path\to\game.nes
```

- You can also drag and drop a `.nes` or `.nsave` file onto the emulator window.
- Save states are written automatically next to the loaded ROM using the same base name with a `.nsave` extension and loaded automatically on the next run.

## Screenshots

![](https://github.com/acoto87/nes/blob/master/pics/castlevania.gif)
![](https://github.com/acoto87/nes/blob/master/pics/Capture15.PNG)
![](https://github.com/acoto87/nes/blob/master/pics/Capture16.PNG)
![](https://github.com/acoto87/nes/blob/master/pics/Capture20.PNG)
