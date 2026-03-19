#ifndef HEADLESS_H
#define HEADLESS_H

#include "types.h"
#include <stdbool.h>

typedef struct HeadlessConfig {
    const char* romPath;
    const char* logPath;
    u16 startPC;
    u64 maxInstructions;
    u64 maxCycles;
    bool logCPU;
} HeadlessConfig;

int RunHeadless(HeadlessConfig* config);

#endif // HEADLESS_H
