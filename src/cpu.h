#ifndef CPU_H
#define CPU_H

#include "types.h"
#include "memory.h"
#include "oam.h"
#include "ppu.h"
#include "apu.h"
#include "controller.h"
#include "cpu_debug.h"

#define CPU_RAM_TOTAL_SIZE KILOBYTES(64)

#define CPU_RESET_ADDRESS 0xFFFC
#define CPU_IRQ_ADDRESS 0xFFFE
#define CPU_NMI_ADDRESS 0xFFFA

#define CPU_STATUS_INITIAL_VALUE 0x24
#define CPU_STACK_PTR_INITIAL_VALUE 0xFF

#define CPU_PRG_BANK_0_OFFSET 0x8000
#define CPU_PRG_BANK_1_OFFSET 0xC000
#define CPU_PRG_BANK_SIZE KILOBYTES(16)

#define CPU_RAM_OFFSET 0x0000
#define CPU_RAM_SIZE 0x0800

#define CPU_RAM_MIRROR_OFFSET 0x0800
#define CPU_RAM_MIRROR_SIZE 0x1800

#define CPU_PPU_OFFSET 0x2000
#define CPU_PPU_SIZE 0x0008

#define CPU_PPU_MIRROR_OFFSET 0x2008
#define CPU_PPU_MIRROR_SIZE 0x1FF8

#define CPU_PPU_CONTROL_REGISTER_ADDRESS 0x2000
#define CPU_PPU_MASK_REGISTER_ADDRESS 0x2001
#define CPU_PPU_STATUS_REGISTER_ADDRESS 0x2002

// internal object attribute memory index pointer (64 attributes, 32 bits // each, byte granular access).
// stored value post-increments on access to port 4.
#define CPU_PPU_SPRITE_ADDRESS_REGISTER_ADDRESS 0x2003

// returns object attribute memory location indexed by port 3, then increments port 3.
#define CPU_PPU_SPRITE_DATA_REGISTER_ADDRESS 0x2004

// scroll offset port
#define CPU_PPU_SCROLL_REGISTER_ADDRESS 0x2005

// PPU address port to access with port 7
#define CPU_PPU_MEMORY_ADDRESS_REGISTER_ADDRESS 0x2006

// PPU memory write port
#define CPU_PPU_MEMORY_DATA_REGISTER_ADDRESS 0x2007

#define CPU_IO_OFFSET 0x4000
#define CPU_IO_SIZE 0x0020

#define CPU_SPRITE_DMA_REGISTER_ADDRESS 0x4014
#define CPU_DMA_ADDRESS_MULTIPLIER 0x100;

#define CPU_GAMEPAD_0_ADDRESS 0x4016
#define CPU_GAMEPAD_1_ADDRESS 0x4017

#define CPU_SRAM_OFFSET 0x6000
#define CPU_SRAM_SIZE 0x2000

#define CARRY_FLAG 0
#define ZERO_FLAG 1
#define INTERRUPT_FLAG 2
#define DECIMAL_FLAG 3
#define BREAK_FLAG 4
#define EMPTY_FLAG 5
#define OVERFLOW_FLAG 6
#define NEGATIVE_FLAG 7

// CPU_FREQ is defined in apu.h (included above) so that apu.c can use it
// without a circular dependency. It is available here because cpu.h includes apu.h.

u8 ReadCPUU8(NES* nes, u16 address);
u8 PeekCPUU8(NES* nes, u16 address);

static inline u16 ReadCPUU16(NES* nes, u16 address)
{
    // read in little-endian mode
    u8 lo = ReadCPUU8(nes, address);
    u8 hi = ReadCPUU8(nes, address + 1);
    return (hi << 8) | lo;
}

void WriteCPUU8(NES* nes, u16 address, u8 value);

static inline void WriteCPUU16(NES* nes, u16 address, u16 value)
{
    // write in little-endian mode
    u8 lo = value & 0xFF;
    u8 hi = (value & 0xFF00) >> 8;
    WriteCPUU8(nes, address, lo);
    WriteCPUU8(nes, address + 1, hi);
}

