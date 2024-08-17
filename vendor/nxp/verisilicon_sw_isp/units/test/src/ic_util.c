/******************************************************************************\
|* Copyright (c) 2020 by VeriSilicon Holdings Co., Ltd. ("VeriSilicon")       *|
|* All Rights Reserved.                                                       *|
|*                                                                            *|
|* The material in this file is confidential and contains trade secrets of    *|
|* of VeriSilicon.  This is proprietary information owned or licensed by      *|
|* VeriSilicon.  No part of this work may be disclosed, reproduced, copied,   *|
|* transmitted, or used in any way for any purpose, without the express       *|
|* written permission of VeriSilicon.                                         *|
|*                                                                            *|
\******************************************************************************/

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "ic_util.h"

static int readHeader(FILE *fp, uint32_t *width, uint32_t *height,
                      uint32_t *stride, PicBufType_t *type,
                      PicBufLayout_t *layout, uint64_t *timeStamp) {
    char line[256];
    char *delimit = " \t\n";
    char *start, *end;
    size_t n, m;
    int step = 0;

    int ret = 0;

    while (1) {
        char *pos;
        size_t len;

        if (!fgets(line, sizeof(line), fp)) {
            printf("Failed to read file.\n");
            return -EOF;
        }

        if (line[0] == '#') {
            start = strstr(line, "<Type>");
            if (start) {
                end = strstr(line, "</Type>");
                if (end) {
                    *type = (PicBufType_t)atoi(start + 6);
                }
            }

            start = strstr(line, "<Layout>");
            if (start) {
                end = strstr(line, "</Layout>");
                if (end) {
                    *layout = (PicBufLayout_t)atoi(start + 8);
                }
            }

            start = strstr(line, "<TimeStampUs>");
            if (start) {
                end = strstr(line, "</TimeStampUs>");
                if (end) {
                    *timeStamp = (uint64_t)atoll(start + 13);
                }
            }

            continue;
        }

        pos = line;
        len = strlen(line);

        /* Check the magic number. */
        if (step == 0) {
            n = strspn(pos, delimit);
            if (n >= len)
                continue;

            start = pos + n;

            m = strcspn(start, delimit);
            if (m == 0)
                continue;

            if (strncmp(start, "P5", m)) {
                printf("Wrong magic number.\n");

                return -EINVAL;
            }

            pos += n + m;

            if (len < n + m)
                len = 0;
            else
                len -= n + m;

            step = 1;
        }

        /* Get width. */
        if (step == 1) {
            n = strspn(pos, delimit);
            if (n >= len)
                continue;

            start = pos + n;

            m = strcspn(start, delimit);

            if (m == 0)
                continue;

            *width = atoi(start);

            pos += n + m;

            if (len < n + m)
                len = 0;
            else
                len -= n + m;

            step = 2;
        }

        /* Get height. */
        if (step == 2) {
            n = strspn(pos, delimit);
            if (n >= len)
                continue;

            start = pos + n;

            m = strcspn(start, delimit);

            if (m == 0)
                continue;

            *height = atoi(start);

            pos += n + m;

            if (len < n + m)
                len = 0;
            else
                len -= n + m;

            step = 3;
        }

        if (step == 3) {
            n = strspn(pos, delimit);
            if (n >= len)
                continue;

            start = pos + n;

            m = strcspn(start, delimit);

            if (m == 0)
                continue;

            *stride = atoi(start) > 255 ? *width << 1 : *width;

            break;
        }
    }

    printf("width: %u, height: %u, stride: %u, type: %d, layout: %d, "
           "timeStamp: %" PRIu64 ".\n",
           *width, *height, *stride, *type, *layout, *timeStamp);

    return ret;
}

static int readData(FILE *fp, uint32_t width, uint32_t height, uint32_t stride,
                    uint8_t *pBuffer) {
    size_t num;

    if (!fp || !pBuffer) {
        return -EINVAL;
    }

    num = height;

    if (fread(pBuffer, stride, num, fp) < num) {
        return errno;
    }

    return 0;
}

int icoUtil_LoadPGMRaw(char *pFileName, PicBufMetaData_t *pPicBuf) {
    FILE *fp = NULL;
    uint32_t width = 0, height = 0, stride = 0;
    PicBufType_t type = PIC_BUF_TYPE_INVALID;
    PicBufLayout_t layout = PIC_BUF_LAYOUT_INVALID;
    uint64_t timeStamp = 0;
    uint8_t *pBuffer = NULL;

    int ret = 0;

    if (!pFileName) {
        printf("The file name is NULL.\n");
        return -EINVAL;
    }

    fp = fopen(pFileName, "rb");
    if (!fp) {
        printf("Failed to open file %s: %s.\n", pFileName, strerror(errno));
        return errno;
    }

    ret = readHeader(fp, &width, &height, &stride, &type, &layout, &timeStamp);
    if (ret < 0) {
        printf("Failed to read the header.\n");
        goto err;
    }

    pBuffer = malloc(stride * height);
    if (!pBuffer) {
        printf("Failed to allocate memory.\n");
        goto err;
    }

    switch (type) {
    case PIC_BUF_TYPE_RAW8:
    case PIC_BUF_TYPE_RAW10:
    case PIC_BUF_TYPE_RAW12:
    case PIC_BUF_TYPE_RAW16:
        ret = readData(fp, width, height, stride, pBuffer);
        if (ret < 0) {
            printf("Failed to read the data.\n");
            goto err;
        }
        break;
    default:
        printf("%s(): unknown image format.\n", __func__);
        goto err;
    }

    pPicBuf->Type = type;
    pPicBuf->Layout = layout;
    pPicBuf->TimeStampUs = timeStamp;

    pPicBuf->Data.raw.pData = pBuffer;
    pPicBuf->Data.raw.BaseAddress = ~0U;
    pPicBuf->Data.raw.PicWidthPixel = width;
    pPicBuf->Data.raw.PicWidthBytes = stride;
    pPicBuf->Data.raw.PicHeightPixel = height;

    fclose(fp);

    return 0;

err:
    if (pBuffer) {
        free(pBuffer);
    }

    if (fp) {
        fclose(fp);
    }

    return ret;
}
