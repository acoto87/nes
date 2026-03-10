#ifndef MAPPER_H
#define MAPPER_H

#include "types.h"

void Mapper0Init(NES *nes);
u8 Mapper0ReadU8(NES *nes, u16 address);
void Mapper0WriteU8(NES *nes, u16 address, u8 value);

void Mapper1Init(NES *nes);
u8 Mapper1ReadU8(NES *nes, u16 address);
void Mapper1WriteU8(NES *nes, u16 address, u8 value);
void Mapper1Save(NES *nes, FILE *file);
void Mapper1Load(NES *nes, FILE *file);

void Mapper2Init(NES *nes);
u8 Mapper2ReadU8(NES *nes, u16 address);
void Mapper2WriteU8(NES *nes, u16 address, u8 value);

void Mapper3Init(NES *nes);
u8 Mapper3ReadU8(NES *nes, u16 address);
void Mapper3WriteU8(NES *nes, u16 address, u8 value);

void Mapper66Init(NES *nes);
u8 Mapper66ReadU8(NES *nes, u16 address);
void Mapper66WriteU8(NES *nes, u16 address, u8 value);

#endif
