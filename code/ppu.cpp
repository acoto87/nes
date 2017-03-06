#include "memory.h"
#include "ppu.h"
#include "oam.h"
#include "gui.h"

/*
 * http://wiki.nesdev.com/w/index.php/PPU_sprite_evaluation
 */

internal inline u8 GetCurrentY(PPU *ppu)
{
    // since there are 262 scanlines, but the screen size in NTSC is 240, 
    // there are 21 scanlines that aren't part of the visible scanlines
    return ppu->scanline;
}

internal inline u8 GetCurrentX(PPU *ppu)
{
    // since the ppu render 1 pixel per scanline cycle, this refers to the x position in the screen
    return ppu->cycle;
}

internal void RenderPixel(NES *nes)
{
    PPU *ppu = &nes->ppu;
    GUI *gui = &nes->gui;

    b32 renderBackground = GetBitFlag(ppu->mask, BACKGROUND_ENABLED_FLAG);
    b32 renderSprites = GetBitFlag(ppu->mask, SPRITES_ENABLED_FLAG);

    u8 background = 0;
    u8 sprite = 0, a = 0, idx = -1;

    u8 x = GetCurrentX(ppu);
    u8 y = GetCurrentY(ppu);

    if (renderBackground)
    {
        background = (u8)(((u32)(ppu->tileData >> 32)) >> ((7 - ppu->x) * 4)) & 0xF;

        if (x < 8 && !GetBitFlag(ppu->mask, BACKGROUND_CLIP_MASK_FLAG))
        {
            background = 0;
        }
    }

    if (renderSprites)
    {
        u16 spriteBaseAddress = 0x1000 * GetBitFlag(ppu->control, SPRITE_ADDR_FLAG);

        for (s32 i = 0; i < ppu->spriteCount; ++i)
        {
            u8 spriteY = ReadU8(&nes->oamMemory2, i * 4 + 0);
            u8 spriteIdx = ReadU8(&nes->oamMemory2, i * 4 + 1);
            u8 spriteAttr = ReadU8(&nes->oamMemory2, i * 4 + 2);
            u8 spriteX = ReadU8(&nes->oamMemory2, i * 4 + 3);

            u8 rowOffset = y - spriteY - 1;
            u8 colOffset = x - spriteX;

            // the bit 6 indicate that the sprite should flip horizontally
            if (spriteAttr & 0x40)
            {
                // @TODO: this doesn't care about 8x16 sprites
                colOffset = 7 - colOffset;
            }

            // the bit 7 indicate that the sprite should flip vertically
            if (spriteAttr & 0x80)
            {
                // @TODO: this doesn't care about 8x16 sprites
                rowOffset = 7 - rowOffset;
            }

            if (colOffset >= 0 && colOffset < 8)
            {
                u8 row1 = ReadPPUU8(nes, spriteBaseAddress + spriteIdx * 16 + rowOffset);
                u8 row2 = ReadPPUU8(nes, spriteBaseAddress + spriteIdx * 16 + 8 + rowOffset);

                u8 h = ((row2 >> (7 - colOffset)) & 0x1);
                u8 l = ((row1 >> (7 - colOffset)) & 0x1);
                sprite = (h << 0x1) | l;

                sprite |= (spriteAttr & 0x03) << 2;
                if (sprite % 4 != 0)
                {
                    idx = i;
                    a = spriteAttr;
                    break;
                }
            }
        }

        if (x < 8 && !GetBitFlag(ppu->mask, SPRITE_CLIP_MASK_FLAG))
        {
            sprite = 0;
        }
    }

    b32 b = background % 4 != 0;
    b32 s = sprite % 4 != 0;

    u8 colorIndex;

    if (!b && !s)
    {
        colorIndex = 0;
    }
    else if (!b && s)
    {
        // AND (&) with 0x10 to make sure that the color is picked from palette 2 (0x3F10)
        colorIndex = sprite | 0x10;
    }
    else if (b && !s)
    {
        colorIndex = background;
    }
    else
    {
        // if the sprite is with index 0, the set the sprite 0 hit flag
        if (idx == 0 && x < 255)
        {
            SetBitFlag(&ppu->status, HIT_FLAG);
        }

        // the bit 5 indicate that the sprite has priority over the background
        // 0 - front, 1 - back
        if (!(a & 0x20))
        {
            // AND (&) with 0x10 to make sure that the color is picked from palette 2 (0x3F10)
            colorIndex = sprite | 0x10;
        }
        else 
        {
            colorIndex = background;
        }
    }

    colorIndex = ReadPPUU8(nes, 0x3F00 + colorIndex);

    // if the grayscale bit is set, then AND (&) with 0x30 to set
    // any color in the palette to the grey ones
    if (GetBitFlag(ppu->mask, COLOR_FLAG))
    {
        colorIndex &= 0x30;
    }

    Color color = systemPalette[colorIndex % 64];

    // check the bits 5, 6, 7 to color emphasis
    u8 colorMask = (ppu->mask & 0xE0) >> 5;
    if (colorMask != 0)
    {
        ColorEmphasis(&color, colorMask);
    }

    // draw pixel at 'x', 'y' with color 'color'
    SetPixel(gui, x, y, color);
}

