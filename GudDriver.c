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

#define GUD_INTERFACE L"GUD USB Display"

#define USB_INTERFACE_CLASS_VENDOR_SPECIFIC 0xff
#define LANG_ID_EN_US 0x0409

EFI_STATUS
GudSendControlMessage (
    IN OUT USB_GUD_FB_DEV *GudDev,
    IN EFI_USB_DATA_DIRECTION Dir,
    IN UINT8 Request,
    IN UINT8 Value,
    IN OUT UINT8* Data,
    IN UINTN DataLength,
    OUT UINT32 *UsbStatus OPTIONAL
  )
{
  EFI_USB_DEVICE_REQUEST DeviceRequest;
  UINT32 LocalUsbStatus;
  EFI_STATUS Status;

  ZeroMem (&DeviceRequest, sizeof(DeviceRequest));

  DeviceRequest.Index = 0;
  DeviceRequest.Length = DataLength;
  DeviceRequest.Request = Request;
  DeviceRequest.Value = Value;
  DeviceRequest.RequestType = USB_REQ_TYPE_VENDOR | USB_TARGET_INTERFACE;

  if (Dir == EfiUsbDataIn) {
    DeviceRequest.RequestType |= USB_ENDPOINT_DIR_IN;
  }

  Status = GudDev->UsbIo->UsbControlTransfer (
    GudDev->UsbIo,
    &DeviceRequest,
    Dir,
    PcdGet32 (PcdUsbTransferTimeoutValue),
    Data,
    DataLength,
    &LocalUsbStatus
  );

  // DEBUG((DEBUG_INFO,"UsbGudFbDriver: usb result = 0x%08x\n", LocalUsbStatus));

  if (UsbStatus) {
    *UsbStatus = LocalUsbStatus;
  }

  return Status;
}

EFI_STATUS
GudGetStatus (
    IN OUT USB_GUD_FB_DEV   *GudDev,
    OUT UINT8               *GudStatus,
    OUT UINT32              *UsbStatus OPTIONAL
  )
{
  EFI_STATUS Status;
  UINT8 Buf[1];

  Status = GudSendControlMessage (
    GudDev,
    EfiUsbDataIn,
    GUD_REQ_GET_STATUS,
    0,
    Buf,
    1,
    UsbStatus
  );

  if (EFI_ERROR(Status)) {
    return Status;
  }

  *GudStatus = Buf[0];

  return Status;
}

EFI_STATUS
GudUsbTransfer (
    IN OUT USB_GUD_FB_DEV *GudDev,
    IN EFI_USB_DATA_DIRECTION Dir,
    IN UINT8 Request,
    IN UINT8 Value,
    IN OUT UINT8* Data,
    IN UINTN DataLength,
    OUT UINT8 *GudStatus
  )
{
  EFI_STATUS Status;
  UINT32 UsbStatus;
  UINT8 LocalGudStatus = 0;
  BOOLEAN Stall = FALSE;

  Status = GudSendControlMessage(
    GudDev,
    Dir,
    Request,
    Value,
    Data,
    DataLength,
    &UsbStatus
  );

  if (EFI_ERROR(Status)) {
    if (Status == EFI_DEVICE_ERROR && (UsbStatus & EFI_USB_ERR_STALL) == EFI_USB_ERR_STALL) {
      // DEBUG((DEBUG_INFO,"UsbGudFbDriver: stalled\n");
      Stall = TRUE;
    }
    else {
      // DEBUG((DEBUG_INFO,"UsbGudFbDriver: error: %d\n", Status);
      // DEBUG((DEBUG_INFO,"UsbGudFbDriver: got other usb transfer result: 0x%08x\n", UsbStatus);
      goto ErrorExit;
    }
  }

  if (Stall || ((GudDev->VendorDescriptor.Flags & GUD_DISPLAY_FLAG_STATUS_ON_SET) && Dir != EfiUsbDataIn))
  {
    Status = GudGetStatus(GudDev, &LocalGudStatus, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG((DEBUG_INFO,"UsbGudFbDriver: failed to get gud status\n"));
      goto ErrorExit;
    }

    if (Stall && LocalGudStatus == GUD_STATUS_OK) {
      DEBUG((DEBUG_INFO,"UsbGudFbDriver: unexpected OK status on stall\n"));
      Status = EFI_DEVICE_ERROR;
      goto ErrorExit;
    }

    if (LocalGudStatus) {
      Status = EFI_DEVICE_ERROR;
      DEBUG((DEBUG_INFO,"UsbGudFbDriver: got gud status %d from device\n", LocalGudStatus));
    }
  }

ErrorExit:
  if (GudStatus) {
    *GudStatus = LocalGudStatus;
  }

  return Status;
}

