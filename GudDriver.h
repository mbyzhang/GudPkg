#ifndef _EFI_GUD_DRIVER_H_
#define _EFI_GUD_DRIVER_H_

#include <Protocol/UsbIo.h>
#include <Protocol/GraphicsOutput.h>
#include "GudDrm.h"

#define USB_GUD_FB_DEV_SIGNATURE SIGNATURE_32 ('g','u','d','F')

typedef struct {
  UINTN                         Signature;
  EFI_GRAPHICS_OUTPUT_PROTOCOL  Gop;
  EFI_USB_IO_PROTOCOL           *UsbIo;
  EFI_USB_INTERFACE_DESCRIPTOR  InterfaceDescriptor;
  EFI_USB_ENDPOINT_DESCRIPTOR   BulkEndpointDescriptor;
  GUD_DRM_USB_VENDOR_DESCRIPTOR VendorDescriptor;
} USB_GUD_FB_DEV;

EFI_STATUS
GudInit (
  IN OUT USB_GUD_FB_DEV *GudDev
);

BOOLEAN
GudDetect(
  EFI_USB_IO_PROTOCOL *UsbIo
);

#endif
