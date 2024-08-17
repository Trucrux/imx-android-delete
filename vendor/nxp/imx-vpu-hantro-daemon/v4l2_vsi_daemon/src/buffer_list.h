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

#ifndef BUFFER_LIST_H
#define BUFFER_LIST_H
#include <string.h>
#include "command_defines.h"
#define MAX_BUFFER_SIZE 32

typedef struct BUFFER {
  union {
    v4l2_daemon_enc_params enc_cmd;
    v4l2_daemon_dec_params dec_cmd;
  };
  uint32_t frame_display_id;
} BUFFER;

typedef struct BUFFERLIST {
  BUFFER** list;
  uint32_t size;  // list size
  uint32_t capacity;
} BUFFERLIST;

int bufferlist_init(BUFFERLIST* list, uint32_t size);
void bufferlist_destroy(BUFFERLIST* list);
uint32_t bufferlist_get_size(BUFFERLIST* list);
uint32_t bufferlist_get_capacity(BUFFERLIST* list);
BUFFER** bufferlist_at(BUFFERLIST* list, uint32_t i);
BUFFER* bufferlist_find_buffer(BUFFERLIST* list, uint32_t next_pic_id,
                               uint32_t* num);
int bufferlist_push_buffer(BUFFERLIST* list, BUFFER* buff);
void bufferlist_remove(BUFFERLIST* list, uint32_t i);
void bufferlist_clear(BUFFERLIST* list);
BUFFER* bufferlist_get_buffer(BUFFERLIST* list);
int bufferlist_pop_buffer(BUFFERLIST* list);
BUFFER* bufferlist_get_tail(BUFFERLIST* list);
BUFFER* bufferlist_get_head(BUFFERLIST* list);
void bufferlist_flush(BUFFERLIST* list);

#endif
