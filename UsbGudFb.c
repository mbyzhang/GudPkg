#include <Uefi.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/UsbIo.h>
#include <Protocol/ComponentName2.h>
#include <Protocol/ComponentName.h>
#include <Protocol/GraphicsOutput.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiUsbLib.h>
#include <Library/FrameBufferBltLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiBootManagerLib.h>

#include <IndustryStandard/Usb.h>

#include "GudDriver.h"

typedef struct {
  VENDOR_DEVICE_PATH DisplayDevicePath;
  EFI_DEVICE_PATH    EndDevicePath;
} DISPLAY_DEVICE_PATH;

STATIC CONST DISPLAY_DEVICE_PATH mDisplayDevicePath = {
    {{HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
          (UINT8)(sizeof(DISPLAY_DEVICE_PATH)),
          (UINT8)((sizeof(DISPLAY_DEVICE_PATH)) >> 8),
      }},
     EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID},
    {END_DEVICE_PATH_TYPE,
     END_ENTIRE_DEVICE_PATH_SUBTYPE,
     {sizeof(EFI_DEVICE_PATH_PROTOCOL), 0}}};

VOID
GopModeInfoFromGudMode
  (
    IN GUD_DRM_REQ_DISPLAY_MODE *GudMode,
    OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *GopInfo
  )
{
  GopInfo->HorizontalResolution = GudMode->HDisplay;
  GopInfo->VerticalResolution = GudMode->VDisplay;
  GopInfo->PixelsPerScanLine = GopInfo->HorizontalResolution;
  GopInfo->Version = 0;
  GopInfo->PixelFormat = PixelBlueGreenRedReserved8BitPerColor;
}

