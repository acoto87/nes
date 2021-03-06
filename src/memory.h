#pragma once
#ifndef MEMORY_H
#define MEMORY_H

inline void CreateMemory(Memory *memory, u32 length)
{
    memory->bytes = (u8*)Allocate(length);
    memory->length = length;
    memory->created = true;
}

inline void DestroyMemory(Memory *memory)
{
    if (memory->bytes)
    {
        Free(memory->bytes);
    }
}

inline void ZeroMemoryBytes(Memory *memory)
{
    memset(memory->bytes, 0, sizeof(memory->length));
}

inline void CopyMemoryBytes(Memory *memory, u32 offset, u8 *bytes, size size)
{
    if (memory->created)
    {
        memcpy(memory->bytes + offset, bytes, size);
    }
}

inline u8 ReadU8(Memory *memory, u16 address)
{
    ASSERT(address < memory->length);
    return memory->bytes[address];
}

inline void WriteU8(Memory *memory, u16 address, u8 value)
{
    ASSERT(address < memory->length);
    memory->bytes[address] = value;
}

inline u16 ReadU16(Memory *memory, u16 address)
{
    // read in little-endian mode
    u8 lo = ReadU8(memory, address);
    u8 hi = ReadU8(memory, address + 1);
    return (hi << 8) | lo;
}

inline void WriteU16(Memory *memory, u16 address, u16 value)
{
    // write in little-endian mode
    u8 lo = value & 0xFF;
    u8 hi = (value & 0xFF00) >> 8;
    WriteU8(memory, address, lo);
    WriteU8(memory, address + 1, hi);
}

#endif // !MEMORY_H