// from: https://wiki.nesdev.com/w/index.php?title=PPU_scrolling
void FetchNameTableByte(NES *nes)
{
    PPU *ppu = &nes->ppu;
    u16 v = ppu->v;
    u16 address = 0x2000 | (v & 0x0FFF);
    ppu->nameTableByte = ReadPPUU8(nes, address);
}

// from: https://wiki.nesdev.com/w/index.php?title=PPU_scrolling
void FetchAttrTableByte(NES *nes)
{
    PPU *ppu = &nes->ppu;
    u16 v = ppu->v;
    u16 address = 0x23C0 | (v & 0x0C00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07);
    u16 shift = ((v >> 4) & 4) | (v & 2);

    u8 a = ReadPPUU8(nes, address);
    ppu->attrTableByte = ((a >> shift) & 3) << 2;
}

// from: https://wiki.nesdev.com/w/index.php?title=PPU_scrolling
void FetchLowTileByte(NES *nes)
{
    PPU *ppu = &nes->ppu;
    u8 tile = ppu->nameTableByte;
    u8 table = GetBitFlag(ppu->control, BACKGROUND_ADDR_FLAG);
    u16 fineY = (ppu->v >> 12) & 7;
    u16 address = 0x1000 * (u16)table + (u16)tile * 16 + fineY;
    ppu->lowTileByte = ReadPPUU8(nes, address);
}

// from: https://wiki.nesdev.com/w/index.php?title=PPU_scrolling
void FetchHighTileByte(NES *nes)
{
    PPU *ppu = &nes->ppu;
    u8 tile = ppu->nameTableByte;
    u8 table = GetBitFlag(ppu->control, BACKGROUND_ADDR_FLAG);
    u16 fineY = (ppu->v >> 12) & 7;
    u16 address = 0x1000 * (u16)table + (u16)tile * 16 + fineY;
    ppu->highTileByte = ReadPPUU8(nes, address + 8);
}

void StoreTileData(NES *nes)
{
    PPU *ppu = &nes->ppu;

    u32 data = 0;
    for (s32 i = 0; i < 8; i++)
    {
        u8 a = ppu->attrTableByte;
        u8 p1 = (ppu->lowTileByte & 0x80) >> 7;
        u8 p2 = (ppu->highTileByte & 0x80) >> 6;
        ppu->lowTileByte <<= 1;
        ppu->highTileByte <<= 1;
        data <<= 4;
        data |= (u32)(a | p1 | p2);
    }
    ppu->tileData |= (u64)data;
}

// from: https://wiki.nesdev.com/w/index.php?title=PPU_scrolling
void IncrementX(NES *nes)
{
    PPU *ppu = &nes->ppu;

    // increment hori(v)
    // if coarse X == 31
    if ((ppu->v & 0x001F) == 31) {
        // coarse X = 0
        ppu->v &= 0xFFE0;
        // switch horizontal nametable
        ppu->v ^= 0x0400;
    }
    else {
        // increment coarse X
        ppu->v++;
    }
}

