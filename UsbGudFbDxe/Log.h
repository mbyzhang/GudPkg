#ifndef _EFI_GUD_LOG_H_
#define _EFI_GUD_LOG_H_

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>

// #define GUD_LOG_TO_STDOUT

#ifdef GUD_LOG_TO_STDOUT
#define GUD_LOG(msg, ...) Print(L"UsbGudFbDxe: " msg "\n", ##__VA_ARGS__)
#else
#define GUD_LOG(msg, ...) DEBUG ((DEBUG_INFO, "UsbGudFbDxe: " msg "\n", ##__VA_ARGS__))
#endif

#endif
