#pragma once
#ifndef PPU_H
#define PPU_H

#include "types.h"

/*
* http://wiki.nesdev.com/w/index.php/PPU_power_up_state
* http://wiki.nesdev.com/w/index.php/PPU_rendering
* http://wiki.nesdev.com/w/index.php/PPU_sprite_evaluation
* http://wiki.nesdev.com/w/images/d/d1/Ntsc_timing.png
*
* Creating a NES emulator in C++11 (PART 1/2)
* https://www.youtube.com/watch?v=y71lli8MS8s
*/

// PPU Control Register
#define NAMETABLE_ADDR_FLAG 0           // Name Table Address 
                                        // 0 = 0x2000, 1 = 0x2400, 2 = 0x2800, 3 = 0x2C00)
#define NAMETABLE_Y_SCROLL_FLAG 0       // bit 0 = Y scroll name table selection
#define NAMETABLE_X_SCROLL_FLAG 1       // bit 1 = X scroll name table selection

#define VERTICAL_WRITE_FLAG 2           // Vertical Write 
                                        // 0 = PPU memory address increments by 1 
                                        // 1 = PPU memory address increments by 32

#define SPRITE_ADDR_FLAG 3              // Sprite Pattern Table Address
                                        // 1 = $1000, 0 = $0000.

#define BACKGROUND_ADDR_FLAG 4          // Screen Pattern Table Address
                                        // 1 = $1000, 0 = $0000.

#define SPRITE_SIZE_FLAG 5              // Sprite Size
                                        // 1 = 8x16, 0 = 8x8.

#define VBLANK_FLAG 7                   // VBlank Enable, 
                                        // 1 = generate interrupts on VBlank, 0 = no generate interrups on VBlank

// PPU Mask Register
#define COLOR_FLAG 0                    // Indicates whether the system is in colour.
                                        // 0 = normal color, 1 = monochrome mode (all palette entries AND with 0x30, 
                                        // effectively producing a monochrome display; note that colour emphasis STILL works when this is on)

#define BACKGROUND_CLIP_MASK_FLAG 1     // Background Mask
                                        // 0 = don't show left 8 columns of the screen.

#define SPRITE_CLIP_MASK_FLAG 2         // Sprite Mask
                                        // 0 = don't show sprites in left 8 columns. 

#define BACKGROUND_ENABLED_FLAG 3       // Screen Enable
                                        // 1 = show background, 0 = blank screen.

#define SPRITES_ENABLED_FLAG 4          // Sprites Enable
                                        // 1 = show sprites, 0 = hide sprites.

#define BACKGROUND_FLAG 5               // Background Color
                                        // 0 = black, 1 = blue, 2 = green, 4 = red. 
                                        // Do not use any other numbers as you may damage PPU hardware.

// PPU Status Register
#define IGNORE_VBLANK_FLAG 4            // If set, indicates that writes to VRAM should be ignored.

#define SCANLINE_COUNT_FLAG 5           // Scanline sprite count, if set, indicates more than 8 sprites on the current scanline.
                                        // The PPU can handle only eight sprites on one scanline 
                                        // and sets this bit if it starts dropping sprites. 
                                        // Normally, this triggers when there are 9 sprites on a scanline, 
                                        // but the actual behavior is significantly more complicated

#define HIT_FLAG 6                      // Sprite 0 hit flag, set when a non-transparent pixel of sprite 0 overlaps a non - transparent background pixel.

#define VBLANK_FLAG 7                   // Indicates whether V-Blank is occurring. 

// The NES PPU operates at a speed of 21.477272 MHz / 4 = 5369318Hz.
#define PPU_FREQ 5369318

#define PPU_RAM_SIZE KILOBYTES(64)

#define PPU_PATTERN_TABLE_0_OFFSET 0x0000 
#define PPU_PATTERN_TABLE_1_OFFSET 0x1000 
#define PPU_PATTERN_TABLE_SIZE 0x1000

#define PPU_CYCLES_PER_SCANLINE 341
#define PPU_SCANLINES_PER_FRAME 262