EFI_STATUS
GudGetArray (
    IN OUT USB_GUD_FB_DEV *GudDev,
    IN UINT8 Request,
    IN UINT8 Value,
    IN OUT UINT8* Data,
    IN UINT8 DataUnitLength,
    IN UINT8 MaxCount,
    OUT UINT8 *Count,
    OUT UINT8 *GudStatus
  )
{
  EFI_STATUS Status;
  // UINT8 LocalGudStatus;

  // Status = GudUsbTransfer(GudDev, EfiUsbDataIn, Request, Value, Data, DataUnitLength * MaxCount, &LocalGudStatus);

  // if (GudStatus) {
  //   *GudStatus = LocalGudStatus;
  // }

  // if (!(Status == EFI_DEVICE_ERROR && LocalGudStatus == 0)) {
  //   return Status;
  // }

  // work around

  EFI_STATUS LastStatus;
  UINT8 Index;

  for (Index = 1; Index <= MaxCount; Index++) {
    EFI_USB_DATA_DIRECTION Dir = (Index)? EfiUsbDataIn : EfiUsbNoData;
    Status = GudUsbTransfer(GudDev, Dir, Request, Value, Data, DataUnitLength * Index, GudStatus);
    if (EFI_ERROR(Status)) {
      break;
    }
    LastStatus = Status;
  }

  *Count = Index - 1;

  if (Index == 1) {
    return Status;
  }
  else {
    return LastStatus;
  }
}

EFI_STATUS
GudGetU8 (
    IN OUT USB_GUD_FB_DEV *GudDev,
    IN UINT8 Request,
    IN UINT8 Index,
    OUT UINT8 *Value,
    OUT UINT8 *GudStatus OPTIONAL
  ) 
{
  EFI_STATUS Status;
  UINT8 Buf[1];

  Status = GudUsbTransfer (
    GudDev,
    EfiUsbDataIn,
    Request,
    Index,
    Buf,
    1,
    GudStatus
  );

  if (EFI_ERROR(Status))
  {
    return Status;
  }

  *Value = Buf[0];

  return Status;
}


EFI_STATUS
GudSetU8 (
    IN OUT USB_GUD_FB_DEV *GudDev,
    IN UINT8 Request,
    IN UINT8 Index,
    IN UINT8 Value,
    OUT UINT8 *GudStatus OPTIONAL
  ) 
{
  EFI_STATUS Status;
  UINT8 Buf[1];

  Buf[0] = Value;
  Status = GudUsbTransfer (
    GudDev,
    EfiUsbDataOut,
    Request,
    Index,
    Buf,
    1,
    GudStatus
  );

  return Status;
}

EFI_STATUS
GudSetDisplayEnable (
    IN OUT USB_GUD_FB_DEV *GudDev,
    IN BOOLEAN Enabled
  )
{
  return GudSetU8 (
    GudDev,
    GUD_REQ_SET_DISPLAY_ENABLE,
    0,
    (Enabled)? 1 : 0,
    NULL
  );
}


EFI_STATUS
GudSetControllerEnable (
    IN OUT USB_GUD_FB_DEV *GudDev,
    IN BOOLEAN Enabled
  )
{
  return GudSetU8 (
    GudDev,
    GUD_REQ_SET_CONTROLLER_ENABLE,
    0,
    (Enabled)? 1 : 0,
    NULL
  );
}

EFI_STATUS
GudGetDescriptor(
    IN OUT USB_GUD_FB_DEV *GudDev,
    OUT GUD_DRM_USB_VENDOR_DESCRIPTOR *Descriptor
  )
{
  UINT32 UsbStatus;
  EFI_STATUS Status;

  Status = GudSendControlMessage (
    GudDev,
    EfiUsbDataIn,
    GUD_REQ_GET_DESCRIPTOR,
    0,
    (UINT8*)Descriptor,
    sizeof(GUD_DRM_USB_VENDOR_DESCRIPTOR),
    &UsbStatus
  );
  
  if (EFI_ERROR (Status))
  {
    goto ErrorExit;
  }

  if (Descriptor->Magic != GUD_DISPLAY_MAGIC)
  {
    DEBUG((DEBUG_INFO,"UsbGudFbDriver: unexpected magic %x\n", Descriptor->Magic));
    Status = EFI_DEVICE_ERROR;
    goto ErrorExit;
  }

  if (Descriptor->Version == 0 || Descriptor->MaxWidth == 0 || Descriptor->MaxHeight == 0 || 
      Descriptor->MinWidth > Descriptor->MaxWidth || Descriptor->MinHeight > Descriptor->MaxHeight)
  {
    DEBUG((DEBUG_INFO,"UsbGudFbDriver: bad descriptor\n"));
    Status = EFI_DEVICE_ERROR;
    goto ErrorExit;
  }

  if (Descriptor->MaxBufferSize == 0 || Descriptor->MaxBufferSize > USB_GUD_MAX_BUFFER_SIZE) {
    Descriptor->MaxBufferSize = USB_GUD_MAX_BUFFER_SIZE;
  }

ErrorExit:
  return Status;
}

