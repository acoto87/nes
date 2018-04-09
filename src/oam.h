#pragma once
#ifndef OAM_H
#define OAM_H

/* 
 * There is an independent area of memory known as sprite ram, which is 256 bytes in size.  
 * This memory stores 4 bytes of information for 64 sprites. 
 * These 4 bytes contain the x and y location of the sprite on the screen, the upper two color bits, 
 * the tile index number (pattern table tile of the sprite), and information about flipping (horizontal and vertical), 
 * and priority (behind/on top of background).  
 * Sprite ram can be accessed byte-by-byte through the NES registers, 
 * or also can be loaded via DMA transfer through another register. 
 */

#define OAM_FIRST_ADDRESS 0 
#define OAM_LAST_ADDRESS 255 

#define OAM_NUM_ADDRESSES (OAM_LAST_ADDRESS + 1) 

#define OAM_BYTES_PER_SPRITE 4 

#define OAM_NUMBER_OF_SPRITES (OAM_NUM_ADDRESSES / OAM_BYTES_PER_SPRITE)

#define OAM_BYTES_PER_SPRITE 4

#define OAM_Y_BYTE_OFFSET 0
#define OAM_TILE_BYTE_OFFSET 1
#define OAM_ATTRIBUTES_BYTE_OFFSET 2
#define OAM_X_BYTE_OFFSET 3

#define MASK_OAM_FLIP_HORIZONTAL_ON BIT6_MASK
#define MASK_OAM_FLIP_VERTICAL_ON BIT7_MASK
#define MASK_OAM_BEHIND_BACKGROUND_ON BIT5_MASK
#define MASK_OAM_BANK_NUMBER_ON BIT0_MASK

inline u8 GetSpriteX(Memory *oamMemory, u8 spriteIndex)
{
    u16 address = (OAM_BYTES_PER_SPRITE * spriteIndex) + OAM_X_BYTE_OFFSET;
    return ReadU8(oamMemory, address);
}

inline u8 GetSpriteY(Memory *oamMemory, u8 spriteIndex)
{
    u16 address = (OAM_BYTES_PER_SPRITE * spriteIndex) + OAM_Y_BYTE_OFFSET;
    return ReadU8(oamMemory, address);
}

inline u8 GetBankNumber(Memory *oamMemory, u8 spriteIndex)
{
    u16 address = (OAM_BYTES_PER_SPRITE * spriteIndex) + OAM_TILE_BYTE_OFFSET;
    u8 data = ReadU8(oamMemory, address);
    return HAS_FLAG(data, MASK_OAM_BANK_NUMBER_ON) ? 1 : 0;
}

inline u8 GetTileNumber(Memory *oamMemory, u8 spriteIndex, b32 using8x16)
{
    u16 address = (OAM_BYTES_PER_SPRITE * spriteIndex) + OAM_TILE_BYTE_OFFSET;   
    u8 data = ReadU8(oamMemory, address);

    if (!using8x16) 
    { 
        return data; 
    }
    
    // lose the 0th bit     
    return (data >> 1) << 1;
}

inline u8 GetAttributes(Memory *oamMemory, u8 spriteIndex)
{
    u16 address = (OAM_BYTES_PER_SPRITE * spriteIndex) + OAM_ATTRIBUTES_BYTE_OFFSET;
    return ReadU8(oamMemory, address);
}

inline u8 GetPalette(Memory *oamMemory, u8 spriteIndex)
{
    u8 attributes = GetAttributes(oamMemory, spriteIndex);
    
    // lose the 765432 bits   
    attributes = (attributes << 6) >> 4;
    return attributes;
}

inline b32 IsFlippedHorizontal(Memory *oamMemory, u8 spriteIndex)
{
    u8 attributes = GetAttributes(oamMemory, spriteIndex);
    return HAS_FLAG(attributes, MASK_OAM_FLIP_HORIZONTAL_ON);
}

inline b32 IsFlippedVertical(Memory *oamMemory, u8 spriteIndex)
{
    u8 attributes = GetAttributes(oamMemory, spriteIndex);
    return HAS_FLAG(attributes, MASK_OAM_FLIP_VERTICAL_ON);
}

inline b32 IsBehindBackground(Memory *oamMemory, u8 spriteIndex)
{
    u8 attributes = GetAttributes(oamMemory, spriteIndex);
    return HAS_FLAG(attributes, MASK_OAM_BEHIND_BACKGROUND_ON);
}

#endif // !OAM_H

