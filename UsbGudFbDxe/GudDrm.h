#ifndef _EFI_GUD_DRM_H_
#define _EFI_GUD_DRM_H_

#include <Uefi.h>

#pragma pack(1)

#define GUD_DISPLAY_MAGIC 0x1d50614d

#define GUD_COMPRESSION_LZ4 BIT0

#define GUD_DISPLAY_FLAG_STATUS_ON_SET BIT0
#define GUD_DISPLAY_FLAG_FULL_UPDATE BIT1

typedef struct {
  UINT32 Magic;
  UINT8 Version;
  UINT32 Flags;
  UINT8 Compression;
  UINT32 MaxBufferSize;
  UINT32 MinWidth;
  UINT32 MaxWidth;
  UINT32 MinHeight;
  UINT32 MaxHeight;
} GUD_DRM_USB_VENDOR_DESCRIPTOR;

typedef struct {
    UINT16 Prop;
    UINT64 Val;
} GUD_DRM_REQ_PROPERTY;

typedef struct {
    UINT32 Clock;
    UINT16 HDisplay;
    UINT16 HSyncStart;
    UINT16 HSyncEnd;
    UINT16 HTotal;
    UINT16 VDisplay;
    UINT16 VSyncStart;
    UINT16 VSyncEnd;
    UINT16 VTotal;
    UINT32 Flags;
} GUD_DRM_REQ_DISPLAY_MODE;

#define GUD_DISPLAY_MODE_FLAG_PREFERRED BIT10

typedef struct {
    UINT8 ConnectorType;
    UINT32 Flags;
} GUD_DRM_REQ_CONNECTOR_DESCRIPTOR;

#define GUD_CONNECTOR_TYPE_PANEL 0
#define GUD_CONNECTOR_FLAGS_POLL_STATUS BIT0
#define GUD_CONNECTOR_FLAGS_INTERLACE BIT1
#define GUD_CONNECTOR_FLAGS_DOUBLESCAN BIT2

typedef struct {
    UINT32 X;
    UINT32 Y;
    UINT32 Width;
    UINT32 Height;
    UINT32 Length;
    UINT8 Compression;
    UINT32 CompressedLength;
} GUD_DRM_REQ_SET_BUFFER;

#define GUD_PROPERTIES_MAX_NUM 32

typedef struct {
    GUD_DRM_REQ_DISPLAY_MODE Mode;
    UINT8 Format;
    UINT8 Connector;
    GUD_DRM_REQ_PROPERTY Properties[0];
} GUD_DRM_REQ_SET_STATE;

/* USB Control requests: */

/* Get status from the last GET/SET control request. Value is u8. */
#define GUD_REQ_GET_STATUS				0x00
  /* Status values: */
  #define GUD_STATUS_OK				0x00
  #define GUD_STATUS_BUSY			0x01
  #define GUD_STATUS_REQUEST_NOT_SUPPORTED	0x02
  #define GUD_STATUS_PROTOCOL_ERROR		0x03
  #define GUD_STATUS_INVALID_PARAMETER		0x04
  #define GUD_STATUS_ERROR			0x05

/* Get display descriptor as a &gud_display_descriptor_req */
#define GUD_REQ_GET_DESCRIPTOR				0x01

/* Get supported pixel formats as a byte array of GUD_PIXEL_FORMAT_* */
#define GUD_REQ_GET_FORMATS				0x40
  #define GUD_FORMATS_MAX_NUM			32
  #define GUD_PIXEL_FORMAT_R1			0x01 /* 1-bit monochrome */
  #define GUD_PIXEL_FORMAT_R8			0x08 /* 8-bit greyscale */
  #define GUD_PIXEL_FORMAT_XRGB1111		0x20
  #define GUD_PIXEL_FORMAT_RGB332		0x30
  #define GUD_PIXEL_FORMAT_RGB565		0x40
  #define GUD_PIXEL_FORMAT_RGB888		0x50
  #define GUD_PIXEL_FORMAT_XRGB8888		0x80
  #define GUD_PIXEL_FORMAT_ARGB8888		0x81

/*
 * Get supported properties that are not connector propeties as a &gud_property_req array.
 * gud_property_req.val often contains the initial value for the property.
 */
#define GUD_REQ_GET_PROPERTIES				0x41
  #define GUD_PROPERTIES_MAX_NUM		32

/* Connector requests have the connector index passed in the wValue field */

/* Get connector descriptors as an array of &gud_connector_descriptor_req */
#define GUD_REQ_GET_CONNECTORS				0x50
  #define GUD_CONNECTORS_MAX_NUM		32

/*
 * Get properties supported by the connector as a &gud_property_req array.
 * gud_property_req.val often contains the initial value for the property.
 */
#define GUD_REQ_GET_CONNECTOR_PROPERTIES		0x51
  #define GUD_CONNECTOR_PROPERTIES_MAX_NUM	32

/*
 * Issued when there's a TV_MODE property present.
 * Gets an array of the supported TV_MODE names each entry of length
 * GUD_CONNECTOR_TV_MODE_NAME_LEN. Names must be NUL-terminated.
 */
#define GUD_REQ_GET_CONNECTOR_TV_MODE_VALUES		0x52
  #define GUD_CONNECTOR_TV_MODE_NAME_LEN	16
  #define GUD_CONNECTOR_TV_MODE_MAX_NUM		16

/* When userspace checks connector status, this is issued first, not used for poll requests. */
#define GUD_REQ_SET_CONNECTOR_FORCE_DETECT		0x53

/*
 * Get connector status. Value is u8.
 *
 * Userspace will get a HOTPLUG uevent if one of the following is true:
 * - Connection status has changed since last
 * - CHANGED is set
 */
#define GUD_REQ_GET_CONNECTOR_STATUS			0x54
  #define GUD_CONNECTOR_STATUS_DISCONNECTED	0x00
  #define GUD_CONNECTOR_STATUS_CONNECTED	0x01
  #define GUD_CONNECTOR_STATUS_UNKNOWN		0x02
  #define GUD_CONNECTOR_STATUS_CONNECTED_MASK	0x03
  #define GUD_CONNECTOR_STATUS_CHANGED		BIT(7)

/*
 * Display modes can be fetched as either EDID data or an array of &gud_display_mode_req.
 *
 * If GUD_REQ_GET_CONNECTOR_MODES returns zero, EDID is used to create display modes.
 * If both display modes and EDID are returned, EDID is just passed on to userspace
 * in the EDID connector property.
 */

/* Get &gud_display_mode_req array of supported display modes */
#define GUD_REQ_GET_CONNECTOR_MODES			0x55
  #define GUD_CONNECTOR_MAX_NUM_MODES		128

/* Get Extended Display Identification Data */
#define GUD_REQ_GET_CONNECTOR_EDID			0x56
  #define GUD_CONNECTOR_MAX_EDID_LEN		2048

/* Set buffer properties before bulk transfer as &gud_set_buffer_req */
#define GUD_REQ_SET_BUFFER				0x60

/* Check display configuration as &gud_state_req */
#define GUD_REQ_SET_STATE_CHECK				0x61

/* Apply the previous STATE_CHECK configuration */
#define GUD_REQ_SET_STATE_COMMIT			0x62

/* Enable/disable the display controller, value is u8: 0/1 */
#define GUD_REQ_SET_CONTROLLER_ENABLE			0x63

/* Enable/disable display/output (DPMS), value is u8: 0/1 */
#define GUD_REQ_SET_DISPLAY_ENABLE			0x64


#pragma pack()

#endif