EFI_STATUS
GudGetConnectors (
    IN OUT USB_GUD_FB_DEV *GudDev,
    OUT GUD_DRM_REQ_CONNECTOR_DESCRIPTOR *Connectors,
    OUT UINT8 *Count,
    OUT UINT8 *GudStatus
  )
{
  return GudGetArray(
    GudDev,
    GUD_REQ_GET_CONNECTORS,
    0,
    (UINT8*)Connectors,
    sizeof(*Connectors),
    GUD_CONNECTORS_MAX_NUM,
    Count,
    GudStatus
  );
}

EFI_STATUS
GudGetConnectorModes (
    IN OUT USB_GUD_FB_DEV *GudDev,
    IN UINT8 ConnectorIndex,
    OUT GUD_DRM_REQ_DISPLAY_MODE *Modes,
    OUT UINT8 *Count,
    OUT UINT8 *GudStatus
  )
{
  return GudGetArray(
    GudDev,
    GUD_REQ_GET_CONNECTOR_MODES,
    ConnectorIndex,
    (UINT8*)Modes,
    sizeof(*Modes),
    GUD_CONNECTOR_MAX_NUM_MODES,
    Count,
    GudStatus
  );
}

EFI_STATUS
GudGetProperties (
    IN OUT USB_GUD_FB_DEV *GudDev,
    OUT GUD_DRM_REQ_PROPERTY *Properties,
    OUT UINT8 *Count,
    OUT UINT8 *GudStatus
  )
{
  return GudGetArray(
    GudDev,
    GUD_REQ_GET_PROPERTIES,
    0,
    (UINT8*)Properties,
    sizeof(*Properties),
    GUD_PROPERTIES_MAX_NUM,
    Count,
    GudStatus
  );
}

EFI_STATUS
GudGetConnectorProperties (
    IN OUT USB_GUD_FB_DEV *GudDev,
    IN UINT8 ConnectorIndex,
    OUT GUD_DRM_REQ_PROPERTY *Properties,
    OUT UINT8 *Count,
    OUT UINT8 *GudStatus
  )
{
  return GudGetArray(
    GudDev,
    GUD_REQ_GET_CONNECTOR_PROPERTIES,
    ConnectorIndex,
    (UINT8*)Properties,
    sizeof(*Properties),
    GUD_CONNECTOR_PROPERTIES_MAX_NUM,
    Count,
    GudStatus
  );
}

EFI_STATUS
GudSetStateCheck (
    IN OUT USB_GUD_FB_DEV *GudDev,
    IN GUD_DRM_REQ_SET_STATE *State,
    OUT UINT8 *GudStatus
  )
{
  return GudUsbTransfer(
    GudDev,
    EfiUsbDataOut,
    GUD_REQ_SET_STATE_CHECK,
    0,
    (UINT8*)State,
    sizeof(GUD_DRM_REQ_SET_STATE),
    GudStatus
  );
}

EFI_STATUS
GudSetStateCommit (
    IN OUT USB_GUD_FB_DEV *GudDev,
    OUT UINT8 *GudStatus
  )
{
  return GudUsbTransfer(
    GudDev,
    EfiUsbNoData,
    GUD_REQ_SET_STATE_COMMIT,
    0,
    NULL,
    0,
    GudStatus
  );
}

EFI_STATUS
GudSetBuffer (
    IN OUT USB_GUD_FB_DEV *GudDev,
    IN GUD_DRM_REQ_SET_BUFFER *Req,
    OUT UINT8 *GudStatus OPTIONAL
  )
{
  return GudUsbTransfer(
    GudDev,
    EfiUsbDataOut,
    GUD_REQ_SET_BUFFER,
    0,
    (UINT8*)Req,
    sizeof(GUD_DRM_REQ_SET_BUFFER),
    GudStatus
  );
}

