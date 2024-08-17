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

#ifndef _HANTRO_MUTEX_H_
#define _HANTRO_MUTEX_H_

#include "vsi_compiler.h"

#define PTHREADS
#if defined PTHREADS
#include <pthread.h>

typedef pthread_mutex_t _HANTROMutex;

static INLINE void _hantroInitMutex(_HANTROMutex *m) {
  pthread_mutex_init(m, NULL);
}

static INLINE void _hantroDestroyMutex(_HANTROMutex *m) {
  pthread_mutex_destroy(m);
}

static INLINE void _hantroLockMutex(_HANTROMutex *m) { pthread_mutex_lock(m); }

static INLINE void _hantroUnlockMutex(_HANTROMutex *m) {
  pthread_mutex_unlock(m);
}

#define _HANTRO_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define _HANTRO_DECLARE_MUTEX(m) _HANTROMutex m = _HANTRO_MUTEX_INITIALIZER

#else

typedef int _HANTROMutex;
static INLINE void _hantroInitMutex(_HANTROMutex *m) { (void)m; }
static INLINE void _hantroDestroyMutex(_HANTROMutex *m) { (void)m; }
static INLINE void _hantroLockMutex(_HANTROMutex *m) { (void)m; }
static INLINE void _hantroUnlockMutex(_HANTROMutex *m) { (void)m; }

#define _HANTRO_MUTEX_INITIALIZER 0
#define _HANTRO_DECLARE_MUTEX(m) _HANTROMutex m = _HANTRO_MUTEX_INITIALIZER

#endif

#endif /* _HANTRO_MUTEX_H_ */
