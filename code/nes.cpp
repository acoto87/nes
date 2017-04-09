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

    cartridge->prgBanks = header.prgROMSize;
    if (cartridge->prgBanks > 0)
    {
        cartridge->prgSizeInBytes = cartridge->prgBanks * CPU_PRG_BANK_SIZE;
        cartridge->prg = (u8*)Allocate(cartridge->prgSizeInBytes);
        fread(cartridge->prg, sizeof(u8), cartridge->prgSizeInBytes, file);
    }

    cartridge->chrBanks = header.chrROMSize;
    if (cartridge->chrBanks > 0)
    {
        cartridge->chrSizeInBytes = cartridge->chrBanks * CHR_BANK_SIZE;
        cartridge->chr = (u8*)Allocate(cartridge->chrSizeInBytes);
        fread(cartridge->chr, sizeof(u8), cartridge->chrSizeInBytes, file);
    }

    fread(cartridge->title, sizeof(u8), MAX_TITLE_LENGTH, file);

    fclose(file);

    return true;
}

internal void CreateMapper(NES *nes)
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
            nes->mapperSave = Mapper1Save;
            nes->mapperLoad = Mapper1Load;
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
}

void InitMapper(NES *nes)
{
    CreateMapper(nes);
    nes->mapperInit(nes);
}

