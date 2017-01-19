#include "memory.h"
#include "ppu.h"
#include "oam.h"
#include "gui.h"

/*
 * http://wiki.nesdev.com/w/index.php/PPU_sprite_evaluation
 */

internal inline u16 GetBaseSpritePatternAddress(PPU *ppu)
{
    return GetBitFlag(ppu->control, SPRITE_ADDR_FLAG)
        ? PPU_PATTERN_TABLE_1_OFFSET
        : PPU_PATTERN_TABLE_0_OFFSET;
}

internal inline u16 GetBaseBackgroundPatternAddress(PPU *ppu)
{
    return GetBitFlag(ppu->control, BACKGROUND_ADDR_FLAG)
        ? PPU_PATTERN_TABLE_1_OFFSET
        : PPU_PATTERN_TABLE_0_OFFSET;
}

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

internal u8 GetPatternColorIndex(NES *nes, u16 basePatternAddress, u8 patternTileNumber, u8 x, u8 y)
{
    PPU *ppu = &nes->ppu;

    u16 horizontalOffset = patternTileNumber * PPU_PATTERN_BYTES_PER_TILE;
    u16 verticalOffset = (y % PPU_VERTICAL_PIXELS_PER_TILE);

    u16 pattern1Address = basePatternAddress + horizontalOffset + verticalOffset;
    u16 pattern2Address = pattern1Address + 8;

    u8 pattern1 = ReadPPUU8(nes, pattern1Address);
    u8 pattern2 = ReadPPUU8(nes, pattern2Address);

    // combine pattern bytes
    pattern1 = pattern1 << (x % PPU_HORIZONTAL_PIXELS_PER_TILE);
    pattern1 = pattern1 >> (PPU_HORIZONTAL_PIXELS_PER_TILE - 1);

    pattern2 = pattern2 << (x % PPU_HORIZONTAL_PIXELS_PER_TILE);
    pattern2 = pattern2 >> (PPU_HORIZONTAL_PIXELS_PER_TILE - 1);
    pattern2 = pattern2 << 1;

    u8 patternIndex = pattern1 + pattern2;
    ASSERT(patternIndex <= 3);

    return patternIndex;
}

internal inline u16 GetBaseNametableAddress(PPU *ppu)
{
    u16 nametableStart = PPU_NAME_TABLE_0_FIRST_ADDRESS;
    /*
        if (GetBitFlag(ppu->control, NAMETABLE_Y_SCROLL_FLAG))
        {
            nametableStart += PPU_NAME_TABLE_SIZE;
        }

        if (GetBitFlag(ppu->control, NAMETABLE_X_SCROLL_FLAG))
        {
            nametableStart += PPU_NAME_TABLE_SIZE;
            nametableStart += PPU_NAME_TABLE_SIZE;
        }
    */
    return nametableStart;
}

internal inline u16 GetBackgroundTileNumber(u8 x, u8 y)
{
    u8 horizontalOffset = (x / PPU_HORIZONTAL_PIXELS_PER_TILE);
    u8 verticalOffset = (y / PPU_VERTICAL_PIXELS_PER_TILE) * PPU_BACKGROUND_TILES_PER_ROW;

    u16 tileNumber = horizontalOffset + verticalOffset;
    return tileNumber;
}

