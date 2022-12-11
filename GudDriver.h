#ifndef _EFI_GUD_DRIVER_H_
#define _EFI_GUD_DRIVER_H_

#include <Protocol/UsbIo.h>
#include <Protocol/GraphicsOutput.h>
#include <Library/DebugLib.h>
#include <Library/FrameBufferBltLib.h>
#include "GudDrm.h"

#define USB_GUD_BPP 4
#define USB_GUD_MAX_BUFFER_SIZE (4 * 1024 * 1024) // 4MB
#define USB_GUD_FB_DEV_SIGNATURE SIGNATURE_32 ('g','u','d','F')

typedef struct {
  UINTN                         Signature;
  EFI_GRAPHICS_OUTPUT_PROTOCOL  Gop;
  EFI_USB_IO_PROTOCOL           *UsbIo;
  EFI_USB_INTERFACE_DESCRIPTOR  InterfaceDescriptor;
  EFI_USB_ENDPOINT_DESCRIPTOR   BulkEndpointDescriptor;
  GUD_DRM_USB_VENDOR_DESCRIPTOR VendorDescriptor;

  UINT8                         ModeIndex;
  UINT8                         ModeCount;
  GUD_DRM_REQ_DISPLAY_MODE      Modes[GUD_CONNECTOR_MAX_NUM_MODES];

  UINT8 ConnectorCount;
  GUD_DRM_REQ_CONNECTOR_DESCRIPTOR Connectors[GUD_CONNECTORS_MAX_NUM];

  UINT8 PropertyCount;
  GUD_DRM_REQ_PROPERTY Properties[GUD_PROPERTIES_MAX_NUM];

  UINT8 ConnectorPropertyCount;
  GUD_DRM_REQ_PROPERTY ConnectorProperties[GUD_CONNECTOR_PROPERTIES_MAX_NUM];

  UINTN                         FrameBufferSize;
  UINT8                         *FrameBufferBase;
  UINTN                         FrameBufferBltConfigureSize;
  FRAME_BUFFER_CONFIGURE        *FrameBufferBltConfigure;
  EFI_HANDLE                    GopHandle;
  UINT8                         *TransferBuffer;
  EFI_DEVICE_PATH               *GopDevicePath;
} USB_GUD_FB_DEV;

#define USB_GUD_FROM_GOP(This) CR(This, USB_GUD_FB_DEV, Gop, USB_GUD_FB_DEV_SIGNATURE)
#define USB_GUD_FB_AT(Dev, X, Y) (Dev->FrameBufferBase + USB_GUD_BPP * (Dev->Modes[Dev->ModeIndex].HDisplay * Y + X))

EFI_STATUS
GudInit (
  IN OUT USB_GUD_FB_DEV *GudDev
);

BOOLEAN
GudDetect(
  IN EFI_USB_IO_PROTOCOL *UsbIo
);

EFI_STATUS
GudFlush(
  IN OUT USB_GUD_FB_DEV *GudDev,
  IN UINTN X,
  IN UINTN Y,
  IN UINTN Width,
  IN UINTN Height
);

EFI_STATUS
GudSetMode (
  IN OUT USB_GUD_FB_DEV *GudDev,
  IN UINT8 ModeIndex
);

#endif
