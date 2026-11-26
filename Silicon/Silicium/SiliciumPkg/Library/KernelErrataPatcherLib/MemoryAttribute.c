/**
  Based on https://github.com/SamuelTulach/rainbow

  Copyright (c) 2021 Samuel Tulach
  Copyright (c) 2022-2023 DuoWoA authors
  Copyright (c) 2026 Project Silicium

  SPDX-License-Identifier: MIT
**/

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/MemoryAttribute.h>

//
// Global Variables
//
STATIC EFI_MEMORY_ATTRIBUTE_PROTOCOL *mMemoryAttributeProtocol;

//
// Winload Memory
//
extern EFI_PHYSICAL_ADDRESS mWinloadBase;
extern EFI_PHYSICAL_ADDRESS mWinloadEnd;

EFI_STATUS
SetMemoryProtection (
  IN EFI_PHYSICAL_ADDRESS Base,
  IN UINTN                Length,
  IN BOOLEAN              Enable
  )
{
  // Verify Memory Attribute Protocol
  if (mMemoryAttributeProtocol == NULL) {
    return EFI_SUCCESS;
  }

  if (Base == 0 || Length == 0) {
    return EFI_INVALID_PARAMETER;
  }

  UINTN Offset = (UINTN)Base & (EFI_PAGE_SIZE - 1);
  Base        &= ~(EFI_PAGE_SIZE - 1);
  Length       = ALIGN_VALUE (Length + Offset, EFI_PAGE_SIZE);

  // Set / Clear Read-Only Memory Attribute
  return Enable ? mMemoryAttributeProtocol->SetMemoryAttributes   (mMemoryAttributeProtocol, Base, Length, EFI_MEMORY_RO)
                : mMemoryAttributeProtocol->ClearMemoryAttributes (mMemoryAttributeProtocol, Base, Length, EFI_MEMORY_RO);
}

EFI_STATUS
SetWinloadProtection (IN BOOLEAN Enable)
{
  // Verify Winload Memory Range
  if (mWinloadBase == 0 || mWinloadEnd == 0) {
    return EFI_INVALID_PARAMETER;
  }

  return SetMemoryProtection (mWinloadBase, mWinloadEnd - mWinloadBase, Enable);
}

EFI_STATUS
LocateMemoryAttributeProtocol (VOID)
{
  EFI_STATUS Status;

  // Locate Memory Attribute Protocol
  Status = gBS->LocateProtocol (&gEfiMemoryAttributeProtocolGuid, NULL, (VOID *)&mMemoryAttributeProtocol);
  if (EFI_ERROR (Status) || mMemoryAttributeProtocol == NULL) {
    DEBUG ((EFI_D_WARN, "[KEP] Memory Attribute Protocol unavailable: %r\n", Status));
  } else {
    DEBUG ((EFI_D_WARN, "[KEP] Memory Attribute Protocol available\n"));
  }

  return EFI_SUCCESS;
}
