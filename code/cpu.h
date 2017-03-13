#pragma once
#ifndef CPU_H
#define CPU_H

#include "types.h"
#include "memory.h"
#include "oam.h"
#include "ppu.h"
#include "controller.h"

#define CPU_RAM_TOTAL_SIZE KILOBYTES(64)

#define CPU_INSTRUCTIONS_COUNT 0x100

#define CPU_RESET_ADDRESS 0xFFFC 
#define CPU_IRQ_ADDRESS 0xFFFE 
#define CPU_NMI_ADDRESS 0xFFFA 

#define CPU_STATUS_INITIAL_VALUE 0x34 // or 0x24
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

#define PAGE_CROSS(x, y) (((x) & 0xFF00) != ((y) & 0xFF00));

#define CARRY_FLAG 0
#define ZERO_FLAG 1
#define INTERRUPT_FLAG 2
#define DECIMAL_FLAG 3
#define BREAK_FLAG 4
#define EMPTY_FLAG 5
#define OVERFLOW_FLAG 6
#define NEGATIVE_FLAG 7

// The NES CPU operates at a speed of 21.477272 MHz / 12 = 1.789773 MHz = 1789773Hz,
// that's is ~1789773 cycles per second.
// see https://wiki.nesdev.com/w/index.php/Clock_rate
#define CPU_FREQ 1789773