#define PPU_CONTROL_ADDRESS_VERTICAL_INCREMENT 32
#define PPU_CONTROL_ADDRESS_HORIZONTAL_INCREMENT 1

#define PPU_WASTED_VBLANK_SCANLINES 20
#define PPU_WASTED_PREFETCH_SCANLINES 1

#define PPU_BACKGROUND_TILES_PER_ROW 32
#define PPU_BACKGROUND_TILES_PER_COLUMN 30

#define PPU_HORIZONTAL_PIXELS_PER_TILE 8
#define PPU_VERTICAL_PIXELS_PER_TILE 8

#define PPU_PATTERN_BYTES_PER_TILE 16 

/* The palette for the background runs from VRAM $3F00 to $3F0F;

$3F00  Universal background color

$3F01-$3F03  Background palette 0

$3F05-$3F07  Background palette 1

$3F09-$3F0B  Background palette 2

$3F0D-$3F0F  Background palette 3

Addresses $3F04/$3F08/$3F0C can contain unique data,  though these values are not used by the PPU when rendering. */

#define PPU_BACKGROUND_PALETTE_FIRST_ADDRESS 0x3F00
#define PPU_BACKGROUND_PALETTE_LAST_ADDRESS 0x3F0F 
#define PPU_BACKGROUND_PALETTE_TOTAL_SIZE (PPU_BACKGROUND_PALETTE_LAST_ADDRESS - PPU_BACKGROUND_PALETTE_FIRST_ADDRESS + 1) 

#define PPU_BACKGROUND_PALETTE_0_FIRST_ADDRESS 0x3F00
#define PPU_BACKGROUND_PALETTE_0_LAST_ADDRESS  0x3F03 

#define PPU_BACKGROUND_PALETTE_1_FIRST_ADDRESS 0x3F04
#define PPU_BACKGROUND_PALETTE_1_LAST_ADDRESS  0x3F07 

#define PPU_BACKGROUND_PALETTE_2_FIRST_ADDRESS 0x3F08
#define PPU_BACKGROUND_PALETTE_2_LAST_ADDRESS  0x3F0B 

#define PPU_BACKGROUND_PALETTE_3_FIRST_ADDRESS 0x3F0C
#define PPU_BACKGROUND_PALETTE_3_LAST_ADDRESS  0x3F0F

/*

the palette for the sprites runs from $3F10 to $3F1F.

$3F11-$3F13  Sprite palette 0

$3F15-$3F17  Sprite palette 1

$3F19-$3F1B  Sprite palette 2

$3F1D-$3F1F  Sprite palette 3

*/

#define PPU_SPRITE_PALETTE_FIRST_ADDRESS 0x3F10
#define PPU_SPRITE_PALETTE_LAST_ADDRESS  0x3F1F

#define PPU_SPRITE_PALETTE_0_FIRST_ADDRESS 0x3F10
#define PPU_SPRITE_PALETTE_0_LAST_ADDRESS  0x3F13

#define PPU_SPRITE_PALETTE_1_FIRST_ADDRESS 0x3F14
#define PPU_SPRITE_PALETTE_1_LAST_ADDRESS  0x3F17

#define PPU_SPRITE_PALETTE_2_FIRST_ADDRESS 0x3F18
#define PPU_SPRITE_PALETTE_2_LAST_ADDRESS  0x3F1B

#define PPU_SPRITE_PALETTE_3_FIRST_ADDRESS 0x3F1C
#define PPU_SPRITE_PALETTE_3_LAST_ADDRESS  0x3F1F

#define PPU_SPRITE_PALETTE_TOTAL_SIZE (PPU_SPRITE_PALETTE_LAST_ADDRESS - PPU_SPRITE_PALETTE_FIRST_ADDRESS + 1)

#define PPU_NAME_TABLE_0_FIRST_ADDRESS 0x2000
#define PPU_NAME_TABLE_0_LAST_ADDRESS  0x23FF

#define PPU_NAME_TABLE_1_FIRST_ADDRESS 0x2400
#define PPU_NAME_TABLE_1_LAST_ADDRESS  0x27FF

