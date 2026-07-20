#pragma once

#include <Uefi.h>

VOID* AllocateZeroPool(UINTN AllocationSize);
VOID FreePool(VOID* Buffer);