global CPUInstruction cpuInstructions[CPU_INSTRUCTIONS_COUNT] =
{
    { 0x00, CPU_BRK, AM_IMM, CPU_NR, 2, 7, 0 },
    { 0x01, CPU_ORA, AM_IZX, CPU_XR, 2, 6, 0 },
    { 0x02, CPU_KIL, AM_NON, CPU_NR, 0, 0, 0 },
    { 0x03, CPU_SLO, AM_IZX, CPU_XR, 2, 8, 0 },
    { 0x04, CPU_NOP, AM_ZPA, CPU_NR, 2, 3, 0 },
    { 0x05, CPU_ORA, AM_ZPA, CPU_NR, 2, 3, 0 },
    { 0x06, CPU_ASL, AM_ZPA, CPU_NR, 2, 5, 0 },
    { 0x07, CPU_SLO, AM_ZPA, CPU_NR, 2, 5, 0 },
    { 0x08, CPU_PHP, AM_IMP, CPU_NR, 1, 3, 0 },
    { 0x09, CPU_ORA, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0x0A, CPU_ASL, AM_ACC, CPU_NR, 1, 2, 0 },
    { 0x0B, CPU_ANC, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0x0C, CPU_NOP, AM_ABS, CPU_NR, 3, 4, 0 },
    { 0x0D, CPU_ORA, AM_ABS, CPU_NR, 3, 4, 0 },
    { 0x0E, CPU_ASL, AM_ABS, CPU_NR, 3, 6, 0 },
    { 0x0F, CPU_SLO, AM_ABS, CPU_NR, 3, 6, 0 },
    { 0x10, CPU_BPL, AM_REL, CPU_NR, 2, 2, 1 },
    { 0x11, CPU_ORA, AM_IZY, CPU_YR, 2, 5, 1 },
    { 0x12, CPU_KIL, AM_NON, CPU_NR, 0, 0, 0 },
    { 0x13, CPU_SLO, AM_IZY, CPU_YR, 2, 8, 0 },
    { 0x14, CPU_NOP, AM_ZPX, CPU_XR, 2, 4, 0 },
    { 0x15, CPU_ORA, AM_ZPX, CPU_XR, 2, 4, 0 },
    { 0x16, CPU_ASL, AM_ZPX, CPU_XR, 2, 6, 0 },
    { 0x17, CPU_SLO, AM_ZPX, CPU_XR, 2, 6, 0 },
    { 0x18, CPU_CLC, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0x19, CPU_ORA, AM_ABY, CPU_YR, 3, 4, 1 },
    { 0x1A, CPU_NOP, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0x1B, CPU_SLO, AM_ABY, CPU_YR, 3, 7, 0 },
    { 0x1C, CPU_NOP, AM_ABX, CPU_XR, 3, 4, 1 },
    { 0x1D, CPU_ORA, AM_ABX, CPU_XR, 3, 4, 1 },
    { 0x1E, CPU_ASL, AM_ABX, CPU_XR, 3, 7, 0 },
    { 0x1F, CPU_SLO, AM_ABX, CPU_XR, 3, 7, 0 },
    { 0x20, CPU_JSR, AM_ABS, CPU_NR, 3, 6, 0 },
    { 0x21, CPU_AND, AM_IZX, CPU_XR, 2, 6, 0 },
    { 0x22, CPU_KIL, AM_NON, CPU_NR, 0, 0, 0 },
    { 0x23, CPU_RLA, AM_IZX, CPU_XR, 2, 8, 0 },
    { 0x24, CPU_BIT, AM_ZPA, CPU_NR, 2, 3, 0 },
    { 0x25, CPU_AND, AM_ZPA, CPU_NR, 2, 3, 0 },
    { 0x26, CPU_ROL, AM_ZPA, CPU_NR, 2, 5, 0 },
    { 0x27, CPU_RLA, AM_ZPA, CPU_NR, 2, 5, 0 },
    { 0x28, CPU_PLP, AM_IMP, CPU_NR, 1, 4, 0 },
    { 0x29, CPU_AND, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0x2A, CPU_ROL, AM_ACC, CPU_NR, 1, 2, 0 },
    { 0x2B, CPU_ANC, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0x2C, CPU_BIT, AM_ABS, CPU_NR, 3, 4, 0 },
    { 0x2D, CPU_AND, AM_ABS, CPU_NR, 3, 4, 0 },
    { 0x2E, CPU_ROL, AM_ABS, CPU_NR, 3, 6, 0 },
    { 0x2F, CPU_RLA, AM_ABS, CPU_NR, 3, 6, 0 },
    { 0x30, CPU_BMI, AM_REL, CPU_NR, 2, 2, 1 },
    { 0x31, CPU_AND, AM_IZY, CPU_YR, 2, 5, 1 },
    { 0x32, CPU_KIL, AM_NON, CPU_NR, 0, 0, 0 },
    { 0x33, CPU_RLA, AM_IZY, CPU_YR, 2, 8, 0 },
    { 0x34, CPU_NOP, AM_ZPX, CPU_XR, 2, 4, 0 },
    { 0x35, CPU_AND, AM_ZPX, CPU_XR, 2, 4, 0 },
    { 0x36, CPU_ROL, AM_ZPX, CPU_XR, 2, 6, 0 },
    { 0x37, CPU_RLA, AM_ZPX, CPU_XR, 2, 6, 0 },
    { 0x38, CPU_SEC, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0x39, CPU_AND, AM_ABY, CPU_YR, 3, 4, 1 },
    { 0x3A, CPU_NOP, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0x3B, CPU_RLA, AM_ABY, CPU_YR, 3, 7, 0 },
    { 0x3C, CPU_NOP, AM_ABX, CPU_XR, 3, 4, 1 },
    { 0x3D, CPU_AND, AM_ABX, CPU_XR, 3, 4, 1 },
    { 0x3E, CPU_ROL, AM_ABX, CPU_XR, 3, 7, 0 },
    { 0x3F, CPU_RLA, AM_ABX, CPU_XR, 3, 7, 0 },
    { 0x40, CPU_RTI, AM_IMP, CPU_NR, 1, 6, 0 },
    { 0x41, CPU_EOR, AM_IZX, CPU_XR, 2, 6, 0 },
    { 0x42, CPU_KIL, AM_NON, CPU_NR, 0, 0, 0 },
    { 0x43, CPU_SRE, AM_IZX, CPU_XR, 2, 8, 0 },
    { 0x44, CPU_NOP, AM_ZPA, CPU_NR, 2, 3, 0 },
    { 0x45, CPU_EOR, AM_ZPA, CPU_NR, 2, 3, 0 },
    { 0x46, CPU_LSR, AM_ZPA, CPU_NR, 2, 5, 0 },
    { 0x47, CPU_SRE, AM_ZPA, CPU_NR, 2, 5, 0 },
    { 0x48, CPU_PHA, AM_IMP, CPU_NR, 1, 3, 0 },
    { 0x49, CPU_EOR, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0x4A, CPU_LSR, AM_ACC, CPU_NR, 1, 2, 0 },
    { 0x4B, CPU_ALR, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0x4C, CPU_JMP, AM_ABS, CPU_NR, 3, 3, 0 },
    { 0x4D, CPU_EOR, AM_ABS, CPU_NR, 3, 4, 0 },
    { 0x4E, CPU_LSR, AM_ABS, CPU_NR, 3, 6, 0 },
    { 0x4F, CPU_SRE, AM_ABS, CPU_NR, 3, 6, 0 },
    { 0x50, CPU_BVC, AM_REL, CPU_NR, 2, 2, 1 },
    { 0x51, CPU_EOR, AM_IZY, CPU_YR, 2, 5, 1 },
    { 0x52, CPU_KIL, AM_NON, CPU_NR, 0, 0, 0 },
    { 0x53, CPU_SRE, AM_IZY, CPU_YR, 2, 8, 0 },
    { 0x54, CPU_NOP, AM_ZPX, CPU_XR, 2, 4, 0 },
    { 0x55, CPU_EOR, AM_ZPX, CPU_XR, 2, 4, 0 },
    { 0x56, CPU_LSR, AM_ZPX, CPU_XR, 2, 6, 0 },
    { 0x57, CPU_SRE, AM_ZPX, CPU_XR, 2, 6, 0 },
    { 0x58, CPU_CLI, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0x59, CPU_EOR, AM_ABY, CPU_YR, 3, 4, 1 },
    { 0x5A, CPU_NOP, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0x5B, CPU_SRE, AM_ABY, CPU_YR, 3, 7, 0 },
    { 0x5C, CPU_NOP, AM_ABX, CPU_XR, 3, 4, 1 },
    { 0x5D, CPU_EOR, AM_ABX, CPU_XR, 3, 4, 1 },
    { 0x5E, CPU_LSR, AM_ABX, CPU_XR, 3, 7, 0 },
    { 0x5F, CPU_SRE, AM_ABX, CPU_XR, 3, 7, 0 },
    { 0x60, CPU_RTS, AM_IMP, CPU_NR, 1, 6, 0 },
    { 0x61, CPU_ADC, AM_IZX, CPU_XR, 2, 6, 0 },
    { 0x62, CPU_KIL, AM_NON, CPU_NR, 0, 0, 0 },
    { 0x63, CPU_RRA, AM_IZX, CPU_XR, 2, 8, 0 },
    { 0x64, CPU_NOP, AM_ZPA, CPU_NR, 2, 3, 0 },
    { 0x65, CPU_ADC, AM_ZPA, CPU_NR, 2, 3, 0 },
    { 0x66, CPU_ROR, AM_ZPA, CPU_NR, 2, 5, 0 },
    { 0x67, CPU_RRA, AM_ZPA, CPU_NR, 2, 5, 0 },
    { 0x68, CPU_PLA, AM_IMP, CPU_NR, 1, 4, 0 },
    { 0x69, CPU_ADC, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0x6A, CPU_ROR, AM_ACC, CPU_NR, 1, 2, 0 },
    { 0x6B, CPU_ARR, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0x6C, CPU_JMP, AM_IND, CPU_NR, 3, 5, 0 },
    { 0x6D, CPU_ADC, AM_ABS, CPU_NR, 3, 4, 0 },
    { 0x6E, CPU_ROR, AM_ABS, CPU_NR, 3, 6, 0 },
    { 0x6F, CPU_RRA, AM_ABS, CPU_NR, 3, 6, 0 },
    { 0x70, CPU_BVS, AM_REL, CPU_NR, 2, 2, 1 },
    { 0x71, CPU_ADC, AM_IZY, CPU_YR, 2, 5, 1 },
    { 0x72, CPU_KIL, AM_NON, CPU_NR, 0, 0, 0 },
    { 0x73, CPU_RRA, AM_IZY, CPU_YR, 2, 8, 0 },
    { 0x74, CPU_NOP, AM_ZPX, CPU_XR, 2, 4, 0 },
    { 0x75, CPU_ADC, AM_ZPX, CPU_XR, 2, 4, 0 },
    { 0x76, CPU_ROR, AM_ZPX, CPU_XR, 2, 6, 0 },
    { 0x77, CPU_RRA, AM_ZPX, CPU_XR, 2, 6, 0 },
    { 0x78, CPU_SEI, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0x79, CPU_ADC, AM_ABY, CPU_YR, 3, 4, 1 },
    { 0x7A, CPU_NOP, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0x7B, CPU_RRA, AM_ABY, CPU_YR, 3, 7, 0 },
    { 0x7C, CPU_NOP, AM_ABX, CPU_XR, 3, 4, 1 },
    { 0x7D, CPU_ADC, AM_ABX, CPU_XR, 3, 4, 1 },
    { 0x7E, CPU_ROR, AM_ABX, CPU_XR, 3, 7, 0 },
    { 0x7F, CPU_RRA, AM_ABX, CPU_XR, 3, 7, 0 },
    { 0x80, CPU_NOP, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0x81, CPU_STA, AM_IZX, CPU_XR, 2, 6, 0 },
    { 0x82, CPU_NOP, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0x83, CPU_SAX, AM_IZX, CPU_XR, 2, 6, 0 },
    { 0x84, CPU_STY, AM_ZPA, CPU_NR, 2, 3, 0 },
    { 0x85, CPU_STA, AM_ZPA, CPU_NR, 2, 3, 0 },
    { 0x86, CPU_STX, AM_ZPA, CPU_NR, 2, 3, 0 },
    { 0x87, CPU_SAX, AM_ZPA, CPU_NR, 2, 3, 0 },
    { 0x88, CPU_DEY, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0x89, CPU_NOP, AM_IMM, CPU_NR, 1, 2, 0 },
    { 0x8A, CPU_TXA, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0x8B, CPU_XAA, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0x8C, CPU_STY, AM_ABS, CPU_NR, 3, 4, 0 },
    { 0x8D, CPU_STA, AM_ABS, CPU_NR, 3, 4, 0 },
    { 0x8E, CPU_STX, AM_ABS, CPU_NR, 3, 4, 0 },
    { 0x8F, CPU_SAX, AM_ABS, CPU_NR, 3, 4, 0 },
    { 0x90, CPU_BCC, AM_REL, CPU_NR, 2, 2, 1 },
    { 0x91, CPU_STA, AM_IZY, CPU_YR, 2, 6, 0 },
    { 0x92, CPU_KIL, AM_NON, CPU_NR, 0, 0, 0 },
    { 0x93, CPU_AHX, AM_IZY, CPU_YR, 2, 6, 0 },
    { 0x94, CPU_STY, AM_ZPX, CPU_XR, 2, 4, 0 },
    { 0x95, CPU_STA, AM_ZPX, CPU_XR, 2, 4, 0 },
    { 0x96, CPU_STX, AM_ZPY, CPU_YR, 2, 4, 0 },
    { 0x97, CPU_SAX, AM_ZPY, CPU_YR, 2, 4, 0 },
    { 0x98, CPU_TYA, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0x99, CPU_STA, AM_ABY, CPU_YR, 3, 5, 0 },
    { 0x9A, CPU_TXS, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0x9B, CPU_TAS, AM_ABY, CPU_YR, 3, 5, 0 },
    { 0x9C, CPU_SHY, AM_ABX, CPU_XR, 3, 5, 0 },
    { 0x9D, CPU_STA, AM_ABX, CPU_XR, 3, 5, 0 },
    { 0x9E, CPU_SHX, AM_ABY, CPU_YR, 3, 5, 0 },
    { 0x9F, CPU_AHX, AM_ABY, CPU_YR, 3, 5, 0 },
    { 0xA0, CPU_LDY, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0xA1, CPU_LDA, AM_IZX, CPU_XR, 2, 6, 0 },
    { 0xA2, CPU_LDX, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0xA3, CPU_LAX, AM_IZX, CPU_XR, 2, 6, 0 },
    { 0xA4, CPU_LDY, AM_ZPA, CPU_NR, 2, 3, 0 },
    { 0xA5, CPU_LDA, AM_ZPA, CPU_NR, 2, 3, 0 },
    { 0xA6, CPU_LDX, AM_ZPA, CPU_NR, 2, 3, 0 },
    { 0xA7, CPU_LAX, AM_ZPA, CPU_NR, 2, 3, 0 },
    { 0xA8, CPU_TAY, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0xA9, CPU_LDA, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0xAA, CPU_TAX, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0xAB, CPU_LAX, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0xAC, CPU_LDY, AM_ABS, CPU_NR, 3, 4, 0 },
    { 0xAD, CPU_LDA, AM_ABS, CPU_NR, 3, 4, 0 },
    { 0xAE, CPU_LDX, AM_ABS, CPU_NR, 3, 4, 0 },
    { 0xAF, CPU_LAX, AM_ABS, CPU_NR, 3, 4, 0 },
    { 0xB0, CPU_BCS, AM_REL, CPU_NR, 2, 2, 1 },
    { 0xB1, CPU_LDA, AM_IZY, CPU_YR, 2, 5, 1 },
    { 0xB2, CPU_KIL, AM_NON, CPU_NR, 0, 0, 0 },
    { 0xB3, CPU_LAX, AM_IZY, CPU_YR, 2, 5, 0 },
    { 0xB4, CPU_LDY, AM_ZPX, CPU_XR, 2, 4, 0 },
    { 0xB5, CPU_LDA, AM_ZPX, CPU_XR, 2, 4, 0 },
    { 0xB6, CPU_LDX, AM_ZPY, CPU_YR, 2, 4, 0 },
    { 0xB7, CPU_LAX, AM_ZPY, CPU_YR, 2, 4, 0 },
    { 0xB8, CPU_CLV, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0xB9, CPU_LDA, AM_ABY, CPU_YR, 3, 4, 1 },
    { 0xBA, CPU_TSX, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0xBB, CPU_LAS, AM_ABY, CPU_YR, 3, 4, 1 },
    { 0xBC, CPU_LDY, AM_ABX, CPU_XR, 3, 4, 1 },
    { 0xBD, CPU_LDA, AM_ABX, CPU_XR, 3, 4, 1 },
    { 0xBE, CPU_LDX, AM_ABY, CPU_YR, 3, 4, 1 },
    { 0xBF, CPU_LAX, AM_ABY, CPU_YR, 3, 4, 1 },
    { 0xC0, CPU_CPY, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0xC1, CPU_CMP, AM_IZX, CPU_XR, 2, 6, 0 },
    { 0xC2, CPU_NOP, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0xC3, CPU_DCP, AM_IZX, CPU_XR, 2, 8, 0 },
    { 0xC4, CPU_CPY, AM_ZPA, CPU_NR, 2, 3, 0 },
    { 0xC5, CPU_CMP, AM_ZPA, CPU_NR, 2, 3, 0 },
    { 0xC6, CPU_DEC, AM_ZPA, CPU_NR, 2, 5, 0 },
    { 0xC7, CPU_DCP, AM_ZPA, CPU_NR, 2, 5, 0 },
    { 0xC8, CPU_INY, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0xC9, CPU_CMP, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0xCA, CPU_DEX, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0xCB, CPU_AXS, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0xCC, CPU_CPY, AM_ABS, CPU_NR, 3, 4, 0 },
    { 0xCD, CPU_CMP, AM_ABS, CPU_NR, 3, 4, 0 },
    { 0xCE, CPU_DEC, AM_ABS, CPU_NR, 3, 6, 0 },
    { 0xCF, CPU_DCP, AM_ABS, CPU_NR, 3, 6, 0 },
    { 0xD0, CPU_BNE, AM_REL, CPU_NR, 2, 2, 1 },
    { 0xD1, CPU_CMP, AM_IZY, CPU_YR, 2, 5, 1 },
    { 0xD2, CPU_KIL, AM_NON, CPU_NR, 0, 0, 0 },
    { 0xD3, CPU_DCP, AM_IZY, CPU_YR, 2, 8, 0 },
    { 0xD4, CPU_NOP, AM_ZPX, CPU_XR, 2, 4, 0 },
    { 0xD5, CPU_CMP, AM_ZPX, CPU_XR, 2, 4, 0 },
    { 0xD6, CPU_DEC, AM_ZPX, CPU_XR, 2, 6, 0 },
    { 0xD7, CPU_DCP, AM_ZPX, CPU_XR, 2, 6, 0 },
    { 0xD8, CPU_CLD, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0xD9, CPU_CMP, AM_ABY, CPU_YR, 3, 4, 1 },
    { 0xDA, CPU_NOP, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0xDB, CPU_DCP, AM_ABY, CPU_YR, 3, 7, 0 },
    { 0xDC, CPU_NOP, AM_ABX, CPU_XR, 3, 4, 1 },
    { 0xDD, CPU_CMP, AM_ABX, CPU_XR, 3, 4, 1 },
    { 0xDE, CPU_DEC, AM_ABX, CPU_XR, 3, 7, 0 },
    { 0xDF, CPU_DCP, AM_ABX, CPU_XR, 3, 7, 0 },
    { 0xE0, CPU_CPX, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0xE1, CPU_SBC, AM_IZX, CPU_XR, 2, 6, 0 },
    { 0xE2, CPU_NOP, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0xE3, CPU_ISC, AM_IZX, CPU_XR, 2, 8, 0 },
    { 0xE4, CPU_CPX, AM_ZPA, CPU_NR, 2, 3, 0 },
    { 0xE5, CPU_SBC, AM_ZPA, CPU_NR, 2, 3, 0 },
    { 0xE6, CPU_INC, AM_ZPA, CPU_NR, 2, 5, 0 },
    { 0xE7, CPU_ISC, AM_ZPA, CPU_NR, 2, 5, 0 },
    { 0xE8, CPU_INX, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0xE9, CPU_SBC, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0xEA, CPU_NOP, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0xEB, CPU_SBC, AM_IMM, CPU_NR, 2, 2, 0 },
    { 0xEC, CPU_CPX, AM_ABS, CPU_NR, 3, 4, 0 },
    { 0xED, CPU_SBC, AM_ABS, CPU_NR, 3, 4, 0 },
    { 0xEE, CPU_INC, AM_ABS, CPU_NR, 3, 6, 0 },
    { 0xEF, CPU_ISC, AM_ABS, CPU_NR, 3, 6, 0 },
    { 0xF0, CPU_BEQ, AM_REL, CPU_NR, 2, 2, 1 },
    { 0xF1, CPU_SBC, AM_IZY, CPU_YR, 2, 5, 1 },
    { 0xF2, CPU_KIL, AM_NON, CPU_NR, 0, 0, 0 },
    { 0xF3, CPU_ISC, AM_IZY, CPU_YR, 2, 8, 0 },
    { 0xF4, CPU_NOP, AM_ZPX, CPU_XR, 2, 4, 0 },
    { 0xF5, CPU_SBC, AM_ZPX, CPU_XR, 2, 4, 0 },
    { 0xF6, CPU_INC, AM_ZPX, CPU_XR, 2, 6, 0 },
    { 0xF7, CPU_ISC, AM_ZPX, CPU_XR, 2, 6, 0 },
    { 0xF8, CPU_SED, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0xF9, CPU_SBC, AM_ABY, CPU_YR, 3, 4, 1 },
    { 0xFA, CPU_NOP, AM_IMP, CPU_NR, 1, 2, 0 },
    { 0xFB, CPU_ISC, AM_ABY, CPU_YR, 3, 7, 0 },
    { 0xFC, CPU_NOP, AM_ABX, CPU_XR, 3, 4, 1 },
    { 0xFD, CPU_SBC, AM_ABX, CPU_XR, 3, 4, 1 },
    { 0xFE, CPU_INC, AM_ABX, CPU_XR, 3, 7, 0 },
    { 0xFF, CPU_ISC, AM_ABX, CPU_XR, 3, 7, 0 }
};