EFI_STATUS
GudSetMode (
    IN OUT USB_GUD_FB_DEV *GudDev,
    IN UINT8 ModeIndex
  )
{
  EFI_STATUS Status;
  UINT8 Index;
  GUD_DRM_REQ_SET_STATE *State;
  UINT8 PropertyCount = GudDev->PropertyCount;
  UINT8 ConnectorPropertyCount = GudDev->ConnectorPropertyCount;

  GudDev->ModeIndex = ModeIndex;

  UINTN StateSize = sizeof(GUD_DRM_REQ_SET_STATE) + sizeof(GUD_DRM_REQ_PROPERTY) * (PropertyCount + ConnectorPropertyCount);
  State = AllocateZeroPool(StateSize);

  State->Connector = 0;
  State->Mode = GudDev->Modes[GudDev->ModeIndex];
  State->Format = GUD_PIXEL_FORMAT_XRGB8888;

  for (Index = 0; Index < PropertyCount + ConnectorPropertyCount; Index++) {
    if (Index < PropertyCount) {
      State->Properties[Index] = GudDev->Properties[Index];
    }
    else {
      State->Properties[Index] = GudDev->ConnectorProperties[Index - PropertyCount];
    }
  }

  Status = GudSetStateCheck(GudDev, State, NULL);

  if (EFI_ERROR (Status)) {
    DEBUG((DEBUG_INFO,"UsbGudFbDriver: failed to set state check\n"));
    goto ErrorExit1;
  }

  Status = GudSetStateCommit(GudDev, NULL);

  if (EFI_ERROR (Status)) {
    DEBUG((DEBUG_INFO,"UsbGudFbDriver: failed to set state commit\n"));
    goto ErrorExit1;
  }

ErrorExit1:
  FreePool(State);
  return Status;
}

