#include <stdio.h>
#include <stdlib.h>
#include "headless.h"
#include "nes.h"
#include "cpu.h"
#include "cpu_trace.h"

#undef nes

int RunHeadless(HeadlessConfig* config)
{
    if (!config->romPath) {
        fprintf(stderr, "Error: No ROM path specified for headless mode.\n");
        return 1;
    }

    Cartridge cartridge = {0};
    if (!LoadNesRom((char*)config->romPath, &cartridge)) {
        fprintf(stderr, "Error: Could not load ROM: %s\n", config->romPath);
        return 1;
    }

    NES* nes = CreateNES(cartridge);
    if (!nes) {
        fprintf(stderr, "Error: Unsupported mapper or failed to create NES.\n");
        return 1;
    }

    if (config->startPC != 0) {
        nes->cpu.pc = config->startPC;
    }

    FILE* logFile = NULL;
    if (config->logCPU && config->logPath) {
        logFile = fopen(config->logPath, "w");
        if (!logFile) {
            fprintf(stderr, "Error: Could not open log file: %s\n", config->logPath);
            Destroy(nes);
            return 1;
        }
    }

    u64 instructionsRun = 0;
    while (true) {
        if (config->maxInstructions > 0 && instructionsRun >= config->maxInstructions) {
            break;
        }
        if (config->maxCycles > 0 && nes->cpu.cycles >= config->maxCycles) {
            break;
        }

        if (logFile) {
            LogCPUState(nes, logFile);
        }

        StepCPU(nes);
        instructionsRun++;
    }

    if (logFile) {
        fclose(logFile);
    }
    Destroy(nes);

    return 0;
}