internal u8 GetBackgroundColor(NES *nes)
{
    PPU *ppu = &nes->ppu;

    u8 backgroundColorIndex = 0;

    // get current background color
    if (GetBitFlag(ppu->mask, BACKGROUND_ENABLED_FLAG))
    {
        u8 screenX = GetCurrentX(ppu);
        u8 screenY = GetCurrentY(ppu);

        // get the background nametable byte
        u8 patternTileNumber;
        {
            u16 nametableStart = GetBaseNametableAddress(ppu);

            ASSERT(nametableStart == PPU_NAME_TABLE_0_FIRST_ADDRESS ||
                nametableStart == PPU_NAME_TABLE_1_FIRST_ADDRESS ||
                nametableStart == PPU_NAME_TABLE_2_FIRST_ADDRESS ||
                nametableStart == PPU_NAME_TABLE_3_FIRST_ADDRESS);

            u16 tileNumber = GetBackgroundTileNumber(screenX, screenY);
            u16 nametableAddress = nametableStart + tileNumber;

            patternTileNumber = ReadPPUU8(nes, nametableAddress);
        }

        // get the address of the background as the control1 flag is set
        u8 basePatternAddress = GetBaseBackgroundPatternAddress(ppu);
        // get pattern color index
        u8 patternColorIndex = GetPatternColorIndex(nes, basePatternAddress, patternTileNumber, screenX, screenY);

        // get background attribute color index
        u8 attributeColourIndex;
        {
            u16 nametableStart = GetBaseNametableAddress(ppu);

            ASSERT(nametableStart == PPU_NAME_TABLE_0_FIRST_ADDRESS ||
                nametableStart == PPU_NAME_TABLE_1_FIRST_ADDRESS ||
                nametableStart == PPU_NAME_TABLE_2_FIRST_ADDRESS ||
                nametableStart == PPU_NAME_TABLE_3_FIRST_ADDRESS);

            u16 attributetableStart = nametableStart + PPU_NAMETABLE_BYTES_BEFORE_ATTRIBUTE_TABLE;
            u16 tileNumber = GetBackgroundTileNumber(screenX, screenY);

            u16 tileRowNumber = (tileNumber / PPU_BACKGROUND_TILES_PER_ROW);
            u16 tileColumnNumber = tileNumber % PPU_BACKGROUND_TILES_PER_ROW;

            u16 horizontalOffset = (tileColumnNumber / PPU_HORIZONTAL_TILES_PER_ATTRIBUTE_BYTE);
            u16 verticalOffset = (tileRowNumber / PPU_VERTICAL_TILES_PER_ATTRIBUTE_BYTE) * PPU_ATTRIBUTE_BYTES_PER_ROW;
            u16 attributeByteAddress = attributetableStart + horizontalOffset + verticalOffset;
            u8 attributeByte = ReadPPUU8(nes, attributeByteAddress);

            u16 hOffset = (tileNumber % PPU_HORIZONTAL_TILES_PER_ATTRIBUTE_BYTE);
            u16 vOffset = (tileRowNumber % PPU_VERTICAL_TILES_PER_ATTRIBUTE_BYTE);

            s32 attributeTileNumber = attributeTableLookup[vOffset][hOffset];
            switch (attributeTileNumber)
            {
                case 0x0:
                case 0x1:
                case 0x2:
                case 0x3:
                    attributeColourIndex = (attributeByte << 6);
                    break;

                case 0x4:
                case 0x5:
                case 0x6:
                case 0x7:
                    attributeColourIndex = (attributeByte << 4);
                    break;

                case 0x8:
                case 0x9:
                case 0xA:
                case 0xB:
                    attributeColourIndex = (attributeByte << 2);
                    break;

                case 0xC:
                case 0xD:
                case 0xE:
                case 0xF:
                    attributeColourIndex = attributeByte;
                    break;
            }

            attributeColourIndex = attributeColourIndex >> 6;
            attributeColourIndex = attributeColourIndex << 2;
        }

        // if transparent, use the background colour   
        if (patternColorIndex == 0)
        {
            backgroundColorIndex = 0;
        }
        else
        {
            backgroundColorIndex = patternColorIndex + attributeColourIndex;
        }
    }

    return backgroundColorIndex;
}

internal u8 GetSpriteColor(NES *nes, u8 backgroundColorIndex)
{
    PPU *ppu = &nes->ppu;

    u8 spriteColorIndex = 0;

    if (GetBitFlag(ppu->mask, SPRITES_ENABLED_FLAG))
    {
        //u8 screenX = GetCurrentX(ppu);

        //u8 systemColorIndex = 0;

        //if (!ppu->spriteColorsForScanlineSet[screenX])
        //{
        //    u16 address = PPU_BACKGROUND_PALETTE_FIRST_ADDRESS + backgroundColorIndex;
        //    systemColorIndex = ReadPPUU8(nes, address);

        //    // one of the lower two bits set
        //}
        //else if ((ppu->spriteColorsForScanlineIsBehindBackground[screenX]) && (backgroundColorIndex % 4 != 0))
        //{
        //    u16 address = PPU_BACKGROUND_PALETTE_FIRST_ADDRESS + backgroundColorIndex;
        //    systemColorIndex = ReadPPUU8(nes, address);

        //    SetBitFlag(&ppu->status, HIT_FLAG);
        //}
        //else
        //{
        //    u16 address = PPU_SPRITE_PALETTE_FIRST_ADDRESS + ppu->spriteColorsForScanline[screenX];
        //    systemColorIndex = ReadPPUU8(nes, address);

        //    SetBitFlag(&ppu->status, HIT_FLAG);
        //}

        //ASSERT(systemColorIndex < PPU_NUM_SYSTEM_COLOURS);

        //spriteColorIndex = systemColorIndex;
    }

    return spriteColorIndex;
}