internal inline char* GetInstructionStr(CPUInstructionSet instruction)
{
    switch (instruction)
    {
        case CPU_ADC: return "ADC";
        case CPU_AND: return "AND";
        case CPU_ASL: return "ASL";
        case CPU_BCC: return "BCC";
        case CPU_BCS: return "BCS";
        case CPU_BEQ: return "BEQ";
        case CPU_BIT: return "BIT";
        case CPU_BMI: return "BMI";
        case CPU_BNE: return "BNE";
        case CPU_BPL: return "BPL";
        case CPU_BRK: return "BRK";
        case CPU_BVC: return "BVC";
        case CPU_BVS: return "BVS";
        case CPU_CLC: return "CLC";
        case CPU_CLD: return "CLD";
        case CPU_CLI: return "CLI";
        case CPU_CLV: return "CLV";
        case CPU_CMP: return "CMP";
        case CPU_CPX: return "CPX";
        case CPU_CPY: return "CPY";
        case CPU_DEC: return "DEC";
        case CPU_DEX: return "DEX";
        case CPU_DEY: return "DEY";
        case CPU_EOR: return "EOR";
        case CPU_INC: return "INC";
        case CPU_INX: return "INX";
        case CPU_INY: return "INY";
        case CPU_JMP: return "JMP";
        case CPU_JSR: return "JSR";
        case CPU_LDA: return "LDA";
        case CPU_LDX: return "LDX";
        case CPU_LDY: return "LDY";
        case CPU_LSR: return "LSR";
        case CPU_NOP: return "NOP";
        case CPU_ORA: return "ORA";
        case CPU_PHA: return "PHA";
        case CPU_PHP: return "PHP";
        case CPU_PLA: return "PLA";
        case CPU_PLP: return "PLP";
        case CPU_ROL: return "ROL";
        case CPU_ROR: return "ROR";
        case CPU_RTI: return "RTI";
        case CPU_RTS: return "RTS";
        case CPU_SBC: return "SBC";
        case CPU_SEC: return "SEC";
        case CPU_SED: return "SED";
        case CPU_SEI: return "SEI";
        case CPU_STA: return "STA";
        case CPU_STX: return "STX";
        case CPU_STY: return "STY";
        case CPU_TAX: return "TAX";
        case CPU_TAY: return "TAY";
        case CPU_TSX: return "TSX";
        case CPU_TXA: return "TXA";
        case CPU_TXS: return "TXS";
        case CPU_TYA: return "TYA";
        case CPU_SLO: return "SLO";
        case CPU_ANC: return "ANC";
        case CPU_RLA: return "RLA";
        case CPU_SRE: return "SRE";
        case CPU_ALR: return "ALR";
        case CPU_RRA: return "RRA";
        case CPU_ARR: return "ARR";
        case CPU_SAX: return "SAX";
        case CPU_XAA: return "XAA";
        case CPU_AHX: return "AHX";
        case CPU_TAS: return "TAS";
        case CPU_SHY: return "SHY";
        case CPU_SHX: return "SHX";
        case CPU_LAX: return "LAX";
        case CPU_LAS: return "LAS";
        case CPU_DCP: return "DCP";
        case CPU_AXS: return "AXS";
        case CPU_ISC: return "ISC";
        case CPU_KIL: return "KIL";
        case CPU_FEX: return "FEX";
        default: return "UNK";
    }
}

