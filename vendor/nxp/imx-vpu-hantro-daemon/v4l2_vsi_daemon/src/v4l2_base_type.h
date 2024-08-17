/****************************************************************************
*
*    Copyright 2019 - 2020 VeriSilicon Inc. All Rights Reserved.
*
*    Permission is hereby granted, free of charge, to any person obtaining
*    a copy of this software and associated documentation files (the
*    'Software'), to deal in the Software without restriction, including
*    without limitation the rights to use, copy, modify, merge, publish,
*    distribute, sub license, and/or sell copies of the Software, and to
*    permit persons to whom the Software is furnished to do so, subject
*    to the following conditions:
*
*    The above copyright notice and this permission notice (including the
*    next paragraph) shall be included in all copies or substantial
*    portions of the Software.
*
*    THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
*    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
*    IN NO EVENT SHALL VIVANTE AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
*    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
*    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
*    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*****************************************************************************/

#ifndef V4L2_BASE_TYPE_H
#define V4L2_BASE_TYPE_H

#ifndef V4L2_DRIVER
#include <stdint.h>
#include <stdio.h>
#ifndef NDEBUG
#include <assert.h>
#endif
#endif

#if 0
typedef int8_t    i8;
typedef uint8_t   u8;
typedef int16_t   i16;
typedef uint16_t  u16;
typedef int32_t   i32;
typedef uint32_t  u32;
typedef int64_t   i64;
typedef uint64_t  u64;
typedef size_t    ptr_t;

typedef       short               Short;
typedef       int                 Int;
typedef       unsigned int        UInt;
#endif

typedef long long unsigned int vpu_addr_t;  // same as kernel's dma_addr_t

#ifndef INLINE
#define INLINE inline
#endif

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else /*  */
#define NULL ((void *)0)
#endif /*  */
#endif

/* ASSERT */
#ifndef ASSERT
#ifndef NDEBUG
#define ASSERT(x) assert(x)
#else
#define ASSERT(x)
#endif
#endif

#endif  // V4L2_BASE_TYPE_H