static inline void PushStackU8(NES* nes, u8 value)
{
    CPU* cpu = &nes->cpu;
    u8 sp = cpu->sp;
    WriteCPUU8(nes, sp + 0x100, value);
    cpu->sp = sp > 0 ? sp - 1 : 0xFF;
}

static inline void PushStackU16(NES* nes, u16 value)
{
    u8 lo = value & 0xFF;
    u8 hi = (value & 0xFF00) >> 8;
    PushStackU8(nes, hi);
    PushStackU8(nes, lo);
}

static inline u8 PopStackU8(NES* nes)
{
    CPU* cpu = &nes->cpu;
    u8 sp = cpu->sp + 1;
    u8 v = ReadCPUU8(nes, sp + 0x100);
    cpu->sp = sp;
    return v;
}

static inline u16 PopStackU16(NES* nes)
{
    u8 lo = PopStackU8(nes);
    u8 hi = PopStackU8(nes);
    return ((u16)hi << 8) | (u16)lo;
}

#define GetCarry(cpu) GetBitFlag(cpu->p, CARRY_FLAG)
#define SetCarry(cpu, v)                                                                                               \
    if (v) SetBitFlag(&cpu->p, CARRY_FLAG);                                                                            \
    else ClearBitFlag(&cpu->p, CARRY_FLAG);

#define GetZero(cpu) GetBitFlag(cpu->p, ZERO_FLAG)
#define SetZero(cpu, v)                                                                                                \
    if (v) SetBitFlag(&cpu->p, ZERO_FLAG);                                                                             \
    else ClearBitFlag(&cpu->p, ZERO_FLAG);

#define GetInterrupt(cpu) GetBitFlag(cpu->p, INTERRUPT_FLAG)
#define SetInterrupt(cpu, v)                                                                                           \
    if (v) SetBitFlag(&cpu->p, INTERRUPT_FLAG);                                                                        \
    else ClearBitFlag(&cpu->p, INTERRUPT_FLAG);

#define GetDecimal(cpu) GetBitFlag(cpu->p, DECIMAL_FLAG)
#define SetDecimal(cpu, v)                                                                                             \
    if (v) SetBitFlag(&cpu->p, DECIMAL_FLAG);                                                                          \
    else ClearBitFlag(&cpu->p, DECIMAL_FLAG);

#define GetBreak(cpu) GetBitFlag(cpu->p, BREAK_FLAG)
#define SetBreak(cpu, v)                                                                                               \
    if (v) SetBitFlag(&cpu->p, BREAK_FLAG);                                                                            \
    else ClearBitFlag(&cpu->p, BREAK_FLAG);

#define GetOverflow(cpu) GetBitFlag(cpu->p, OVERFLOW_FLAG)
#define SetOverflow(cpu, v)                                                                                            \
    if (v) SetBitFlag(&cpu->p, OVERFLOW_FLAG);                                                                         \
    else ClearBitFlag(&cpu->p, OVERFLOW_FLAG);

#define GetNegative(cpu) GetBitFlag(cpu->p, NEGATIVE_FLAG)
#define SetNegative(cpu, v)                                                                                            \
    if (v) SetBitFlag(&cpu->p, NEGATIVE_FLAG);                                                                         \
    else ClearBitFlag(&cpu->p, NEGATIVE_FLAG);

void ResetCPU(NES* nes);
void PowerCPU(NES* nes);
void InitCPU(NES* nes);
CPUStep StepCPU(NES* nes);

void CPURequestReset(NES* nes);
void CPUSetNMILine(NES* nes, bool asserted);
void CPUSetIRQSource(NES* nes, u32 sourceMask, bool asserted);

static inline void StepCPUCycles(NES* nes, s32 cycles)
{
    StepPPUCycles(nes, cycles);
    StepAPUCycles(nes, cycles);
}

#endif // CPU_H