// from: https://wiki.nesdev.com/w/index.php?title=PPU_scrolling
void IncrementY(NES *nes)
{
    PPU *ppu = &nes->ppu;

    // increment vert(v)
    // if fine Y < 7
    if ((ppu->v & 0x7000) != 0x7000) {
        // increment fine Y
        ppu->v += 0x1000;
    }
    else
    {
        // fine Y = 0
        ppu->v &= 0x8FFF;
        // let y = coarse Y
        u16 y = (ppu->v & 0x03E0) >> 5;
        if (y == 29) {
            // coarse Y = 0
            y = 0;
            // switch vertical nametable
            ppu->v ^= 0x0800;
        }
        else if (y == 31) {
            // coarse Y = 0, nametable not switched
            y = 0;
        }
        else {
            // increment coarse Y
            y++;
        }
        // put coarse Y back into v
        ppu->v = (ppu->v & 0xFC1F) | (y << 5);
    }
}

// from: https://wiki.nesdev.com/w/index.php?title=PPU_scrolling
void CopyX(NES *nes)
{
    PPU *ppu = &nes->ppu;

    // hori(v) = hori(t)
    // v: .....F.. ...EDCBA = t: .....F.. ...EDCBA
    ppu->v = (ppu->v & 0xFBE0) | (ppu->t & 0x041F);
}

// from: https://wiki.nesdev.com/w/index.php?title=PPU_scrolling
void CopyY(NES *nes)
{
    PPU *ppu = &nes->ppu;

    // vert(v) = vert(t)
    // v: .IHGF.ED CBA..... = t: .IHGF.ED CBA.....
    ppu->v = (ppu->v & 0x841F) | (ppu->t & 0x7BE0);
}

