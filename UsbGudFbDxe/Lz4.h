#ifndef _EFI_LZ4_H_
#define _EFI_LZ4_H_

#include <Uefi.h>
#include "lz4/lz4.h"

EFI_STATUS
Lz4Compress(
    IN CONST UINT8  *DataIn,
    IN UINTN  DataInSize,
    IN UINT8  *DataOut,
    UINTN  DataOutCapacity,
    OUT UINTN *DataOutSize
  )
{
  *DataOutSize = LZ4_compress_default((const char*)DataIn, (char*)DataOut, DataInSize, DataOutCapacity);

  if (*DataOutSize == 0) {
    return EFI_BUFFER_TOO_SMALL;
  }
  else {
    return EFI_SUCCESS;
  }
}

#endif