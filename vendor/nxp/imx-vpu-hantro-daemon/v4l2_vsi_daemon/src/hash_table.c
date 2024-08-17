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

#include "hash_table.h"
#include <string.h>
#include "vsi_daemon_debug.h"

int32_t hash_func_default(KeyType key) {
  return (key & 0xffffffff) % HashMaxSize;
}

void hash_table_init(HashTable* ht) {
  if (ht == NULL) return;
  ht->size = 0;
  ht->hashfunc = hash_func_default;
  for (size_t i = 0; i < HashMaxSize; i++) {
    ht->data[i].key = 0;
    ht->data[i].stat = Empty;
    ht->data[i].value = NULL;
  }
}

int32_t hash_table_insert(HashTable* ht, KeyType key) {
  if (ht == NULL) return 1;
  if (ht->size >= HashMaxSize) return 1;
  int32_t cur = ht->hashfunc(key);
  if (cur >= HashMaxSize) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR,
               "When HashTableInsert, HashMaxSize is reached.!\n");
    return 1;
  }
  while (1) {
    if (ht->data[cur].key == key)  // already exist
    {
      HANTRO_LOG(HANTRO_LEVEL_ERROR,
                 "When HashTableInsert, key already exist!\n");
      return 1;
    }
    if (ht->data[cur].stat != Valid) {
      ht->data[cur].key = key;
      ht->data[cur].stat = Valid;
      ht->data[cur].value = (ValueType*)malloc(sizeof(ValueType));
      if (ht->data[cur].value == NULL) {
        HANTRO_LOG(HANTRO_LEVEL_ERROR,
                   "hash_table_insert: Error when malloc hashtable elem.\n");
        return 1;
      }
      ht->size++;
      return 0;  // insert successful
    } else {
      HANTRO_LOG(HANTRO_LEVEL_ERROR,
                 "When HashTableInsert, state is not EMPTY!\n");
    }
    cur++;
    if (cur >= HashMaxSize) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR,
                 "When HashTableInsert, HashMaxSize is reached.!\n");
      return 1;
    }
  }
}

int32_t hash_table_find(HashTable* ht, KeyType key, ValueType** value) {
  if (ht == NULL) return 1;
  int32_t offset = ht->hashfunc(key);
  if (ht->data[offset].key == key && ht->data[offset].stat == Valid) {
    *value = ht->data[offset].value;
    return 0;  // find successful.
  }
  return 1;  // find unsuccessful.
}

int32_t hash_table_find_cur(HashTable* ht, KeyType key,
                            int32_t* cur)  // find index, for delete.
{
  if (ht == NULL) return 0;
  for (int32_t i = 0; i < HashMaxSize; i++) {
    if (ht->data[i].key == key && ht->data[i].stat == Valid) {
      *cur = i;
      return 1;  // find
    }
  }
  return 0;  // not find
}

ValueType* hash_table_find_bytid(HashTable* ht, pthread_t tid) {
  if (ht == NULL) return 0;
  for (int32_t i = 0; i < HashMaxSize; i++) {
    if (ht->data[i].value && ht->data[i].stat == Valid &&
        ht->data[i].value->tid == tid)
      return ht->data[i].value;
  }
  return NULL;  // not find
}

void hash_table_remove(HashTable* ht, KeyType key, int freeobj) {
  if (ht == NULL) return;
  // ValueType value = 0;
  int32_t cur = 0;
  int32_t ret = hash_table_find_cur(ht, key, &cur);
  if (ret == 0)
    return;
  else {
    ht->data[cur].stat = Empty;
    ht->data[cur].key = 0;
    if (freeobj)
      free(ht->data[cur].value);
    ht->size--;
  }
}

int32_t hash_table_empty(HashTable* ht) {
  if (ht == NULL)
    return 0;
  else
    return ht->size > 0 ? 1 : 0;
}

int32_t hash_table_size(HashTable* ht) {
  if (ht == NULL) return 0;
  return ht->size;
}

void hash_table_print(HashTable* ht, const int8_t* msg) {
  if (ht == NULL || ht->size == 0) return;
  HANTRO_LOG(HANTRO_LEVEL_DEBUG, "%s\n", msg);
  for (int32_t i = 0; i < HashMaxSize; i++) {
    // if (ht->data[i].stat != Empty)
    // printf("[%d]  key=%d  value=%d  stat=%d\n", i,
    // ht->data[i].key,ht->data[i].value, (int32_t)ht->data[i].stat);
  }
}
