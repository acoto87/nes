#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "nes.h"
#include "cpu.h"
#include "ppu.h"
#include "controller.h"
#include "gui.h"
#include "mapper0.h"
#include "mapper1.h"
#include "mapper2.h"
#include "mapper3.h"

/*
 * NES file format urls
 * https://wiki.nesdev.com/w/index.php/INES
 * http://fms.komkon.org/EMUL8/NES.html#LABM
 * https://wiki.nesdev.com/w/index.php/Emulator_tests
 * http://nesdev.com/NESDoc.pdf
 *
 * OpCodes:
 * http://nesdev.com/6502.txt
 * http://www.oxyron.de/html/opcodes02.html
 *
 * 6502 reference
 * http://www.obelisk.me.uk/6502/reference.html
 *
 * Nes
 * http://graphics.stanford.edu/~ianbuck/proj/Nintendo/Nintendo.html
 *
 * Nes emulators:
 * http://www.fceux.com/web/home.html
 * https://www.qmtpro.com/~nes/nintendulator/
 *
 * Mappers:
 * https://emudocs.org/?page=NES
 *
 * http://web.textfiles.com/games/nestech.txt
 * http://www.optimuscopri.me/nes/report.pdf
 * http://nesdev.com/NES%20emulator%20development%20guide.txt
 */

b32 LoadNesRom(char *filePath, Cartridge *cartridge)
{
    FILE *file = fopen(filePath, "rb");
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
    cartridge->mapper = ((header.flags6 & 0xF0) >> 4) | (header.flags7 & 0xF0);

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

void InitMapper(NES *nes)
{
    switch (nes->cartridge.mapper)
    {
        case 0:
        {
            nes->mapperInit = Mapper0Init;
            nes->mapperReadU8 = Mapper0ReadU8;
            nes->mapperWriteU8 = Mapper0WriteU8;
            break;
        }

        case 1:
        {
            nes->mapperInit = Mapper1Init;
            nes->mapperReadU8 = Mapper1ReadU8;
            nes->mapperWriteU8 = Mapper1WriteU8;
            break;
        }

        case 2:
        {
            nes->mapperInit = Mapper2Init;
            nes->mapperReadU8 = Mapper2ReadU8;
            nes->mapperWriteU8 = Mapper2WriteU8;
            break;
        }

        case 3:
        {
            nes->mapperInit = Mapper3Init;
            nes->mapperReadU8 = Mapper3ReadU8;
            nes->mapperWriteU8 = Mapper3WriteU8;
            break;
        }

        default:
        {
            ASSERT(FALSE);
            break;
        }
    }

    nes->mapperInit(nes);
}

NES* CreateNES(Cartridge cartridge)
{
    NES* nes = (NES*)Allocate(sizeof(NES));

    if (nes)
    {
        nes->cartridge = cartridge;
    
        InitMapper(nes);
        InitCPU(nes);
        InitPPU(nes);
        //InitAPU(nes)
        InitGUI(nes);
        InitController(nes, 0);
        InitController(nes, 1);

        if (cartridge.hasBatteryPack)
        {
            // load battery ram
        }
    }

    return nes;
}

void ResetNES(NES *nes)
{
    ResetCPU(nes);
    ResetPPU(nes);
    // ResetAPU(nes);
    ResetGUI(nes);
    ResetController(nes, 0);
    ResetController(nes, 1);
}

void Destroy(NES *nes)
{
    DestroyMemory(&nes->cpuMemory);
    DestroyMemory(&nes->ppuMemory);
    DestroyMemory(&nes->oamMemory);
    DestroyMemory(&nes->oamMemory2);

    if (nes->cartridge.chr)
    {
        Free(nes->cartridge.chr);
    }

    if (nes->cartridge.pgr)
    {
        Free(nes->cartridge.pgr);
    }

    if (nes->cartridge.title)
    {
        Free(nes->cartridge.title);
    }

    if (nes->cartridge.trainer)
    {
        Free(nes->cartridge.trainer);
    }

    Free(nes);
}