#define PPU_NAME_TABLE_2_FIRST_ADDRESS 0x2800
#define PPU_NAME_TABLE_2_LAST_ADDRESS  0x2BFF

#define PPU_NAME_TABLE_3_FIRST_ADDRESS 0x2C00
#define PPU_NAME_TABLE_3_LAST_ADDRESS  0x2FFF

#define PPU_NAME_TABLE_SIZE (PPU_NAME_TABLE_0_LAST_ADDRESS - PPU_NAME_TABLE_0_FIRST_ADDRESS + 1)
#define PPU_NAME_TABLE_TOTAL_SIZE (PPU_NAME_TABLE_3_LAST_ADDRESS - PPU_NAME_TABLE_0_FIRST_ADDRESS + 1)

#define PPU_NAME_TABLE_MIRROR_FIRST_ADDRESS 0x3000
#define PPU_NAME_TABLE_MIRROR_LAST_ADDRESS 0x3EFF

#define PPU_HORIZONTAL_TILES_PER_ATTRIBUTE_BYTE 4
#define PPU_VERTICAL_TILES_PER_ATTRIBUTE_BYTE 4

#define PPU_ATTRIBUTE_BYTES_PER_ROW 8
#define PPU_ATTRIBUTE_BYTES_PER_COLUMN 8

#define PPU_NAMETABLE_BYTES_BEFORE_ATTRIBUTE_TABLE (PPU_BACKGROUND_TILES_PER_ROW * PPU_BACKGROUND_TILES_PER_COLUMN) 

#define PPU_NUM_SYSTEM_COLOURS 64 

// this is a forward reference to a function in cpu.h, so WriteDMA could compile.
inline u8 ReadCPUU8(NES *nes, u16 address);

// palette adapted from http://nesdev.parodius.com/NESTechFAQ.htm 
global Color systemPalette[PPU_NUM_SYSTEM_COLOURS] =
{
    { 0x75, 0x75, 0x75, 0xFF },
    { 0x27, 0x1B, 0x8F, 0xFF },
    { 0x00, 0x00, 0xAB, 0xFF },
    { 0x47, 0x00, 0x9F, 0xFF },
    { 0x8F, 0x00, 0x77, 0xFF },
    { 0xAB, 0x00, 0x13, 0xFF },
    { 0xA7, 0x00, 0x00, 0xFF },
    { 0x7F, 0x0B, 0x00, 0xFF },
    { 0x43, 0x2F, 0x00, 0xFF },
    { 0x00, 0x47, 0x00, 0xFF },
    { 0x00, 0x51, 0x00, 0xFF },
    { 0x00, 0x3F, 0x17, 0xFF },
    { 0x1B, 0x3F, 0x5F, 0xFF },
    { 0x00, 0x00, 0x00, 0xFF },
    { 0x00, 0x00, 0x00, 0xFF },
    { 0x00, 0x00, 0x00, 0xFF },
    { 0xBC, 0xBC, 0xBC, 0xFF },
    { 0x00, 0x73, 0xEF, 0xFF },
    { 0x23, 0x3B, 0xEF, 0xFF },
    { 0x83, 0x00, 0xF3, 0xFF },
    { 0xBF, 0x00, 0xBF, 0xFF },
    { 0xE7, 0x00, 0x5B, 0xFF },
    { 0xDB, 0x2B, 0x00, 0xFF },
    { 0xCB, 0x4F, 0x0F, 0xFF },
    { 0x8B, 0x73, 0x00, 0xFF },
    { 0x00, 0x97, 0x00, 0xFF },
    { 0x00, 0xAB, 0x00, 0xFF },
    { 0x00, 0x93, 0x3B, 0xFF },
    { 0x00, 0x83, 0x8B, 0xFF },
    { 0x00, 0x00, 0x00, 0xFF },
    { 0x00, 0x00, 0x00, 0xFF },
    { 0x00, 0x00, 0x00, 0xFF },
    { 0xFF, 0xFF, 0xFF, 0xFF },
    { 0x3F, 0xBF, 0xFF, 0xFF },
    { 0x5F, 0x97, 0xFF, 0xFF },
    { 0xA7, 0x8B, 0xFD, 0xFF },
    { 0xF7, 0x7B, 0xFF, 0xFF },
    { 0xFF, 0x77, 0xB7, 0xFF },
    { 0xFF, 0x77, 0x63, 0xFF },
    { 0xFF, 0x9B, 0x3B, 0xFF },
    { 0xF3, 0xBF, 0x3F, 0xFF },
    { 0x83, 0xD3, 0x13, 0xFF },
    { 0x4F, 0xDF, 0x4B, 0xFF },
    { 0x58, 0xF8, 0x98, 0xFF },
    { 0x00, 0xEB, 0xDB, 0xFF },
    { 0x00, 0x00, 0x00, 0xFF },
    { 0x00, 0x00, 0x00, 0xFF },
    { 0x00, 0x00, 0x00, 0xFF },
    { 0xFF, 0xFF, 0xFF, 0xFF },
    { 0xAB, 0xE7, 0xFF, 0xFF },
    { 0xC7, 0xD7, 0xFF, 0xFF },
    { 0xD7, 0xCB, 0xFF, 0xFF },
    { 0xFF, 0xC7, 0xFF, 0xFF },
    { 0xFF, 0xC7, 0xDB, 0xFF },
    { 0xFF, 0xBF, 0xB3, 0xFF },
    { 0xFF, 0xDB, 0xAB, 0xFF },
    { 0xFF, 0xE7, 0xA3, 0xFF },
    { 0xE3, 0xFF, 0xA3, 0xFF },
    { 0xAB, 0xF3, 0xBF, 0xFF },
    { 0xB3, 0xFF, 0xCF, 0xFF },
    { 0x9F, 0xFF, 0xF3, 0xFF },
    { 0x00, 0x00, 0x00, 0xFF },
    { 0x00, 0x00, 0x00, 0xFF },
    { 0x00, 0x00, 0x00, 0xFF }
};