internal inline char* GetRegisterStr(CPURegister cpuRegister)
{
    switch (cpuRegister)
    {
        case CPU_AR: return "A";
        case CPU_XR: return "X";
        case CPU_YR: return "Y";
        case CPU_ST: return "ST";
        case CPU_PC: return "PC";
        case CPU_SP:return "SP";
        default: return "UK";
    }
}

inline u8 ReadCPUU8(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;
    PPU *ppu = &nes->ppu;

    if (ISBETWEEN(address, 0x00, 0x2000))
    {
        // Memory locations $0000-$07FF are mirrored three times at $0800-$1FFF.
        // This means that, for example, any data written to $0000 will also be written to $0800, $1000 and $1800. 
        address = (address % 0x800);
        return ReadU8(&nes->cpuMemory, address);
    }
    
    if (ISBETWEEN(address, 0x2000, 0x4000))
    {
        // Memory locations $2000-$2008 are mirrored each 8 bytes.
        // This means that, for example, any data written to $2000 will also be written to $2008, $2010 and so on... 
        address = 0x2000 + ((address - 0x2000) % 0x08);

        switch (address)
        {
            case 0x2000:
            {
                return ReadCtrl(nes);
            }

            case 0x2001:
            {
                return ReadMask(nes);
            }

            case 0x2002:
            {
                return ReadStatus(nes);
            }

            case 0x2003:
            {
                return ReadOamAddr(nes);
            }

            case 0x2004:
            {
                return ReadOamData(nes);
            }

            case 0x2005:
            {
                return ReadScroll(nes);
            }

            case 0x2006:
            {
                return ReadVramAddr(nes);
            }

            case 0x2007:
            {
                return ReadVramData(nes);
            }
        }
    }
    
    if (ISBETWEEN(address, 0x4000, 0x4020))
    {
        switch (address)
        {
            case 0x4014:
            {
                return 0;
            }

            case 0x4016:
            {
                return ReadControllerU8(nes, 0);
            }

            case 0x4017:
            {
                return ReadControllerU8(nes, 1);
            }
        }
    }
    
    if (ISBETWEEN(address, 0x6000, 0x8000))
    {
        return ReadU8(&nes->cpuMemory, address);
    }
    
    if (ISBETWEEN(address, 0x8000, 0x10000))
    {
        return nes->mapperReadU8(nes, address);
    }

    // it should no get here
    return 0;
}