STATIC
EFI_STATUS
EFIAPI
GopQueryMode
(
    IN  EFI_GRAPHICS_OUTPUT_PROTOCOL          *This,
    IN  UINT32                                ModeNumber,
    OUT UINTN                                 *SizeOfInfo,
    OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  **Info
) {
  Print(L"UsbGudFbDriver: GopQueryMode called\n");

  USB_GUD_FB_DEV *GudDev = USB_GUD_FROM_GOP(This);
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  ModeInfo;
  
  if ((Info == NULL) || (SizeOfInfo == NULL) ||
      (ModeNumber >= This->Mode->MaxMode))
  {
    return EFI_INVALID_PARAMETER;
  }

  GopModeInfoFromGudMode(&GudDev->Modes[ModeNumber], &ModeInfo);

  *Info = AllocateCopyPool (
    sizeof (ModeInfo),
    &ModeInfo
  );

  if (*Info == NULL) {
    Print(L"UsbGudFbDriver: GopQueryMode: out of resources\n");
    return EFI_OUT_OF_RESOURCES;
  }

  *SizeOfInfo = sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
GopSetMode
(
    IN  EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    IN  UINT32                       ModeNumber
)
{
  Print(L"UsbGudFbDriver: GopSetMode called\n");
  EFI_TPL OldTpl;
  EFI_STATUS Status;
  USB_GUD_FB_DEV *GudDev = USB_GUD_FROM_GOP(This);
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *GopInfo = GudDev->Gop.Mode->Info;

  OldTpl = gBS->RaiseTPL(TPL_NOTIFY);

  if (ModeNumber >= GudDev->ModeCount) {
    Status = EFI_INVALID_PARAMETER;
    goto ErrorExit;
  }

  Status = GudSetMode(GudDev, ModeNumber);

  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  GopModeInfoFromGudMode(&GudDev->Modes[GudDev->ModeIndex], GopInfo);


  if (GudDev->FrameBufferBase) {
    FreePages(GudDev->FrameBufferBase, EFI_SIZE_TO_PAGES(GudDev->FrameBufferSize));
    GudDev->FrameBufferBase = NULL;
  }

  GudDev->FrameBufferSize = USB_GUD_BPP * GopInfo->HorizontalResolution * GopInfo->VerticalResolution;

  GudDev->FrameBufferBase = AllocatePages(EFI_SIZE_TO_PAGES(GudDev->FrameBufferSize));
  ASSERT (GudDev->FrameBufferBase);

  GudDev->FrameBufferDirty = FALSE;

  // Print(L"UsbGudFbDriver: size %dx%d\n", GopInfo->HorizontalResolution, GopInfo->VerticalResolution);

  Status = FrameBufferBltConfigure (
    (VOID *)GudDev->FrameBufferBase,
    GopInfo,
    GudDev->FrameBufferBltConfigure,
    &GudDev->FrameBufferBltConfigureSize
  );

  if (Status == RETURN_BUFFER_TOO_SMALL) {
    if (GudDev->FrameBufferBltConfigure != NULL) {
      FreePool (GudDev->FrameBufferBltConfigure);
    }

    GudDev->FrameBufferBltConfigure =
      AllocatePool (GudDev->FrameBufferBltConfigureSize);
    if (GudDev->FrameBufferBltConfigure == NULL) {
      GudDev->FrameBufferBltConfigureSize = 0;
      Print(L"UsbGudFbDriver: Blt configuration failed: out of resources\n");
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit2;
    }

    Status = FrameBufferBltConfigure (
      (VOID *)GudDev->FrameBufferBase,
      GopInfo,
      GudDev->FrameBufferBltConfigure,
      &GudDev->FrameBufferBltConfigureSize
    );
  }

  if (RETURN_ERROR (Status)) {
    Print(L"UsbGudFbDriver: Blt configuration failed\n");
    ASSERT (Status == RETURN_UNSUPPORTED);
    goto ErrorExit2;
  }

  GopInfo->PixelFormat = PixelBltOnly;
  Print(L"UsbGudFbDriver: Blt configured\n");

  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Black;
  ZeroMem (&Black, sizeof (Black));
  Status = FrameBufferBlt (
    GudDev->FrameBufferBltConfigure,
    &Black,
    EfiBltVideoFill,
    0,
    0,
    0,
    0,
    This->Mode->Info->HorizontalResolution,
    This->Mode->Info->VerticalResolution,
    0
  );

  ASSERT_RETURN_ERROR (Status);

  gBS->RestoreTPL(OldTpl);
  return Status;
ErrorExit2:
  FreePages(GudDev->FrameBufferBase, EFI_SIZE_TO_PAGES(GudDev->FrameBufferSize));
  GudDev->FrameBufferBase = NULL;

ErrorExit:
  gBS->RestoreTPL(OldTpl);
  return Status;
}

STATIC
EFI_STATUS
EFIAPI
GopBlt(
	IN  EFI_GRAPHICS_OUTPUT_PROTOCOL      *This,
	IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL     *BltBuffer, OPTIONAL
	IN  EFI_GRAPHICS_OUTPUT_BLT_OPERATION BltOperation,
	IN  UINTN                             SourceX,
	IN  UINTN                             SourceY,
	IN  UINTN                             DestinationX,
	IN  UINTN                             DestinationY,
	IN  UINTN                             Width,
	IN  UINTN                             Height,
	IN  UINTN                             Delta         OPTIONAL
)
{
  // Print(L"UsbGudFbDriver: GopBlt called\n");
  EFI_STATUS Status;
  USB_GUD_FB_DEV *GudDev = USB_GUD_FROM_GOP(This);
  EFI_TPL                      OldTpl;

  OldTpl = gBS->RaiseTPL(TPL_NOTIFY);

  Status = FrameBufferBlt (
    GudDev->FrameBufferBltConfigure,
    BltBuffer,
    BltOperation,
    SourceX,
    SourceY,
    DestinationX,
    DestinationY,
    Width,
    Height,
    Delta
  );

  if (Status == EFI_SUCCESS && BltOperation != EfiBltVideoToBltBuffer) {
#if 0
    Status = GudFlush(GudDev, DestinationX, DestinationY, Width, Height);
    if (EFI_ERROR(Status)) {
      Print(L"UsbGudFbDriver: failed to flush buffer\n");
      goto ErrorExit;
    }
#else
    GudQueueFlush(GudDev, DestinationX, DestinationY, Width, Height);
#endif
  }

// ErrorExit:
  gBS->RestoreTPL(OldTpl);
  return Status;
}


STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL mGopTemplate = {
  GopQueryMode,
  GopSetMode,
  GopBlt,
  NULL
};

EFI_STATUS
EFIAPI
UsbGudFbDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS                   Status;
  EFI_USB_IO_PROTOCOL          *UsbIo;
  EFI_USB_IO_PROTOCOL          *ChildUsbIo;
  USB_GUD_FB_DEV               *GudDev;
  EFI_TPL                      OldTpl;

  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);

  //
  // Open USB I/O Protocol
  //
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiUsbIoProtocolGuid,
                  (VOID **)&UsbIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );

  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  //
  // Allocate device data structure
  //
  GudDev = AllocateZeroPool(sizeof(USB_GUD_FB_DEV));  
  ASSERT(GudDev != NULL);

  GudDev->Signature = USB_GUD_FB_DEV_SIGNATURE;
  GudDev->UsbIo = UsbIo;

  //
  // Initialize GUD
  //
  Status = GudInit(GudDev);

  if (EFI_ERROR(Status)) {
    Print(L"UsbGudFbDriver: failed to initialize\n");
    goto ErrorExit1;
  }

  Print(L"UsbGudFbDriver: initialized\n");

  //
  // Initialize Gop
  //
  GudDev->Gop = mGopTemplate;
  GudDev->Gop.Mode = AllocateZeroPool(sizeof(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE));
  ASSERT (GudDev->Gop.Mode);

  GudDev->Gop.Mode->MaxMode = GudDev->ModeCount;
  GudDev->Gop.Mode->Mode = GudDev->ModeIndex;
  GudDev->Gop.Mode->SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
  GudDev->Gop.Mode->Info = AllocateZeroPool(sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION));
  ASSERT (GudDev->Gop.Mode->Info);

  Status = GopSetMode(&GudDev->Gop, 0);

  if (EFI_ERROR(Status)) {
    Print(L"UsbGudFbDriver: failed to set mode\n");
    goto ErrorExit2;
  }

  EFI_DEVICE_PATH *ParentDevicePath;

  Status = gBS->HandleProtocol (
    ControllerHandle,
    &gEfiDevicePathProtocolGuid,
    (VOID **)&ParentDevicePath
  );

  if (EFI_ERROR (Status)) {
    goto ErrorExit2;
  }

  GudDev->GopDevicePath = AppendDevicePathNode(
    ParentDevicePath,
    &mDisplayDevicePath.DisplayDevicePath.Header
  );

  if (GudDev->GopDevicePath == NULL) {
    Print(L"UsbGudFbDriver: failed to append device path\n");
    goto ErrorExit2;
  }

  Status = gBS->InstallMultipleProtocolInterfaces(
    &GudDev->GopHandle,
    &gEfiDevicePathProtocolGuid,
    GudDev->GopDevicePath,
    &gEfiGraphicsOutputProtocolGuid,
    &GudDev->Gop,
    NULL
  );

  if (EFI_ERROR(Status)) {
    Print(L"UsbGudFbDriver: install GOP failed\n");
    goto ErrorExit3;
  }

  //
  // Reference parent handle from child handle.
  //
  Status = gBS->OpenProtocol (
    ControllerHandle,
    &gEfiUsbIoProtocolGuid,
    (VOID **)&ChildUsbIo,
    This->DriverBindingHandle,
    GudDev->GopHandle,
    EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
  );

  if (EFI_ERROR (Status)) {
    goto ErrorExit3;
  }

  Print(L"UsbGudFbDriver: GOP initialized\n");

  //
  // Update ConOut UEFI variable
  //
  Status = EfiBootManagerUpdateConsoleVariable (ConOut, GudDev->GopDevicePath, NULL);

  if (EFI_ERROR (Status)) {
    Print(L"UsbGudFbDriver: failed to update console variable\n");
    Status = EFI_SUCCESS;
  }

  Status = gBS->ConnectController (GudDev->GopHandle, NULL, NULL, TRUE);

  if (EFI_ERROR (Status)) {
    Print(L"UsbGudFbDriver: failed to connect controller\n");
    Status = EFI_SUCCESS;
  }

  //
  // Start GUD polling timer
  //
  Status = GudStartPolling(GudDev);

  if (EFI_ERROR (Status)) {
    Print(L"UsbGudFbDriver: failed to start polling\n");
    goto ErrorExit3;
  }

  gBS->RestoreTPL (OldTpl);
  return Status;