global u8 attributeTableLookup[PPU_VERTICAL_TILES_PER_ATTRIBUTE_BYTE][PPU_HORIZONTAL_TILES_PER_ATTRIBUTE_BYTE] =
{
    { 0x0, 0x1, 0x4, 0x5 },
    { 0x2, 0x3, 0x6, 0x7 },
    { 0x8, 0x9, 0xC, 0xD },
    { 0xA, 0xB, 0xE, 0xF }
};

inline u8 ReadPPUU8(NES *nes, u16 address)
{
    address = address % 0x4000;

    if (ISBETWEEN(address, 0x00, 0x2000))
    {
        return nes->mapperReadU8(nes, address);
    }

    if (ISBETWEEN(address, 0x2000, 0x3F00))
    {
        address = 0x2000 + ((address - 0x2000) % 0x1000);
        return ReadU8(&nes->ppuMemory, address);
    }

    if (ISBETWEEN(address, 0x3F00, 0x4000))
    {
        address = 0x3F00 + ((address - 0x3F00) % 0x20);

        if (ISBETWEEN(address, 0x3F00, 0x3F10))
        {
            return ReadU8(&nes->ppuMemory, address);
        }

        if (ISBETWEEN(address, 0x3F10, 0x3F20))
        {
            // setup the background mirrors for the first byte in each sub palette of the sprite palette 
            switch (address)
            {
                case 0x3F10:
                case 0x3F14:
                case 0x3F18:
                case 0x3F1C:
                {
                    address -= 0x10;
                    return ReadU8(&nes->ppuMemory, address);
                }

                default:
                {
                    return ReadU8(&nes->ppuMemory, address);
                }
            }
        }
    }

    return 0;
}

inline u16 ReadPPUU16(NES *nes, u16 address)
{
    // read in little-endian mode
    u8 lo = ReadPPUU8(nes, address);
    u8 hi = ReadPPUU8(nes, address + 1);
    return (hi << 8) | lo;
}

