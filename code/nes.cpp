#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "nes.h"
#include "cpu.h"
#include "ppu.h"
#include "controller.h"
#include "gui.h"

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

        if (cartridge.hasBatteryPack)
        {
            // load battery ram
        }
    }

    return nes;
}