EFI_STATUS
GudInit (
    IN OUT USB_GUD_FB_DEV *GudDev
  )
{
  EFI_STATUS Status;
  UINT8 ConnectorIndex = 0;
  EFI_USB_IO_PROTOCOL *UsbIo = GudDev->UsbIo;
  EFI_USB_ENDPOINT_DESCRIPTOR  EndpointDescriptor;

  //
  // Get interface descriptor
  //
  Status = UsbIo->UsbGetInterfaceDescriptor (
    UsbIo,
    &GudDev->InterfaceDescriptor
  );

  ASSERT_EFI_ERROR (Status);

  //
  // Find the bulk out endpoint
  //
  if (GudDev->InterfaceDescriptor.NumEndpoints == 0) {
    Status = EFI_UNSUPPORTED;
    goto ErrorExit;
  }

  Status = UsbIo->UsbGetEndpointDescriptor (
    UsbIo,
    0,
    &EndpointDescriptor
  );

  ASSERT_EFI_ERROR (Status);

  if (((EndpointDescriptor.Attributes & (BIT0 | BIT1)) != USB_ENDPOINT_BULK) ||
      ((EndpointDescriptor.EndpointAddress & USB_ENDPOINT_DIR_IN)))
  {
    Status = EFI_UNSUPPORTED;
    goto ErrorExit;
  }

  CopyMem(&GudDev->BulkEndpointDescriptor, &EndpointDescriptor, sizeof(EndpointDescriptor));

  //
  // Get GUD descriptor
  //
  Status = GudGetDescriptor (GudDev, &GudDev->VendorDescriptor);
  if (EFI_ERROR (Status)) 
  {
    DEBUG((DEBUG_INFO, "UsbGudFbDriver: failed to get descriptor\n"));
    goto ErrorExit;
  }

  DEBUG ((
    DEBUG_INFO,
    "UsbGudFbDriver: supported framebuffer size: %dx%d to %dx%d\n", 
    GudDev->VendorDescriptor.MinWidth,
    GudDev->VendorDescriptor.MinHeight,
    GudDev->VendorDescriptor.MaxWidth,
    GudDev->VendorDescriptor.MaxHeight
  ));

  //
  // Allocate transfer buffer
  //
  GudDev->TransferBuffer = AllocatePool(GudDev->VendorDescriptor.MaxBufferSize);
  ASSERT (GudDev->TransferBuffer);

  //
  // Enable GUD controller
  //
  Status = GudSetControllerEnable (GudDev, TRUE);
  if (EFI_ERROR (Status))
  {
    goto ErrorExit;
  }
  
  DEBUG((DEBUG_INFO,"UsbGudFbDriver: controller enabled\n"));

  //
  // Get all connectors
  //
  Status = GudGetConnectors(GudDev, GudDev->Connectors, &GudDev->ConnectorCount, NULL);

  if (EFI_ERROR (Status)) {
    DEBUG((DEBUG_INFO,"UsbGudFbDriver: failed to get connectors\n"));
    goto ErrorExit;
  }

  DEBUG((DEBUG_INFO,"UsbGudFbDriver: %d connectors detected\n", GudDev->ConnectorCount));
  
  // for (Index = 0; Index < ConnectorCount; Index++) {
  //   DEBUG((DEBUG_INFO,"UsbGudFbDriver: Con%d\ttype: 0x%02x\tflags: 0x%08x\n", Index, Connectors[Index].ConnectorType, Connectors[Index].Flags);
  // }

  //
  // Check connector status
  //
  UINT8 ConnectorStatus;
  Status = GudGetU8(GudDev, GUD_REQ_GET_CONNECTOR_STATUS, ConnectorIndex, &ConnectorStatus, NULL);

  if (EFI_ERROR (Status)) {
    DEBUG((DEBUG_INFO,"UsbGudFbDriver: failed to get connector status\n"));
    goto ErrorExit;
  }

  if ((ConnectorStatus & GUD_CONNECTOR_STATUS_CONNECTED_MASK) != GUD_CONNECTOR_STATUS_CONNECTED) {
    DEBUG((DEBUG_INFO,"UsbGudFbDriver: no display connected to connector %d\n", ConnectorIndex));
    goto ErrorExit;
  }

  //
  // Get connector modes
  //
  Status = GudGetConnectorModes(GudDev, ConnectorIndex, GudDev->Modes, &GudDev->ModeCount, NULL);
  
  if (EFI_ERROR (Status)) {
    DEBUG((DEBUG_INFO,"UsbGudFbDriver: failed to get connector modes\n"));
    goto ErrorExit;
  }

  DEBUG((DEBUG_INFO,"UsbGudFbDriver: Con%d has %d modes\n", ConnectorIndex, GudDev->ModeCount));

  // for (Index = 0; Index < ModesCount; Index++) {
  //   DEBUG((DEBUG_INFO,"UsbGudFbDriver: Mode%d\t%dx%d\tflags: 0x%08x\n", Index, Modes[Index].HDisplay, Modes[Index].VDisplay, Modes[Index].Flags);
  // }

  // GudDev->State.Mode = Modes[0];

  // TODO: get formats
  
  //
  // Get properties
  //
  Status = GudGetProperties(GudDev, GudDev->Properties, &GudDev->PropertyCount, NULL);
  
  if (EFI_ERROR (Status)) {
    DEBUG((DEBUG_INFO,"UsbGudFbDriver: failed to get properties, assuming no properties\n"));
    GudDev->PropertyCount = 0;
    Status = EFI_SUCCESS;
  }

  //
  // Get connector properties
  //
  Status = GudGetConnectorProperties(GudDev, ConnectorIndex, GudDev->Properties, &GudDev->ConnectorPropertyCount, NULL);
  
  if (EFI_ERROR (Status)) {
    DEBUG((DEBUG_INFO,"UsbGudFbDriver: failed to get connector properties, assuming no connector properties\n"));
    GudDev->ConnectorPropertyCount = 0;
    Status = EFI_SUCCESS;
  }

  //
  // Set state (mode/format)
  //
  // Status = GudSetMode(GudDev, 0);

  // if (EFI_ERROR (Status)) {
  //   goto ErrorExit;
  // }

  // DEBUG((DEBUG_INFO,"UsbGudFbDriver: using mode %dx%d\n", GudDev->Modes[GudDev->ModeIndex].HDisplay, GudDev->Modes[GudDev->ModeIndex].VDisplay));

ErrorExit:
  return Status;
}