NES* CreateNES(Cartridge cartridge)
{
    NES* nes = (NES*)Allocate(sizeof(NES));

    if (nes)
    {
        nes->cartridge = cartridge;

        InitCPU(nes);
        InitPPU(nes);
        //InitAPU(nes)
        InitGUI(nes);
        InitController(nes, 0);
        InitController(nes, 1);
        InitMapper(nes);

        if (cartridge.hasBatteryPack)
        {
            // load battery ram
        }

        PowerCPU(nes);
        PowerPPU(nes);
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

    if (nes->cartridge.prg)
    {
        Free(nes->cartridge.prg);
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

internal inline void SaveMemory(Memory *memory, FILE *file)
{
    fwrite(&memory->created, sizeof(b32), 1, file);
    fwrite(&memory->length, sizeof(u32), 1, file);
    fwrite(memory->bytes, sizeof(u8), memory->length, file);
}

void Save(NES *nes, char *filePath)
{
    FILE *file = fopen(filePath, "wb");

    // Write ram data
    SaveMemory(&nes->cpuMemory, file);
    SaveMemory(&nes->ppuMemory, file);
    SaveMemory(&nes->oamMemory, file);
    SaveMemory(&nes->oamMemory2, file);

    // Write CPU, PPU and APU data
    fwrite(&nes->cpu, sizeof(CPU), 1, file);
    fwrite(&nes->ppu, sizeof(PPU), 1, file);
    // fwrite(&nes->apu, sizeof(APU), 1, file);

    // Write cartridge data
    Cartridge *cartridge = &nes->cartridge;
    fwrite(&cartridge->mirrorType, sizeof(MirrorType), 1, file);
    fwrite(&cartridge->hasBatteryPack, sizeof(b32), 1, file);
    fwrite(&cartridge->mapper, sizeof(u8), 1, file);
    fwrite(&cartridge->prgRAMSize, sizeof(u8), 1, file);

    fwrite(cartridge->title, sizeof(u8), MAX_TITLE_LENGTH, file);

    fwrite(&cartridge->hasTrainer, sizeof(b32), 1, file);
    fwrite(cartridge->trainer, sizeof(u8), TRAINER_SIZE, file);

    fwrite(&cartridge->prgBanks, sizeof(u32), 1, file);
    fwrite(&cartridge->prgSizeInBytes, sizeof(u32), 1, file);
    fwrite(cartridge->prg, sizeof(u8), cartridge->prgSizeInBytes, file);

    fwrite(&cartridge->chrBanks, sizeof(u32), 1, file);
    fwrite(&cartridge->chrSizeInBytes, sizeof(u32), 1, file);
    fwrite(cartridge->chr, sizeof(u8), cartridge->chrSizeInBytes, file);

    // Write controllers data
    fwrite(&nes->controllers[0], sizeof(Controller), 1, file);
    fwrite(&nes->controllers[1], sizeof(Controller), 1, file);

    // Write mapper
    if (nes->mapperSave)
    {
        nes->mapperSave(nes, file);
    }
    
    // Write GUI data
    GUI *gui = &nes->gui;
    fwrite(&gui->width, sizeof(u32), 1, file);
    fwrite(&gui->height, sizeof(u32), 1, file);
    fwrite(gui->pixels, sizeof(Color), 256 * 240, file);

    for (s32 i = 0; i < 2; ++i)
    {
        fwrite(gui->patterns[i], sizeof(Color), 128 * 128, file);
    }

    fwrite(gui->patternHover, sizeof(Color), 8 * 8, file);

    for (s32 i = 0; i < 64; ++i)
    {
        fwrite(gui->sprites[i], sizeof(Color), 8 * 16, file);
    }

    for (s32 i = 0; i < 8; ++i)
    {
        fwrite(gui->sprites2[i], sizeof(Color), 8 * 16, file);
    }

    fwrite(gui->nametable, sizeof(Color), 256 * 240, file);

    for (s32 i = 0; i < 32; ++i)
    {
        for (s32 j = 0; j < 30; ++j)
        {
            fwrite(gui->nametable2[i][j], sizeof(Color), 64, file);
        }
    }

    fclose(file);
}

internal inline void ReadMemory(Memory *memory, FILE *file)
{
    fread(&memory->created, sizeof(b32), 1, file);
    fread(&memory->length, sizeof(u32), 1, file);
    
    memory->bytes = (u8*)Allocate(memory->length);
    fread(memory->bytes, sizeof(u8), memory->length, file);
}

NES* LoadNesSave(char *filePath)
{
    FILE *file = fopen(filePath, "rb");

    NES* nes = (NES*)Allocate(sizeof(NES));

    // Read ram data
    ReadMemory(&nes->cpuMemory, file);
    ReadMemory(&nes->ppuMemory, file);
    ReadMemory(&nes->oamMemory, file);
    ReadMemory(&nes->oamMemory2, file);

    // Read CPU, PPU and APU data
    fread(&nes->cpu, sizeof(CPU), 1, file);
    fread(&nes->ppu, sizeof(PPU), 1, file);
    // fread(&nes->apu, sizeof(APU), 1, file);

    // Read cartridge data
    Cartridge *cartridge = &nes->cartridge;
    fread(&cartridge->mirrorType, sizeof(MirrorType), 1, file);
    fread(&cartridge->hasBatteryPack, sizeof(b32), 1, file);
    fread(&cartridge->mapper, sizeof(u8), 1, file);
    fread(&cartridge->prgRAMSize, sizeof(u8), 1, file);

    fread(cartridge->title, sizeof(u8), MAX_TITLE_LENGTH, file);

    fread(&cartridge->hasTrainer, sizeof(b32), 1, file);
    fread(&cartridge->trainer, sizeof(u8), TRAINER_SIZE, file);

    fread(&cartridge->prgBanks, sizeof(u32), 1, file);
    fread(&cartridge->prgSizeInBytes, sizeof(u32), 1, file);

    cartridge->prg = (u8*)Allocate(cartridge->prgSizeInBytes);
    fread(cartridge->prg, sizeof(u8), cartridge->prgSizeInBytes, file);

    fread(&cartridge->chrBanks, sizeof(u32), 1, file);
    fread(&cartridge->chrSizeInBytes, sizeof(u32), 1, file);

    cartridge->chr = (u8*)Allocate(cartridge->chrSizeInBytes);
    fread(cartridge->chr, sizeof(u8), cartridge->chrSizeInBytes, file);

    // Read controllers data
    fread(&nes->controllers[0], sizeof(Controller), 1, file);
    fread(&nes->controllers[1], sizeof(Controller), 1, file);

    // Read mapper
    CreateMapper(nes);
    if (nes->mapperLoad)
    {
        nes->mapperLoad(nes, file);
    }

    // Read GUI data
    GUI *gui = &nes->gui;
    fread(&gui->width, sizeof(u32), 1, file);
    fread(&gui->height, sizeof(u32), 1, file);
    fread(gui->pixels, sizeof(Color), 256 * 240, file);

    for (s32 i = 0; i < 2; ++i)
    {
        fread(gui->patterns[i], sizeof(Color), 128 * 128, file);
    }

    fread(gui->patternHover, sizeof(Color), 8 * 8, file);

    for (s32 i = 0; i < 64; ++i)
    {
        fread(gui->sprites[i], sizeof(Color), 8 * 16, file);
    }

    for (s32 i = 0; i < 8; ++i)
    {
        fread(gui->sprites2[i], sizeof(Color), 8 * 16, file);
    }

    fread(gui->nametable, sizeof(Color), 256 * 240, file);

    for (s32 i = 0; i < 32; ++i)
    {
        for (s32 j = 0; j < 30; ++j)
        {
            fread(gui->nametable2[i][j], sizeof(Color), 64, file);
        }
    }

    fclose(file);

    return nes;
}

