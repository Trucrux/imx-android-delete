/****************************************************************************
*
*    Copyright (c) 2017 - 2021 VeriSilicon Inc. All Rights Reserved.
*    Copyright (c) 2007 - 2016 Intel Corporation. All Rights Reserved.
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

#ifndef _OBJECT_HEAP_H_
#define _OBJECT_HEAP_H_
/**
 * @file object_heap.h
 * @brief object heap interface.
 */
#include "hantro_mutext.h"

#define OBJECT_HEAP_OFFSET_MASK 0x7F000000
#define OBJECT_HEAP_ID_MASK 0x00FFFFFF

#define CONFIG_ID_OFFSET 0x01000000
#define CONTEXT_ID_OFFSET 0x02000000
#define SURFACE_ID_OFFSET 0x04000000
#define BUFFER_ID_OFFSET 0x08000000
#define IMAGE_ID_OFFSET 0x0a000000
#define SUBPIC_ID_OFFSET 0x10000000

#define ENC_HEVC_H264_ID_OFFSET 0x20000000
#define ENC_HEVC_H264_PRIV_RECON_BUFF_ID_OFFSET 0x20100000

#define ENC_JPEG_ID_OFFSET 0x21000000

#define DEC_HEVC_H264_ID_OFFSET 0x30000000
#define DEC_JPEG_ID_OFFSET 0x31000000
#define DEC_VP9_ID_OFFSET 0x32000000
#define DEC_MPEG2_ID_OFFSET 0x33000000

typedef struct object_base *object_base_p;
typedef struct object_heap *object_heap_p;

struct object_base {
  int id;
  int next_free;
};

struct object_heap {
  int object_size;
  int id_offset;
  int next_free;
  int heap_size;
  int heap_increment;
  _HANTROMutex mutex;
  void **bucket;
  int num_buckets;
};

typedef int object_heap_iterator;

/*
 * Return 0 on success, -1 on error
 */
int object_heap_init(object_heap_p heap, int object_size, int id_offset);

/*
 * Allocates an object
 * Returns the object ID on success, returns -1 on error
 */
int object_heap_allocate(object_heap_p heap);

/*
 * Lookup an allocated object by object ID
 * Returns a pointer to the object on success, returns NULL on error
 */
object_base_p object_heap_lookup(object_heap_p heap, int id);

/*
 * Iterate over all objects in the heap.
 * Returns a pointer to the first object on the heap, returns NULL if heap is
 * empty.
 */
object_base_p object_heap_first(object_heap_p heap, object_heap_iterator *iter);

/*
 * Iterate over all objects in the heap.
 * Returns a pointer to the next object on the heap, returns NULL if heap is
 * empty.
 */
object_base_p object_heap_next(object_heap_p heap, object_heap_iterator *iter);

/*
 * Frees an object
 */
void object_heap_free(object_heap_p heap, object_base_p obj);

/*
 * Destroys a heap, the heap must be empty.
 */
void object_heap_destroy(object_heap_p heap);

#endif /* _OBJECT_HEAP_H_ */