inline u16 ReadCPUU16(NES *nes, u16 address)
{
    // read in little-endian mode
    u8 lo = ReadCPUU8(nes, address);
    u8 hi = ReadCPUU8(nes, address + 1);
    return (hi << 8) | lo;
}

inline void WriteCPUU8(NES *nes, u16 address, u8 value)
{
    CPU *cpu = &nes->cpu;
    PPU *ppu = &nes->ppu;

    if (ISBETWEEN(address, 0x00, 0x2000))
    {
        // Memory locations $0000-$07FF are mirrored three times at $0800-$1FFF.
        // This means that, for example, any data written to $0000 will also be written to $0800, $1000 and $1800. 
        address = (address % 0x800);
        WriteU8(&nes->cpuMemory, address, value);
        return;
    }

    if (ISBETWEEN(address, 0x2000, 0x4000))
    {
        // Memory locations $2000-$2008 are mirrored each 8 bytes.
        // This means that, for example, any data written to $2000 will also be written to $2008, $2010 and so on... 
        address = 0x2000 + ((address - 0x2000) % 0x08);

        switch (address)
        {
            case 0x2000:
            {
                WriteCtrl(nes, value);
                return;
            }

            case 0x2001:
            {
                WriteMask(nes, value);
                return;
            }

            case 0x2002:
            {
                WriteStatus(nes, value);
                return;
            }

            case 0x2003:
            {
                WriteOamAddr(nes, value);
                return;
            }

            case 0x2004:
            {
                WriteOamData(nes, value);
                return;
            }

            case 0x2005:
            {
                WriteScroll(nes, value);
                return;
            }

            case 0x2006:
            {
                WriteVramAddr(nes, value);
                return;
            }

            case 0x2007:
            {
                WriteVramData(nes, value);
                return;
            }
        }
    }

    if (ISBETWEEN(address, 0x4000, 0x4020))
    {
        switch (address)
        {
            case 0x4014:
            {
                WriteDMA(nes, value);
                break;
            }

            case 0x4016:
            {
                WriteControllerU8(nes, 0, value);
                WriteControllerU8(nes, 1, value);
                break;
            }

            case 0x4017:
            {
                break;
            }
        }

        return;
    }

    if (ISBETWEEN(address, 0x6000, 0x8000))
    {
        WriteU8(&nes->cpuMemory, address, value);
        return;
    }

    if (ISBETWEEN(address, 0x8000, 0x10000))
    {
        nes->mapperWriteU8(nes, address, value);
        return;
    }
}