BOOLEAN
GudDetect(
    IN OUT EFI_USB_IO_PROTOCOL *UsbIo
  )
{
  EFI_STATUS Status;
  EFI_USB_INTERFACE_DESCRIPTOR  InterfaceDescriptor;

  Status = UsbIo->UsbGetInterfaceDescriptor (
                  UsbIo,
                  &InterfaceDescriptor
                  );
    
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  if (InterfaceDescriptor.InterfaceClass == USB_INTERFACE_CLASS_VENDOR_SPECIFIC) {
    UINT8 InterfaceStringIndex = InterfaceDescriptor.Interface;
    CHAR16* Interface;
    
    Status = UsbIo->UsbGetStringDescriptor(UsbIo, LANG_ID_EN_US, InterfaceStringIndex, &Interface);
    
    if (EFI_ERROR (Status)) {
      return FALSE;
    }

    if (StrCmp(Interface, GUD_INTERFACE) != 0) {
      Status = EFI_UNSUPPORTED;
    }

    FreePool(Interface);

    return TRUE;
  }
  else {
    return FALSE;
  }
}

EFI_STATUS
GudFlushPart(
    IN OUT USB_GUD_FB_DEV *GudDev,
    IN UINTN X,
    IN UINTN Y,
    IN UINTN Width,
    IN UINTN Height
  )
{
  EFI_STATUS Status;
  UINTN BufferSize = Width * Height * USB_GUD_BPP;
  UINTN LineSize = Width * USB_GUD_BPP;
  UINTN Index;
  UINT32 UsbStatus;
  DEBUG((DEBUG_INFO,"UsbGudFbDriver: flushing part %dx%d@%d,%d\n", Width, Height, X, Y));

  for (Index = 0; Index < Height; Index++) {
    UINTN CurrentY = Index + Y;
    UINT8 *Base = USB_GUD_FB_AT(GudDev, X, CurrentY);
    CopyMem(GudDev->TransferBuffer + LineSize * Index, Base, LineSize);
  }

  if (!(GudDev->VendorDescriptor.Flags & GUD_DISPLAY_FLAG_FULL_UPDATE)) {
    // TODO: compression
    GUD_DRM_REQ_SET_BUFFER SetBufferReq = {
      .X = X,
      .Y = Y,
      .Height = Height,
      .Width = Width,
      .Length = BufferSize,
      .Compression = 0,
      .CompressedLength = 0
    };

    Status = GudSetBuffer(
      GudDev,
      &SetBufferReq,
      NULL
    );

    if (EFI_ERROR(Status)) {
      DEBUG((DEBUG_INFO,"UsbGudFbDriver: failed to set buffer\n"));
      goto ErrorExit;
    }
    DEBUG((DEBUG_INFO,"UsbGudFbDriver: set buffer ok\n"));
  }

  UINTN BytesTransferred = BufferSize;
  Status = GudDev->UsbIo->UsbBulkTransfer(
    GudDev->UsbIo,
    GudDev->BulkEndpointDescriptor.EndpointAddress,
    GudDev->TransferBuffer,
    &BytesTransferred,
    PcdGet32 (PcdUsbTransferTimeoutValue),
    &UsbStatus
  );

  DEBUG((DEBUG_INFO,"UsbGudFbDriver: bulk endpoint: %d\n", GudDev->BulkEndpointDescriptor.EndpointAddress));
  DEBUG((DEBUG_INFO,"UsbGudFbDriver: bytes transferred: %d\n", BytesTransferred));

ErrorExit:
  return Status;
}

EFI_STATUS
GudFlush(
    IN OUT USB_GUD_FB_DEV *GudDev,
    IN UINTN X,
    IN UINTN Y,
    IN UINTN Width,
    IN UINTN Height
  )
{
  EFI_STATUS Status;

  if (GudDev->VendorDescriptor.Flags & GUD_DISPLAY_FLAG_FULL_UPDATE) {
    X = 0;
    Y = 0;
    Width = GudDev->Modes[GudDev->ModeIndex].HDisplay;
    Height = GudDev->Modes[GudDev->ModeIndex].VDisplay;
  }

  UINTN PartHeight = GudDev->VendorDescriptor.MaxBufferSize / (Width * USB_GUD_BPP);
  ASSERT (PartHeight);
  UINTN CurrentY;

  for (CurrentY = Y; CurrentY < Y + Height; CurrentY += PartHeight) {
    Status = GudFlushPart(GudDev, X, CurrentY, Width, MIN(PartHeight, Y + Height - CurrentY));
    if (EFI_ERROR(Status)) {
      goto ErrorExit;
    }
  }

ErrorExit:
  return Status;
}


EFI_STATUS
GudQueueFlush(
    IN OUT USB_GUD_FB_DEV *GudDev,
    IN UINTN X,
    IN UINTN Y,
    IN UINTN Width,
    IN UINTN Height
  )
{
  return EFI_SUCCESS;
}
