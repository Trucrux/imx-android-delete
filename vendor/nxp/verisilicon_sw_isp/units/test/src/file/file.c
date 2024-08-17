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

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REPORT_DETAIL 0

int FLE_CompareBuffer(FILE *pF, FILE *pF2, unsigned int *pPercent) {
    assert(pF);
    assert(pF2);

    int ret = 0, ret2 = 0;

    unsigned int addr = 0, value = 0, addr2 = 0, value2 = 0;

    unsigned int line = 0;
    unsigned int equalCount = 0;

    for (;; line++) {
        ret = fscanf(pF, "%08x %08x", &addr, &value);
        if (ret <= 0) {
            break;
        }

        ret2 = fscanf(pF2, "%08x %08x", &addr2, &value2);
        if (ret2 <= 0) {
            break;
        }

        if (value != value2) {
#if REPORT_DETAIL
            if (isFirstReport || isLastEqual) {
                isFirstReport = false;

                printf("%d: 0x%08X 0x%02X != 0x%08X 0x%02X \n", line + 1, addr,
                       value, addr2, value2);
            }

            isLastEqual = false;
#endif
        } else {
            equalCount++;

#if REPORT_DETAIL
            if (isFirstReport || !isLastEqual) {
                isFirstReport = false;

                printf("%d: 0x%08X 0x%02X == 0x%08X 0x%02X \n", line + 1, addr,
                       value, addr2, value2);
            }

            isLastEqual = true;
#endif
        }
    }

    if (pPercent) {
        *pPercent = equalCount * 100 / line;
    }

    if (equalCount == line) {
        return 0;
    } else {
        return -1;
    }
}

int FLE_BinaryCompare(FILE *pF, FILE *pF2) {
    assert(pF);
    assert(pF2);

    size_t result = 0, result2 = 0;
    char c = 0, c2 = 0;

    for (;;) {
        result = fread(&c, 1, 1, pF);
        result2 = fread(&c2, 1, 1, pF2);

        if (result != result2) {
            return -1;
        } else if (result < 1) {
            return 0;
        } else if (c != c2) {
            return -1;
        }
    }

    return 0;
}

int FLE_Size(const char *pFile, unsigned int *pSize) {
    assert(pFile);

    FILE *pF = fopen(pFile, "r");
    if (!pF) {
        return -ENFILE;
    }

    fseek(pF, 0L, SEEK_END);

    if (pSize) {
        *pSize = ftell(pF);
    }

    fclose(pF);

    return 0;
}

int FLE_Lines(const char *pFile, unsigned int *pCount) {
    assert(pFile);

    FILE *pF = fopen(pFile, "r");
    if (!pF) {
        return -ENFILE;
    }

    assert(pCount);
    *pCount = 0;

    int ch = 0;

    do {
        // Input character from both files
        ch = fgetc(pF);

        // Increment pLine
        if (ch == '\n') {
            *pCount += 1;
        }
    } while (ch != EOF);

    fclose(pF);

    return 0;
}

int FLE_GetBufferBaseAddress(const char *pFile, unsigned int *pAddr) {
    assert(pFile);

    FILE *pF = fopen(pFile, "r");
    if (!pF) {
        return -ENFILE;
    }

    unsigned int value = 0;

    assert(pAddr);

    if (fscanf(pF, "%08x %08x", pAddr, &value) <= 0) {
        fclose(pF);
        return -1;
    }

    fclose(pF);

    return 0;
}

int FLE_GetBufferLastAddress(const char *pFile, unsigned int *pAddr) {
    assert(pFile);

    FILE *pF = fopen(pFile, "r");
    if (!pF) {
        return -ENFILE;
    }

    char *pStr = NULL;
    char str[50] = {0};

    do {
        pStr = fgets(str, sizeof(str), pF);
    } while (pStr);

    assert(pAddr);

    unsigned int value = 0;

    if (sscanf(str, "%08x %08x", pAddr, &value) > 0) {
    }

    fclose(pF);

    return 0;
}

int FLE_GetBufferLength(const char *pFile, unsigned int *pLength) {
    int ret = 0;

    unsigned int baseAddr = 0;

    ret = FLE_GetBufferBaseAddress(pFile, &baseAddr);
    if (ret != 0) {
        return ret;
    }

    unsigned int lastAddr = 0;

    ret = FLE_GetBufferLastAddress(pFile, &lastAddr);
    if (ret != 0) {
        return ret;
    }

    assert(lastAddr > baseAddr);

    assert(pLength);

    *pLength = lastAddr - baseAddr + 1;

    return ret;
}


#define BUFFER_SIZE 256
int FLE_CopyFile(const char *sourceFileNameWithPath, const char *targetFileNameWithPath)
{
	FILE *fpR, *fpW;
	char buffer[BUFFER_SIZE];
	int lenR, lenW;
	if ((fpR = fopen(sourceFileNameWithPath, "r")) == NULL)
	{
		printf("The file '%s' can not be opened! \n", sourceFileNameWithPath);
		return -1;
	}
	if ((fpW = fopen(targetFileNameWithPath, "w")) == NULL)
	{
		printf("The file '%s' can not be opened! \n", targetFileNameWithPath);
		fclose(fpR);
		return -1;
	}

	memset(buffer, 0, BUFFER_SIZE);
	while ((lenR = fread(buffer, 1, BUFFER_SIZE, fpR)) > 0)
	{
		if ((lenW = fwrite(buffer, 1, lenR, fpW)) != lenR)
		{
			printf("Write to file '%s' failed!\n", targetFileNameWithPath);
			fclose(fpR);
			fclose(fpW);
			return -1;
		}
		memset(buffer, 0, BUFFER_SIZE);
	}

	fclose(fpR);
	fclose(fpW);
	return 0;
}