void StepPPU(NES *nes)
{
    CPU *cpu = &nes->cpu;
    PPU *ppu = &nes->ppu;
    GUI *gui = &nes->gui;

    b32 renderBackground = GetBitFlag(ppu->mask, BACKGROUND_ENABLED_FLAG);
    b32 renderSprites = GetBitFlag(ppu->mask, SPRITES_ENABLED_FLAG);
    b32 renderEnabled = renderBackground || renderSprites;

    if (renderEnabled)
    {
        /*
        * After rendering 1 dummy scanline, the PPU starts to render the
        * actual data to be displayed on the screen. This is done for 240 scanlines,
        * of course.
        */

        if (ppu->scanline >= 0 && ppu->scanline <= 239)
        {
            if (ppu->cycle >= 1 && ppu->cycle <= 256)
            {
                RenderPixel(nes);

                ppu->tileData <<= 4;
                switch (ppu->cycle % 8)
                {
                    case 1:
                    {
                        FetchNameTableByte(nes);
                        break;
                    }
                    case 3:
                    {
                        FetchAttrTableByte(nes);
                        break;
                    }
                    case 5:
                    {
                        FetchLowTileByte(nes);
                        break;
                    }
                    case 7:
                    {
                        FetchHighTileByte(nes);
                        break;
                    }
                    case 0:
                    {
                        StoreTileData(nes);
                        IncrementX(nes);
                        break;
                    }
                }

                if (ppu->cycle == 256)
                {
                    IncrementY(nes);
                }
            }
            else if (ppu->cycle == 257)
            {
                CopyX(nes);
            }
            else if (ppu->cycle >= 321 && ppu->cycle <= 336)
            {
                ppu->tileData <<= 4;
                switch (ppu->cycle % 8)
                {
                    case 1:
                    {
                        FetchNameTableByte(nes);
                        break;
                    }
                    case 3:
                    {
                        FetchAttrTableByte(nes);
                        break;
                    }
                    case 5:
                    {
                        FetchLowTileByte(nes);
                        break;
                    }
                    case 7:
                    {
                        FetchHighTileByte(nes);
                        break;
                    }
                    case 0:
                    {
                        StoreTileData(nes);
                        IncrementX(nes);
                        break;
                    }
                }
            }
        }
        else if (ppu->scanline == 240)
        {
            /*
            * after the very last rendered scanline finishes, the PPU does nothing
            * for 1 scanline (i.e. the programmer gets screwed out of perfectly good VINT time).
            * When this scanline finishes, the VINT flag is set, and the process of drawing lines
            * starts all over again.
            */
        }
        else if (ppu->scanline >= 241 && ppu->scanline <= 260)
        {
            // do nothing, we are in vblank
        }
        else if (ppu->scanline == 261)
        {
            /*
            * After 20 scanlines worth of time go by (since the VINT flag was set),
            * the PPU starts to render scanlines. This first scanline is a dummy one;
            * although it will access it's external memory in the same sequence it would
            * for drawing a valid scanline, no on-screen pixels are rendered during this
            * time, making the fetched background data immaterial. Both horizontal *and*
            * vertical scroll counters are updated (presumably) at cc offset 256 in this
            * scanline. Other than that, the operation of this scanline is identical to
            * any other. The primary reason this scanline exists is to start the object
            * render pipeline, since it takes 256 cc's worth of time to determine which
            * objects are in range or not for any particular scanline.
            */

            if (ppu->cycle >= 1 && ppu->cycle <= 256)
            {
                if (ppu->cycle % 8 == 0)
                {
                    IncrementX(nes);
                }

                if (ppu->cycle == 256)
                {
                    IncrementY(nes);
                }
            }
            else if (ppu->cycle == 257)
            {
                CopyX(nes);
            }
            else if (ppu->cycle >= 280 && ppu->cycle <= 304)
            {
                CopyY(nes);
            }
            else if (ppu->cycle >= 321 && ppu->cycle <= 336)
            {
                if (ppu->cycle % 8 == 0)
                {
                    IncrementX(nes);
                }
            }
        }
        else
        {
            // It's suppose to never reach here
            ASSERT(FALSE);
        }
    }

    if (renderEnabled)
    {
        if (ppu->scanline >= 0 && ppu->scanline <= 239)
        {
            if (ppu->cycle == 257)
            {
                u8 h = GetBitFlag(ppu->control, SPRITE_SIZE_FLAG) ? 16 : 8;

                s32 count = 0;
                for (s32 i = 0; i < 64; ++i)
                {
                    u8 y = ReadU8(&nes->oamMemory, i * 4 + 0);
                    u8 idx = ReadU8(&nes->oamMemory, i * 4 + 1);
                    u8 a = ReadU8(&nes->oamMemory, i * 4 + 2);
                    u8 x = ReadU8(&nes->oamMemory, i * 4 + 3);

                    u8 row = (u8)ppu->scanline - y;
                    if (row >= 0 && row < h)
                    {
                        if (count < 8)
                        {
                            WriteU8(&nes->oamMemory2, count * 4 + 0, y);
                            WriteU8(&nes->oamMemory2, count * 4 + 1, idx);
                            WriteU8(&nes->oamMemory2, count * 4 + 2, a);
                            WriteU8(&nes->oamMemory2, count * 4 + 3, x);
                        }

                        ++count;
                    }

                    if (count > 8)
                    {
                        count = 8;
                        SetBitFlag(&ppu->status, SCANLINE_COUNT_FLAG);
                    }
                }

                ppu->spriteCount = count;
            }
        }
        else
        {
            for (s32 i = 0; i < 8 * 4; ++i)
            {
                WriteU8(&nes->oamMemory2, i, 0);
            }

            ppu->spriteCount = 0;
        }

        //if (ppu->cycle >= 1 && ppu->cycle <= 64)
        //{
        //    // Cycles 1 - 64: Secondary OAM(32 - byte buffer for current sprites on scanline) is initialized to $FF - 
        //    // attempting to read $2004 will return $FF. Internally, the clear operation is implemented by reading from 
        //    // the OAM and writing into the secondary OAM as usual, only a signal is active that makes the read always return $FF.
        //    //
        //    // I think is one cycle for the reading and other for the writing. 
        //    // The read through $2004 should return 0xFF if the ppu is in this phase.
        //    // For now, I'll skip the reading part and only write 0xFF to oamMemory2.

        //    if (ppu->cycle & 0x01)
        //    {
        //        WriteU8(&nes->oamMemory2, ppu->cycle / 2, 0xFF);
        //    }
        //}
        //else if (ppu->cycle >= 65 && ppu->cycle <= 256)
        //{
        //    /*Cycles 65 - 256: Sprite evaluation
        //        On odd cycles, data is read from(primary) OAM
        //        On even cycles, data is written to secondary OAM(unless secondary OAM is full, in which case it will read the value in secondary OAM instead)
        //        1. Starting at n = 0, read a sprite's Y-coordinate (OAM[n][0], copying it to the next open slot in secondary OAM (unless 8 sprites have been found, in which case the write is ignored).
        //            1a.If Y - coordinate is in range, copy remaining bytes of sprite data(OAM[n][1] thru OAM[n][3]) into secondary OAM.
        //        2. Increment n
        //            2a.If n has overflowed back to zero(all 64 sprites evaluated), go to 4
        //            2b.If less than 8 sprites have been found, go to 1
        //            2c.If exactly 8 sprites have been found, disable writes to secondary OAM because it is full.This causes sprites in back to drop out.
        //        3. Starting at m = 0, evaluate OAM[n][m] as a Y - coordinate.
        //            3a.If the value is in range, set the sprite overflow flag in $2002 and read the next 3 entries of OAM(incrementing 'm' after each byte and incrementing 'n' when 'm' overflows); if m = 3, increment n
        //            3b.If the value is not in range, increment n and m(without carry).If n overflows to 0, go to 4; otherwise go to 3
        //                The m increment is a hardware bug - if only n was incremented, the overflow flag would be set whenever more than 8 sprites were present on the same scanline, as expected.
        //        4. Attempt(and fail) to copy OAM[n][0] into the next free slot in secondary OAM, and increment n(repeat until HBLANK is reached)*/

        //    if (ppu->cycle == 65)
        //    {
        //        ppu->spriteN = 0;
        //        ppu->spriteM = 0;
        //        ppu->spriteCount = 0;
        //    }

        //    // Este proceso deberia respetar los tiempos de los ciclos del PPU
        //    if (ppu->spriteN < 64)
        //    {
        //        u8 h = GetBitFlag(ppu->control, 0x20) ? 16 : 8;
        //        u8 spriteY = ReadU8(&nes->oamMemory, ppu->spriteN * 4 + 0);

        //        if (ppu->spriteCount < 8)
        //        {
        //            WriteU8(&nes->oamMemory2, ppu->spriteCount * 4 + 0, spriteY);

        //            u8 row = (u8)ppu->scanline - spriteY;
        //            if (row >= 0 && row < h)
        //            {
        //                u8 patternIndex = ReadU8(&nes->oamMemory, ppu->spriteN * 4 + 1);
        //                WriteU8(&nes->oamMemory2, ppu->spriteCount * 4 + 1, patternIndex);

        //                u8 attr = ReadU8(&nes->oamMemory, ppu->spriteN * 4 + 2);
        //                WriteU8(&nes->oamMemory2, ppu->spriteCount * 4 + 2, attr);

        //                u8 spriteX = ReadU8(&nes->oamMemory, ppu->spriteN * 4 + 3);
        //                WriteU8(&nes->oamMemory2, ppu->spriteCount * 4 + 3, spriteX);
        //            }

        //            ppu->spriteCount++;
        //        }
        //        
        //        ppu->spriteN++;

        //        if (ppu->spriteCount < 64)
        //        {
        //            if (ppu->spriteCount = 8)
        //            {
        //                u8 spriteY = ReadU8(&nes->oamMemory, ppu->spriteN * 4 + ppu->spriteM);
        //                u8 row = (u8)ppu->scanline - spriteY;
        //                if (row >= 0 && row < h)
        //                {
        //                    SetBitFlag(&ppu->status, 0x20);
        //                }
        //                else
        //                {
        //                    ppu->spriteN++;

        //                    // The m increment is a hardware bug - if only n was incremented, 
        //                    // the overflow flag would be set whenever more than 8 sprites were present on the same scanline, as expected.
        //                    ppu->spriteM++;
        //                }
        //            }
        //        }
        //    }
        //}
        //else if (ppu->cycle >= 257 && ppu->cycle <= 320)
        //{
        //    if (ppu->cycle = 257)
        //    {
        //        ppu->spriteN = 0;
        //        ppu->spriteM = 0;
        //    }

        //    /*Cycles 257 - 320: Sprite fetches(8 sprites total, 8 cycles per sprite)
        //        1 - 4 : Read the Y - coordinate, tile number, attributes, and X - coordinate of the selected sprite from secondary OAM
        //        5 - 8 : Read the X - coordinate of the selected sprite from secondary OAM 4 times(while the PPU fetches the sprite tile data)
        //        For the first empty sprite slot, this will consist of sprite #63's Y-coordinate followed by 3 $FF bytes; for subsequent empty sprite slots, this will be four $FF bytes*/


        //}
        //else if (ppu->cycle >= 257 && ppu->cycle <= 320)
        //{
        //    /*Cycles 321 - 340 + 0: Background render pipeline initialization
        //        Read the first byte in secondary OAM(while the PPU fetches the first two background tiles for the next scanline)*/
        //}
        //else
        //{
        //    // no deberia llegar aqui
        //    ASSERT(FALSE);
        //}
    }

    ppu->cycle++;
    ppu->totalCycles++;

    if (ppu->scanline == 241)
    {
        if (ppu->cycle == 1 && ppu->frameCount > 0)
        {
            SetVerticalBlank(nes);
        }
    }
    else if (ppu->scanline == 261)
    {
        if (ppu->cycle == 1)
        {
            ClearVerticalBlank(nes);
        }
    }

    if (ppu->cycle == 341)
    {
        if (ppu->scanline == 261)
        {
            ppu->cycle = 0;
            ppu->scanline = 0;
            ppu->frameCount++;
        }
        else
        {
            ppu->cycle = 0;
            ppu->scanline++;
        }
    }
    else if (ppu->cycle == 340)
    {
        if (renderBackground || renderSprites)
        {
            if (ppu->scanline == 261 && (ppu->frameCount & 0x1) /* ppu->f == 1 */)
            {
                ppu->cycle = 0;
                ppu->scanline = 0;
                ppu->frameCount++;
            }
        }
    }
}