internal void RenderPixel(NES *nes)
{
    PPU *ppu = &nes->ppu;
    GUI *gui = &nes->gui;

    //if (ppu->cycle == 0)
    //{
    //    // calculate the colors for the current scanline
    //    CalculateSpriteColorsForScanline(nes);
    //}

    u32 x = GetCurrentX(ppu);
    u32 y = GetCurrentY(ppu);

    // get the background color
    //u8 backgroundColorIndex = GetBackgroundColor(nes);

    u8 backgroundColorIndex = ((ppu->tileData >> 32) >> ((7 - ppu->x) * 4)) & 0x0F;

    // get the sprite color
    // u8 spriteColorIndex = GetSpriteColor(nes, backgroundColorIndex);

    // get color from palettes
    Color color = systemPalette[backgroundColorIndex % 64];

    // draw pixel at 'x', 'y' with color 'color'
    SetPixel(gui, x, y, color);
}

void FetchNameTableByte(NES *nes)
{
    PPU *ppu = &nes->ppu;
    u16 address = ppu->v;
    address = 0x2000 | (address & 0x0FFF);
    ppu->nameTableByte = ReadPPUU8(nes, address);
}

void FetchAttrTableByte(NES *nes)
{
    PPU *ppu = &nes->ppu;
    u16 v = ppu->v;
    u16 address = 0x23C0 | (v & 0x0C00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07);
    u16 shift = ((v >> 4) & 4) | (v & 2);
    ppu->attrTableByte = ((ReadPPUU8(nes, address) >> shift) & 3) << 2;
}

void FetchLowTileByte(NES *nes)
{
    PPU *ppu = &nes->ppu;
    u16 fineY = (ppu->v >> 12) & 7;
    u8 table = GetBitFlag(ppu->control, BACKGROUND_ADDR_FLAG);
    u8 tile = ppu->nameTableByte;
    u16 address = 0x1000 * (u16)table + (u16)tile * 16 + fineY;
    ppu->lowTileByte = ReadPPUU8(nes, address);
}

void FetchHighTileByte(NES *nes)
{
    PPU *ppu = &nes->ppu;
    u16 fineY = (ppu->v >> 12) & 7;
    u8 table = GetBitFlag(ppu->control, BACKGROUND_ADDR_FLAG);
    u8 tile = ppu->nameTableByte;
    u16 address = 0x1000 * (u16)table + (u16)tile * 16 + fineY;
    ppu->lowTileByte = ReadPPUU8(nes, address + 8);
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

    if (renderBackground)
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
            ASSERT(false);
        }
    }

    ppu->cycle++;
    ppu->totalCycles++;

    if (ppu->scanline == 241)
    {
        if (ppu->cycle == 1)
        {
            SetBitFlag(&ppu->status, VBLANK_FLAG);

            if (GetBitFlag(ppu->control, VBLANK_FLAG))
            {
                cpu->interrupt = CPU_INTERRUPT_NMI;
            }
        }
    }
    else if (ppu->scanline == 260)
    {
        if (ppu->cycle == 1)
        {
            ClearBitFlag(&ppu->status, SCANLINE_COUNT_FLAG);
            ClearBitFlag(&ppu->status, HIT_FLAG);
            ClearBitFlag(&ppu->status, VBLANK_FLAG);
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
    ppu->frameCount = 0;

    ppu->control = 0;
    ppu->mask = 0;
    ppu->status = 0;
    ppu->address = 0;
    ppu->data = 0;
    ppu->oamAddress = 0;
    ppu->oamData = 0;

    //ResetSpriteColorsForScanline(ppu);
}

void InitPPU(NES *nes)
{
    if (!nes->ppuMemory.created)
    {
        nes->ppuMemory = *CreateMemory(PPU_RAM_SIZE);

        u32 chrBanks = nes->cartridge.chrBanks;
        u32 chrSizeInBytes = nes->cartridge.chrSizeInBytes;
        u8 *chr = nes->cartridge.chr;

        CopyMemoryBytes(&nes->ppuMemory, PPU_PATTERN_TABLE_0_OFFSET, chr, chrSizeInBytes);

        if (chrBanks < 2)
        {
            CopyMemoryBytes(&nes->ppuMemory, PPU_PATTERN_TABLE_1_OFFSET, chr, chrSizeInBytes);
        }

        nes->oamMemory = *CreateMemory(256);
        nes->oamMemory2 = *CreateMemory(32);
    }

    ResetPPU(nes);
}