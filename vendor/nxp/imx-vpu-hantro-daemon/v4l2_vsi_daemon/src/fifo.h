/****************************************************************************
*
*    Copyright (c) 2015-2021, VeriSilicon Inc. All rights reserved.
*    Copyright (c) 2011-2014, Google Inc. All rights reserved.
*    Copyright (c) 2007-2010, Hantro OY. All rights reserved.
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

#ifndef __FIFO_H__
#define __FIFO_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file fifo.h
 * @brief Fifo interface.
 */

#include "v4l2_base_type.h"

#define FIFO_DATATYPE void*

/* FIFO_DATATYPE must be defined to hold specific type of objects. If it is not
 * defined, we need to report an error. */
#ifndef FIFO_DATATYPE
#error "You must define FIFO_DATATYPE to use this module."
#endif /* FIFO_DATATYPE */

#define MAX_FIFO_CAPACITY 100

typedef FIFO_DATATYPE FifoObject;

/* Possible return values. */
enum FifoRet {
  FIFO_OK,             /* Operation was successful. */
  FIFO_ERROR_MEMALLOC, /* Failed due to memory allocation error. */
  FIFO_EMPTY,
  FIFO_FULL,
  FIFO_NOK,
  FIFO_ABORT = 0x7FFFFFFF
};

enum FifoException { FIFO_EXCEPTION_DISABLE, FIFO_EXCEPTION_ENABLE };

typedef void* FifoInst;

/* FifoInit initializes the queue.
 * |num_of_slots| defines how many slots to reserve at maximum.
 * |instance| is output parameter holding the instance. */
enum FifoRet FifoInit(uint32_t num_of_slots, FifoInst* instance);

/* FifoPush pushes an object to the back of the queue. Ownership of the
 * contained object will be moved from the caller to the queue. Returns OK
 * if the object is successfully pushed into fifo.
 *
 * |inst| is the instance push to.
 * |object| holds the pointer to the object to push into queue.
 * |exception_enable| enable FIFO_FULL return value */
enum FifoRet FifoPush(FifoInst inst, FifoObject object,
                      enum FifoException exception_enable);

/* FifoPop returns object from the front of the queue. Ownership of the popped
 * object will be moved from the queue to the caller. Returns OK if the object
 * is successfully popped from the fifo.
 *
 * |inst| is the instance to pop from.
 * |object| holds the pointer to the object popped from the queue.
 * |exception_enable| enable FIFO_EMPTY return value */
enum FifoRet FifoPop(FifoInst inst, FifoObject* object,
                     enum FifoException exception_enable);

/* Ask how many objects there are in the fifo. */
uint32_t FifoCount(FifoInst inst);

/* FifoRelease releases and deallocated queue. User needs to make sure the
 * queue is empty and no threads are waiting in FifoPush or FifoPop.
 * |inst| is the instance to release. */
void FifoRelease(FifoInst inst);
void FifoSetAbort(FifoInst inst);
void FifoClearAbort(FifoInst inst);

#ifdef __cplusplus
}
#endif

#endif /* __FIFO_H__ */
