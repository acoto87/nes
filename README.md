# NES

A Nintendo Entertainment System emulator written in C.

This is an implementation of the NES console developed and manufactured by Nintendo.

NES currently implements the first three mappers, which convers a good percentage of the games:

* NROM (Mapper 000)
* MMC1 / SxROM (Mapper 001)
* UxROM (Mapper 002)
* CNROM (Mapper 003)

It also has an integrated debugger, you can set breakpoints and run step by step the code of the game. It has a register and memory viewers, and also palettes, sprites and nametables viewer.

## Libraries used

* [nuklear](https://github.com/vurtun/nuklear): A minimal state immediate mode graphical user interface toolkit written in ANSI C.
* [SDL](https://www.libsdl.org/): Simple DirectMedia Layer is a cross-platform development library designed to provide low level access to audio, keyboard, mouse, joystick, and graphics hardware via OpenGL and Direct3D.

## Screenshots

![](https://github.com/acoto87/nes/blob/master/pics/castlevania.gif)
![](https://github.com/acoto87/nes/blob/master/pics/Capture15.PNG)
![](https://github.com/acoto87/nes/blob/master/pics/Capture16.PNG)
![](https://github.com/acoto87/nes/blob/master/pics/Capture20.PNG)
