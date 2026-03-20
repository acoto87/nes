#ifndef CPU_DEBUG_H
#define CPU_DEBUG_H

#include "types.h"

#define CPU_INSTRUCTIONS_COUNT 0x100

extern CPUInstruction cpuInstructions[CPU_INSTRUCTIONS_COUNT];
const char* GetInstructionStr(CPUInstructionMnemonic instruction);
const char* GetRegisterStr(CPURegister cpuRegister);

#endif
