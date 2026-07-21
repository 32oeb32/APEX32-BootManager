#include <Uefi.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/GraphicsOutput.h>

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                    Status;
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Signature;

  (VOID)ImageHandle;
  (VOID)SystemTable;

  Gop = NULL;
  Status = gBS->LocateProtocol (
                  &gEfiGraphicsOutputProtocolGuid,
                  NULL,
                  (VOID **)&Gop
                  );
  if (EFI_ERROR (Status) || (Gop == NULL) || (Gop->Mode == NULL) ||
      (Gop->Mode->Info == NULL)) {
    return EFI_ERROR (Status) ? Status : EFI_UNSUPPORTED;
  }

  Signature.Blue     = 0xD4;
  Signature.Green    = 0x78;
  Signature.Red      = 0x00;
  Signature.Reserved = 0;
  Status = Gop->Blt (
                  Gop,
                  &Signature,
                  EfiBltVideoFill,
                  0,
                  0,
                  0,
                  0,
                  Gop->Mode->Info->HorizontalResolution,
                  Gop->Mode->Info->VerticalResolution,
                  0
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (;;) {
    gBS->Stall (1000000);
  }
}