inline void WritePPUU8(NES *nes, u16 address, u8 value)
{
    address = address % 0x4000;

    if (ISBETWEEN(address, 0x00, 0x2000))
    {
        nes->mapperWriteU8(nes, address, value);
        return;
    }

    if (ISBETWEEN(address, 0x2000, 0x3F00))
    {
        address = 0x2000 + ((address - 0x2000) % 0x1000);

        if (nes->cartridge.mirrorType == MIRROR_HORIZONTAL)
        {
            if (address < 0x2400)
            {
                WriteU8(&nes->ppuMemory, address, value);
                WriteU8(&nes->ppuMemory, address + 0x400, value);
            }
            else if (address < 0x2800)
            {
                WriteU8(&nes->ppuMemory, address - 0x400, value);
                WriteU8(&nes->ppuMemory, address, value);
            }
            else if (address < 0x2C00)
            {
                WriteU8(&nes->ppuMemory, address, value);
                WriteU8(&nes->ppuMemory, address + 0x400, value);
            }
            else if (address < 0x3000)
            {
                WriteU8(&nes->ppuMemory, address - 0x400, value);
                WriteU8(&nes->ppuMemory, address, value);
            }
        }
        else if (nes->cartridge.mirrorType == MIRROR_VERTICAL)
        {
            if (address < 0x2400)
            {
                WriteU8(&nes->ppuMemory, address, value);
                WriteU8(&nes->ppuMemory, address + 0x800, value);
            }
            else if (address < 0x2800)
            {
                WriteU8(&nes->ppuMemory, address, value);
                WriteU8(&nes->ppuMemory, address + 0x400, value);
            }
            else if (address < 0x2C00)
            {
                WriteU8(&nes->ppuMemory, address - 0x400, value);
                WriteU8(&nes->ppuMemory, address, value);
            }
            else if (address < 0x3000)
            {
                WriteU8(&nes->ppuMemory, address - 0x400, value);
                WriteU8(&nes->ppuMemory, address, value);
            }
        }
        else
        {
            WriteU8(&nes->ppuMemory, address, value);
        }

        return;
    }

    if (ISBETWEEN(address, 0x3F00, 0x4000))
    {
        address = 0x3F00 + ((address - 0x3F00) % 0x20);

        if (ISBETWEEN(address, 0x3F00, 0x3F10))
        {
            WriteU8(&nes->ppuMemory, address, value);
            return;
        }

        if (ISBETWEEN(address, 0x3F10, 0x3F20))
        {
            // setup the background mirrors for the first byte in each sub palette of the sprite palette 
            switch (address)
            {
                case 0x3F10:
                case 0x3F14:
                case 0x3F18:
                case 0x3F1C:
                {
                    address -= 0x10;
                    WriteU8(&nes->ppuMemory, address, value);
                    break;
                }

                default:
                {
                    WriteU8(&nes->ppuMemory, address, value);
                    break;
                }
            }
        }
    }
}

inline void WritePPUU16(NES *nes, u16 address, u16 value)
{
    // write in little-endian mode
    u8 lo = value & 0xFF00;
    u8 hi = (value & 0xFF00) >> 8;
    WritePPUU8(nes, address, lo);
    WritePPUU8(nes, address + 1, hi);
}

inline u8 ReadCtrl(NES *nes)
{
    PPU *ppu = &nes->ppu;
    return ppu->control;
}

inline void WriteCtrl(NES *nes, u8 value)
{
    PPU *ppu = &nes->ppu;
    ppu->control = value;
    ppu->t = (ppu->t & 0xF3FF) | (((u16)value & 0x3) << 10);

    // Least significant bits previously written into a PPU register goes into PPUSTATUS
    ppu->status = (ppu->status & 0xE0) | (value & 0x1F);
}

inline u8 ReadMask(NES *nes)
{
    PPU *ppu = &nes->ppu;
    return ppu->mask;
}

inline void WriteMask(NES *nes, u8 value)
{
    PPU *ppu = &nes->ppu;
    ppu->mask = value;

    // Least significant bits previously written into a PPU register goes into PPUSTATUS
    ppu->status = (ppu->status & 0xE0) | (value & 0x1F);
}