inline void WriteCPUU16(NES *nes, u16 address, u16 value)
{
    // write in little-endian mode
    u8 lo = value & 0xFF;
    u8 hi = (value & 0xFF00) >> 8;
    WriteCPUU8(nes, address, lo);
    WriteCPUU8(nes, address + 1, hi);
}

inline void PushStackU8(NES *nes, u8 value)
{
    CPU *cpu = &nes->cpu;
    u8 sp = cpu->sp;
    WriteCPUU8(nes, sp + 0x100, value);
    cpu->sp = sp > 0 ? sp - 1 : 0xFF;
}

inline void PushStackU16(NES *nes, u16 value)
{
    u8 lo = value & 0xFF;
    u8 hi = (value & 0xFF00) >> 8;
    PushStackU8(nes, hi);
    PushStackU8(nes, lo);
}

inline u8 PopStackU8(NES *nes)
{
    CPU *cpu = &nes->cpu;
    u8 sp = cpu->sp + 1;
    u8 v = ReadCPUU8(nes, sp + 0x100);
    cpu->sp = sp;
    return v;
}

inline u16 PopStackU16(NES *nes)
{
    u8 lo = PopStackU8(nes);
    u8 hi = PopStackU8(nes);
    return ((u16)hi << 8) | (u16)lo;
}