ErrorExit3:
  // TODO: close protocol
  FreePool(GudDev->GopDevicePath);
ErrorExit2:
  FreePool(GudDev->Gop.Mode->Info);
  FreePool(GudDev->Gop.Mode);

ErrorExit1:
  FreePool(GudDev);

ErrorExit:
  gBS->RestoreTPL (OldTpl);
  return Status;
}


EFI_STATUS
EFIAPI
UsbGudFbDriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS                    Status;
  EFI_USB_IO_PROTOCOL           *UsbIo;

  //
  // Open the USB I/O Protocol on ControllerHandle
  //
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiUsbIoProtocolGuid,
                  (VOID **)&UsbIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    // Print(L"UsbGudFbDriverBindingSupported: failed to open USB I/O Protocol\n");
    return Status;
  }

  //
  // Check to see if the interface descriptor is supported by this driver
  //
  BOOLEAN IsGud = GudDetect(UsbIo);
  if (!IsGud) {
    // Print(L"UsbGudFbDriverBindingSupported: GUD detection failed\n");
    Status = EFI_UNSUPPORTED;
    goto Done;
  }
  else {
    Status = EFI_SUCCESS;
  }

Done:
  //
  // Close the USB I/O Protocol
  //
  gBS->CloseProtocol (
         ControllerHandle,
         &gEfiUsbIoProtocolGuid,
         This->DriverBindingHandle,
         ControllerHandle
         );

  return Status;
}


EFI_STATUS
EFIAPI
UsbGudFbDriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL     *This,
  IN  EFI_HANDLE                      ControllerHandle,
  IN  UINTN                           NumberOfChildren,
  IN  EFI_HANDLE                      *ChildHandleBuffer
  )
{
  EFI_STATUS                Status;
  Status = EFI_SUCCESS;
  return Status;
}

EFI_DRIVER_BINDING_PROTOCOL gUsbGudFbDriverBinding = {
  UsbGudFbDriverBindingSupported,
  UsbGudFbDriverBindingStart,
  UsbGudFbDriverBindingStop,
  0x10,
  NULL,
  NULL
};

EFI_STATUS
EFIAPI
UsbGudFbDriverEntryPoint (
  IN EFI_HANDLE                      ImageHandle,
  IN EFI_SYSTEM_TABLE                *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,             // ImageHandle
             SystemTable,             // SystemTable
             &gUsbGudFbDriverBinding, // DriverBinding
             ImageHandle,             // DriverBindingHandle
             NULL,
             NULL
             );
  ASSERT_EFI_ERROR (Status);

  return Status;
}
