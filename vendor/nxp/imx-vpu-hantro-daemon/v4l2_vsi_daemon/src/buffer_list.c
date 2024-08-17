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
#include "buffer_list.h"
#include "stdlib.h"
#include "vsi_daemon_debug.h"
/**
 * @brief bufferlist_init(), initial a buffer list with specified slot num.
 * @param BUFFERLIST* list: list handle.
 * @param uint32_t size: slot num.
 * @return int, 0: succeed; Other value: failed.
 */
int bufferlist_init(BUFFERLIST* list, uint32_t size) {
  ASSERT(list);
  list->list = (BUFFER**)malloc(sizeof(BUFFER*) * size);
  if (!list->list) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "Error insufficient resources./n");
    return -1;
  }

  memset(list->list, 0, sizeof(BUFFER*) * size);
  list->size = 0;
  list->capacity = size;
  return 0;
}

/**
 * @brief bufferlist_destroy(), destroy a buffer list.
 * @param BUFFERLIST* list: list handle.
 * @return none.
 */
void bufferlist_destroy(BUFFERLIST* list) {
  ASSERT(list);
  if (list->list) free(list->list);
  memset(list, 0, sizeof(BUFFERLIST));
}

/**
 * @brief bufferlist_get_size(), get available slot num.
 * @param BUFFERLIST* list: list handle.
 * @return uint32_t, available slot num.
 */
uint32_t bufferlist_get_size(BUFFERLIST* list) {
  ASSERT(list);
  return list->size;
}

/**
 * @brief bufferlist_get_size(), get total slot num.
 * @param BUFFERLIST* list: list handle.
 * @return uint32_t, total slot num.
 */
uint32_t bufferlist_get_capacity(BUFFERLIST* list) {
  ASSERT(list);
  return list->capacity;
}

/**
 * @brief bufferlist_at(), get buffer from list with specified slot id.
 * @param BUFFERLIST* list: list handle.
 * @param uint32_t i: the specified slot id.
 * @return BUFFER*, handle of got buffer.
 */
BUFFER** bufferlist_at(BUFFERLIST* list, uint32_t i) {
  ASSERT(list);
  ASSERT(i < list->size);
  return &list->list[i];
}

/**
 * @brief bufferlist_find_buffer(), find & get buffer from list with specified
 * pic_id.
 * @param BUFFERLIST* list: list handle.
 * @param uint32_t next_pic_id: the specified pic_id.
 * @param uint32_t* num: slot id of got buffer.
 * @return BUFFER*, handle of got buffer.
 */
BUFFER* bufferlist_find_buffer(BUFFERLIST* list, uint32_t next_pic_id,
                               uint32_t* num) {
  uint32_t size = bufferlist_get_size(list);
  if (size == 0) return NULL;

  uint32_t i = 0;
  for (i = 0; i < size; ++i) {
    BUFFER* buff = *bufferlist_at(list, i);
    if (buff->frame_display_id == next_pic_id) {
      *num = i;
      return buff;
    }
  }
  return NULL;
}

/**
 * @brief bufferlist_push_buffer(), push a buffer to list.
 * @param BUFFERLIST* list: list handle.
 * @param BUFFER* buff: handle of buffer to push.
 * @return int, 0: succeed; Other value: failed.
 */
int bufferlist_push_buffer(BUFFERLIST* list, BUFFER* buff) {
  ASSERT(list);
  if (list->size == list->capacity) return -1;
  list->list[list->size++] = buff;
  return 0;
}

/**
 * @brief bufferlist_remove(), remove a buffer from list with specified slot id.
 * @param BUFFERLIST* list: list handle.
 * @param uint32_t i: the specified slot id.
 * @return none.
 */
void bufferlist_remove(BUFFERLIST* list, uint32_t i) {
  ASSERT(list);
  ASSERT(i < list->size);
  memmove(list->list + i, list->list + i + 1,
          (list->size - i - 1) * sizeof(BUFFER*));
  --list->size;
}

/**
 * @brief bufferlist_get_buffer(), get the first buffer from list.
 * @param BUFFERLIST* list: list handle.
 * @return BUFFER*: handle of got buffer.
 */
BUFFER* bufferlist_get_buffer(BUFFERLIST* list) {
  ASSERT(list);
  if (list->size == 0) {
    return NULL;
  }

  return *bufferlist_at(list, 0);
}

/**
 * @brief bufferlist_pop_buffer(), pop(remove) the first buffer from list.
 * @param BUFFERLIST* list: list handle.
 * @return int, 0: succeed; Other value: failed.
 */
int bufferlist_pop_buffer(BUFFERLIST* list) {
  ASSERT(list);
  if (list->size == 0) {
    return -1;
  }

  bufferlist_remove(list, 0);
  return 0;
}

/**
 * @brief bufferlist_get_tail(), get the last buffer from list.
 * @param BUFFERLIST* list: list handle.
 * @return BUFFER*: handle of got buffer.
 */
BUFFER* bufferlist_get_tail(BUFFERLIST* list) {
  ASSERT(list);
  if (list->size == 0) {
    return NULL;
  }

  return *bufferlist_at(list, list->size - 1);
}

/**
 * @brief bufferlist_get_head(), get the last buffer from list.
 * @param BUFFERLIST* list: list handle.
 * @return BUFFER*: handle of got buffer.
 */
BUFFER* bufferlist_get_head(BUFFERLIST* list) {
  ASSERT(list);
  if (list->size == 0) {
    return NULL;
  }

  return *bufferlist_at(list, 0);
}

/**
 * @brief bufferlist_flush(), flush buffer list.
 * @param BUFFERLIST *list: buffer list.
 * @return void.
 */
void bufferlist_flush(BUFFERLIST* list) {
  BUFFER* p = NULL;

  do {
    p = bufferlist_get_buffer(list);
    if (p) {
      free(p);
      bufferlist_pop_buffer(list);
    }
  } while (p);
}