#define GetCarry(cpu) GetBitFlag(cpu->p, CARRY_FLAG)
#define SetCarry(cpu, v) \
    if (v) \
        SetBitFlag(&cpu->p, CARRY_FLAG); \
    else \
        ClearBitFlag(&cpu->p, CARRY_FLAG);

#define GetZero(cpu) GetBitFlag(cpu->p, ZERO_FLAG)
#define SetZero(cpu, v) \
    if (v) \
        SetBitFlag(&cpu->p, ZERO_FLAG); \
    else \
        ClearBitFlag(&cpu->p, ZERO_FLAG);

#define GetInterrupt(cpu) GetBitFlag(cpu->p, INTERRUPT_FLAG)
#define SetInterrupt(cpu, v) \
    if (v) \
        SetBitFlag(&cpu->p, INTERRUPT_FLAG); \
    else \
        ClearBitFlag(&cpu->p, INTERRUPT_FLAG);

#define GetDecimal(cpu) GetBitFlag(cpu->p, DECIMAL_FLAG)
#define SetDecimal(cpu, v) \
    if (v) \
        SetBitFlag(&cpu->p, DECIMAL_FLAG); \
    else \
        ClearBitFlag(&cpu->p, DECIMAL_FLAG);

#define GetBreak(cpu) GetBitFlag(cpu->p, BREAK_FLAG)
#define SetBreak(cpu, v) \
    if (v) \
        SetBitFlag(&cpu->p, BREAK_FLAG); \
    else \
        ClearBitFlag(&cpu->p, BREAK_FLAG);

#define GetOverflow(cpu) GetBitFlag(cpu->p, OVERFLOW_FLAG)
#define SetOverflow(cpu, v) \
    if (v) \
        SetBitFlag(&cpu->p, OVERFLOW_FLAG); \
    else \
        ClearBitFlag(&cpu->p, OVERFLOW_FLAG);

#define GetNegative(cpu) GetBitFlag(cpu->p, NEGATIVE_FLAG)
#define SetNegative(cpu, v) \
    if (v) \
        SetBitFlag(&cpu->p, NEGATIVE_FLAG); \
    else \
        ClearBitFlag(&cpu->p, NEGATIVE_FLAG);

void ResetCPU(NES *nes);
void PowerCPU(NES *nes);
void InitCPU(NES *nes);
CPUStep StepCPU(NES *nes);

#endif
