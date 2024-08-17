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

#ifndef _VSI_COMPILER_H_
#define _VSI_COMPILER_H_

/**
 * Function inlining
 */
#if defined(__GNUC__)
#ifndef INLINE
#define INLINE __inline__
#endif
#elif (__STDC_VERSION__ >= 199901L) /* C99 */
#ifndef INLINE
#define INLINE inline
#endif
#else
#ifndef INLINE
#define INLINE
#endif
#endif

/**
 * Function visibility
 */
#if defined(__GNUC__)
#define DLL_HIDDEN __attribute__((visibility("hidden")))
#define DLL_EXPORT __attribute__((visibility("default")))
#else
#define DLL_HIDDEN
#define DLL_EXPORT
#endif

#endif /* _VSI_COMPILER_H_ */