inline u8 ReadStatus(NES *nes)
{
    PPU *ppu = &nes->ppu;

    u8 status = ppu->status;

    // Reading from $2002 clears the vblank flag(bit 7), and resets the internal
    // $2005 / 6 flip - flop. Writes here have no effect.

    ppu->status &= 0x7F;
    ppu->w = 0;

    // Caution: Reading PPUSTATUS at the exact start of vertical blank will return 0 in bit 7 but clear the latch anyway, 
    // causing the program to miss frames. 
    if (ppu->scanline == 241)
    {
        if (ppu->cycle == 0)
        {
            ppu->suppressNmi = TRUE;
        }
    }

    return status;
}

inline void WriteStatus(NES *nes, u8 value)
{
    // This register is read only
}

inline u8 ReadOamAddr(NES *nes)
{
    // This register is write only
    return 0;
}

inline void WriteOamAddr(NES *nes, u8 value)
{
    PPU *ppu = &nes->ppu;
    ppu->oamAddress = value;

    // Least significant bits previously written into a PPU register goes into PPUSTATUS
    ppu->status = (ppu->status & 0xE0) | (value & 0x1F);
}

inline u8 ReadOamData(NES *nes)
{
    PPU *ppu = &nes->ppu;
    return ReadPPUU8(nes, ppu->oamAddress);
}

inline void WriteOamData(NES *nes, u8 value)
{
    // TODO: write here to oamAddress: WriteU8(oamAddress, value); ??
    PPU *ppu = &nes->ppu;
    ppu->oamData = value;
    WritePPUU8(nes, ppu->oamAddress, value);
    ppu->oamAddress++;

    // Least significant bits previously written into a PPU register goes into PPUSTATUS
    ppu->status = (ppu->status & 0xE0) | (value & 0x1F);
}

inline u8 ReadScroll(NES *nes)
{
    PPU *ppu = &nes->ppu;
    return ppu->scroll;
}

inline void WriteScroll(NES *nes, u8 value)
{
    PPU *ppu = &nes->ppu;
    ppu->scroll = value;

    if (!ppu->w)
    {
        // t: ........ ...HGFED = d: HGFED...
        // x:               CBA = d: .....CBA
        // w:                   = 1
        ppu->t = (ppu->t & 0xFFE0) | (((u16)value & 0xF8) >> 3);
        ppu->x = (value & 0x07);
        ppu->w = 1;
    }
    else
    {
        // t: .CBA..HG FED..... = d: HGFEDCBA
        // w:                   = 0
        ppu->t = (ppu->t & 0x8C1F) | (((u16)value & 0x07) << 12) | (((u16)value & 0xF8) << 2);
        ppu->w = 0;
    }

    // Least significant bits previously written into a PPU register goes into PPUSTATUS
    ppu->status = (ppu->status & 0xE0) | (value & 0x1F);
}

inline u8 ReadVramAddr(NES *nes)
{
    PPU *ppu = &nes->ppu;
    return ppu->address;
}

inline void WriteVramAddr(NES *nes, u8 value)
{
    PPU *ppu = &nes->ppu;
    ppu->address = value;

    if (!ppu->w)
    {
        // t: ..FEDCBA ........ = d: ..FEDCBA
        // t: .X...... ........ = 0
        // w:                   = 1
        ppu->t = (ppu->t & 0x80FF) | ((u16)value & 0x3F) << 8;
        ppu->w = 1;
    }
    else
    {
        // t: ........ HGFEDCBA = d: HGFEDCBA
        // v                    = t
        // w:                   = 0
        ppu->t = (ppu->t & 0xFF00) | (u16)value;
        ppu->v = ppu->t;
        ppu->w = 0;
    }

    // Least significant bits previously written into a PPU register goes into PPUSTATUS
    ppu->status = (ppu->status & 0xE0) | (value & 0x1F);
}

inline u8 ReadVramData(NES *nes)
{
    PPU *ppu = &nes->ppu;

    u8 value = ReadPPUU8(nes, ppu->v);

    // emulate buffered reads
    if (ppu->v % 0x4000 < 0x3F00)
    {
        u8 buffered = ppu->data;
        ppu->data = value;
        value = buffered;
    }
    else
    {
        ppu->data = ReadPPUU8(nes, ppu->v - 0x1000);
    }

    // if increment mode = 1, increment v by 32
    if (HAS_FLAG(ppu->control, BIT2_MASK))
    {
        ppu->v += 32;
    }
    else
    {
        ppu->v += 1;
    }

    return value;
}

