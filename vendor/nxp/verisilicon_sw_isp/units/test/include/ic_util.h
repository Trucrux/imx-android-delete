#ifndef __IC_UTIL_H__
#define __IC_UTIL_H__

#include <common/picture_buffer.h>

int
icoUtil_LoadPGMRaw(
    char *pFileName,
    PicBufMetaData_t *pPicBuf
    );

#endif /* __IC_UTIL_H__ */
