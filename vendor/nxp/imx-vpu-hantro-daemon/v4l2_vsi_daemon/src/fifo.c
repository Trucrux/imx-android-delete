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

#include "fifo.h"

#include "vsi_daemon_debug.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

/* Container for instance. */
struct Fifo {
  sem_t cs_semaphore;    /* Semaphore for critical section. */
  sem_t read_semaphore;  /* Semaphore for readers. */
  sem_t write_semaphore; /* Semaphore for writers. */
  uint32_t num_of_slots;
  uint32_t num_of_objects;
  uint32_t tail_index;
  FifoObject* nodes;
  uint32_t abort;
};

enum FifoRet FifoInit(uint32_t num_of_slots, FifoInst* instance) {
  struct Fifo* inst = calloc(1, sizeof(struct Fifo));
  if (inst == NULL) return FIFO_ERROR_MEMALLOC;
  inst->num_of_slots = num_of_slots;
  /* Allocate memory for the objects. */
  inst->nodes = calloc(num_of_slots, sizeof(FifoObject));
  if (inst->nodes == NULL) {
    free(inst);
    return FIFO_ERROR_MEMALLOC;
  }
  /* Initialize binary critical section semaphore. */
  sem_init(&inst->cs_semaphore, 0, 1);
  /* Then initialize the read and write semaphores. */
  sem_init(&inst->read_semaphore, 0, 0);
  sem_init(&inst->write_semaphore, 0, num_of_slots);
  *instance = inst;
  return FIFO_OK;
}

enum FifoRet FifoPush(FifoInst inst, FifoObject object, enum FifoException e) {
  struct Fifo* instance = (struct Fifo*)inst;
  int32_t value;

  sem_getvalue(&instance->read_semaphore, &value);
  if ((e == FIFO_EXCEPTION_ENABLE) &&
      ((uint32_t)value == instance->num_of_slots) &&
      (instance->num_of_objects == instance->num_of_slots)) {
    return FIFO_FULL;
  }

  sem_wait(&instance->write_semaphore);
  sem_wait(&instance->cs_semaphore);
  instance->nodes[(instance->tail_index + instance->num_of_objects) %
                  instance->num_of_slots] = object;
  instance->num_of_objects++;
  sem_post(&instance->cs_semaphore);
  sem_post(&instance->read_semaphore);
  return FIFO_OK;
}

enum FifoRet FifoPop(FifoInst inst, FifoObject* object, enum FifoException e) {
  struct Fifo* instance = (struct Fifo*)inst;
  int32_t value;

  sem_getvalue(&instance->write_semaphore, &value);
  if ((e == FIFO_EXCEPTION_ENABLE) &&
      ((uint32_t)value == instance->num_of_slots) &&
      (instance->num_of_objects == 0)) {
    return FIFO_EMPTY;
  }

  sem_wait(&instance->read_semaphore);
  sem_wait(&instance->cs_semaphore);

  if (instance->abort) return FIFO_ABORT;

  *object = instance->nodes[instance->tail_index % instance->num_of_slots];
  instance->tail_index++;
  instance->num_of_objects--;
  sem_post(&instance->cs_semaphore);
  sem_post(&instance->write_semaphore);
  return FIFO_OK;
}

uint32_t FifoCount(FifoInst inst) {
  uint32_t count;
  struct Fifo* instance = (struct Fifo*)inst;
  sem_wait(&instance->cs_semaphore);
  count = instance->num_of_objects;
  sem_post(&instance->cs_semaphore);
  return count;
}

void FifoRelease(FifoInst inst) {
  struct Fifo* instance = (struct Fifo*)inst;
#ifdef HEVC_EXT_BUF_SAFE_RELEASE
  ASSERT(instance->num_of_objects == 0);
#endif
  sem_wait(&instance->cs_semaphore);
  sem_destroy(&instance->cs_semaphore);
  sem_destroy(&instance->read_semaphore);
  sem_destroy(&instance->write_semaphore);
  free(instance->nodes);
  free(instance);
}

void FifoSetAbort(FifoInst inst) {
  struct Fifo* instance = (struct Fifo*)inst;
  if (instance == NULL) return;
  instance->abort = 1;
  sem_post(&instance->cs_semaphore);
  sem_post(&instance->read_semaphore);
}

void FifoClearAbort(FifoInst inst) {
  struct Fifo* instance = (struct Fifo*)inst;
  if (instance == NULL) return;
  instance->abort = 0;
}