void ResetPPU(NES *nes)
{
    PPU *ppu = &nes->ppu;

    ppu->cycle = 0;
    ppu->scanline = 241;
    ppu->totalCycles = 0;
    ppu->frameCount = 0;

    ppu->control = 0;
    ppu->mask = 0;
    ppu->status = 0;
    ppu->address = 0;
    ppu->data = 0;
    ppu->oamAddress = 0;
    ppu->oamData = 0;
}

void PowerPPU(NES *nes)
{
    PPU *ppu = &nes->ppu;

    ppu->cycle = 0;
    ppu->scanline = 241;
    ppu->totalCycles = 0;
    ppu->frameCount = 0;

    ppu->control = 0;
    ppu->mask = 0;
    ppu->status = 0;
    ppu->address = 0;
    ppu->data = 0;
    ppu->oamAddress = 0;
    ppu->oamData = 0;
}

void InitPPU(NES *nes)
{
    if (!nes->ppuMemory.created)
    {
        CreateMemory(&nes->ppuMemory, PPU_RAM_SIZE);

        u32 chrSizeInBytes = nes->cartridge.chrSizeInBytes;
        if (chrSizeInBytes > 0)
        {
            u8 *chr = nes->cartridge.chr;
            CopyMemoryBytes(&nes->ppuMemory, 0x0000, chr, MAX(chrSizeInBytes, 0x2000));
        }

        CreateMemory(&nes->oamMemory, 256);
        CreateMemory(&nes->oamMemory2, 32);
    }

    PowerPPU(nes);
}