inline void WriteVramData(NES *nes, u8 value)
{
    PPU *ppu = &nes->ppu;
    ppu->data = value;
    WritePPUU8(nes, ppu->v, value);
    // ppu->data = value;

    // if increment mode = 1, increment v by 32
    if (HAS_FLAG(ppu->control, BIT2_MASK))
    {
        ppu->v += 32;
    }
    else
    {
        ppu->v += 1;
    }

    // Least significant bits previously written into a PPU register goes into PPUSTATUS
    ppu->status = (ppu->status & 0xE0) | (value & 0x1F);
}

// This is the fastest method and how is usually implemented in emulators
// I'll try for now wihout using the cpu cycles and writing on the $2004 register
inline void WriteDMA(NES *nes, u8 value)
{
    CPU *cpu = &nes->cpu;
    PPU *ppu = &nes->ppu;

    u16 readAddress = ((u16)value << 8);

    for (u16 offset = 0; offset < 0x100; ++offset)
    {
        u8 data = ReadCPUU8(nes, readAddress + offset);
        WriteU8(&nes->oamMemory, ppu->oamAddress, data);

        ppu->oamAddress++;
    }

    // This emulate the actual work of the CPU 
    // transferring the bytes to the PPU
    cpu->waitCycles = 513;
    if (cpu->cycles & 0x01)
        ++cpu->waitCycles;
}

inline void ColorEmphasis(Color *color, u8 mask)
{
    switch (mask)
    {
        case 0x1:
            color->g = (u8)(color->g * 0.85);
            color->b = (u8)(color->b * 0.85);
            break;
        case 0x2:
            color->r = (u8)(color->r * 0.85);
            color->b = (u8)(color->b * 0.85);
            break;
        case 0x3:
            color->r = (u8)(color->r * 0.85);
            color->g = (u8)(color->g * 0.85);
            color->b = (u8)(color->b * 0.70);
            break;                      
        case 0x4:                       
            color->r = (u8)(color->r * 0.85);
            color->g = (u8)(color->g * 0.85);
            break;                      
        case 0x5:                       
            color->r = (u8)(color->r * 0.85);
            color->g = (u8)(color->g * 0.70);
            color->b = (u8)(color->b * 0.85);
            break;                      
        case 0x6:                       
            color->r = (u8)(color->r * 0.70);
            color->g = (u8)(color->g * 0.85);
            color->b = (u8)(color->b * 0.85);
            break;                      
        case 0x7:                       
            color->r = (u8)(color->r * 0.70);
            color->g = (u8)(color->g * 0.70);
            color->b = (u8)(color->b * 0.70);
            break;
    }
}

inline void SetVerticalBlank(NES *nes)
{
    CPU *cpu = &nes->cpu;
    PPU *ppu = &nes->ppu;

    if (!ppu->suppressNmi)
    {
        SetBitFlag(&ppu->status, VBLANK_FLAG);

        if (GetBitFlag(ppu->control, VBLANK_FLAG))
        {
            cpu->interrupt = CPU_INTERRUPT_NMI;
        }
    }

    ppu->suppressNmi = FALSE;
}

inline void ClearVerticalBlank(NES *nes)
{
    PPU *ppu = &nes->ppu;

    ClearBitFlag(&ppu->status, SCANLINE_COUNT_FLAG);
    ClearBitFlag(&ppu->status, HIT_FLAG);
    ClearBitFlag(&ppu->status, VBLANK_FLAG);

    /*ppu->delayNmi = 0;
    ppu->outputNmi = FALSE;*/
    ppu->suppressNmi = FALSE;
}

void ResetPPU(NES *nes);
void PowerPPU(NES *nes);
void InitPPU(NES *nes);
void StepPPU(NES *nes);

#endif // !PPU_H

