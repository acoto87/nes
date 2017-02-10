#include "cartridge.h"

b32 LoadNesRom(char *filePath, Cartridge *cartridge)
{
    FILE *file = fopen( filePath, "rb");
    if (!file)
    {
        return false;
    }

    CartridgeHeader header;
    s64 read = fread(&header, sizeof(u8), HEADER_SIZE, file);
    if (read == 0)
    {
        fclose(file);
        return false;
    }

    if (!(header.nesStr[0] == 'N' &&
        header.nesStr[1] == 'E' &&
        header.nesStr[2] == 'S' &&
        header.nesStr[3] == 0x1A))
    {
        fclose(file);
        return false;
    }

    cartridge->hasTrainer = HAS_FLAG(header.flags6, TRAINER_MASK);
    if (cartridge->hasTrainer)
    {
        fread(&cartridge->trainer, sizeof(u8), TRAINER_SIZE, file);
    }

    if (HAS_FLAG(header.flags6, VRAMLAYOUT_MASK))
    {
        cartridge->mirrorType = MIRROR_FOUR;
    }
    else if (HAS_FLAG(header.flags6, VMIRROR_MASK))
    {
        cartridge->mirrorType = MIRROR_VERTICAL;
    }
    else
    {
        cartridge->mirrorType = MIRROR_HORIZONTAL;
    }

    cartridge->hasBatteryPack = HAS_FLAG(header.flags6, BATTERYPACKED_MASK);
    cartridge->prgRAMSize = header.prgRAMSize;
    cartridge->memoryMapper = (header.flags6 & U8HIGH_MASK) | (header.flags7 & U8LOW_MASK);
    
    cartridge->pgrBanks = header.prgROMSize;
    cartridge->pgrSizeInBytes = cartridge->pgrBanks * CPU_PGR_BANK_SIZE;
    cartridge->pgr = (u8*)Allocate(cartridge->pgrSizeInBytes);
    read = fread(cartridge->pgr, sizeof(u8), cartridge->pgrSizeInBytes, file);

    cartridge->chrBanks = header.chrROMSize;
    cartridge->chrSizeInBytes = cartridge->chrBanks * CHR_BANK_SIZE;
    cartridge->chr = (u8*)Allocate(cartridge->chrSizeInBytes);
    read = fread(cartridge->chr, sizeof(u8), cartridge->chrSizeInBytes, file);

    fread(cartridge->title, sizeof(u8), MAX_TITLE_LENGTH, file);

    fclose(file);

    return true;
}