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

#include <IndustryStandard/Usb.h>

#include "GudDriver.h"

EFI_STATUS
EFIAPI
UsbGudFbDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS                   Status;
  EFI_USB_IO_PROTOCOL          *UsbIo;
  USB_GUD_FB_DEV               *UsbGudFbDevice;
  EFI_TPL                      OldTpl;

  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);

  //
  // Open USB I/O Protocol
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiUsbIoProtocolGuid,
                  (VOID **)&UsbIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );

  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  //
  // Allocate device data structure
  //
  UsbGudFbDevice = AllocateZeroPool(sizeof(USB_GUD_FB_DEV));  
  ASSERT(UsbGudFbDevice != NULL);

  UsbGudFbDevice->Signature = USB_GUD_FB_DEV_SIGNATURE;
  UsbGudFbDevice->UsbIo = UsbIo;

  //
  // Initialize GUD
  //
  Status = GudInit(UsbGudFbDevice);

  if (EFI_ERROR(Status)) {
    Print(L"UsbGudFbDriver: failed to initialize\n");
    goto ErrorExit;
  }

  // EFI_GRAPHICS_OUTPUT_PROTOCOL Display;
  // Display.Mode->Info

  Print(L"UsbGudFbDriver: initialized\n");
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
    return Status;
  }
  

  //
  // Check to see if the interface descriptor is supported by this driver
  //
  BOOLEAN IsGud = GudDetect(UsbIo);
  if (!IsGud) {
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
  IN  EFI_HANDLE                      Controller,
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
  0xa,
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
