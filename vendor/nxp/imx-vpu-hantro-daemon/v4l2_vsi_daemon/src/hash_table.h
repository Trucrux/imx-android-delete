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

#ifndef HASH_TABLE_H
#define HASH_TABLE_H
/**
 * @file hash_table.h
 * @brief hash table interface.
 */

#include <stdint.h>
#include "daemon_instance.h"

/**
 * @brief Max size of hash table, this is also the max stream size supported.
 */
#define HashMaxSize MAX_STREAMS

/**
 * @brief key type.
 */
typedef uint64_t KeyType;  // inst_id

/**
 * @brief value type of hash element.
 */
typedef v4l2_daemon_inst ValueType;

/**
 * @brief function pointer for getting current key.
 */
typedef int32_t (*HashFunc)(KeyType key);

/**
 * @brief state of hash element.
 */
typedef enum Stat {
  Empty, /** state: empty.*/
  Valid  /** state: valid.*/
} Stat;

/**
 * @brief hash element.
 */
typedef struct HashElem {
  KeyType key;      /** key */
  ValueType* value; /** value of hash element */
  Stat stat;        /** state */
} HashElem;

/**
 * @brief hash table.
 */
typedef struct HashTable {
  HashElem data[HashMaxSize]; /** data of hash table */
  int32_t size;               /** size of hash table */
  HashFunc hashfunc;          /** function pointer of hash table */
} HashTable;

/**
 * @brief Initial a hash table.
 * @param[in] ht Hash table pointer.
 * @return void.
 */
void hash_table_init(HashTable* ht);

/**
 * @brief Insert an element to a hash table.
 * @param[in] ht Hash table pointer.
 * @param[in] key The key of the element to insert.
 * @return 0: insert successfully, other: insert unsuccessfully.
 */
int32_t hash_table_insert(HashTable* ht, KeyType key);

/**
 * @brief Search a given key in hash table.
 * @param[in] ht Hash table pointer.
 * @param[in] key The key of the element to search.
 * @param[out] value The element pointer if finded, otherwise NULL.
 * @return 0: find successfully, other: find unsuccessfully.
 */
int32_t hash_table_find(HashTable* ht, KeyType key, ValueType** value);

/**
 * @brief Remove a given key in hash table.
 * @param[in] ht Hash table pointer.
 * @param[in] key The key of the element to search.
 * @return void.
 */
void hash_table_remove(HashTable* ht, KeyType key, int freeobj);

/**
 * @brief Check if the hash table is empty.
 * @param[in] ht Hash table pointer.
 * @return 0: empty, other: not empty.
 */
int32_t hash_table_empty(HashTable* ht);

/**
 * @brief Get size of the hash table.
 * @param[in] ht Hash table pointer.
 * @return int Size of the hash table.
 */
int32_t hash_table_size(HashTable* ht);

/**
 * @brief Destory the hash table.
 * @param[in] ht Hash table pointer.
 * @return none.
 */
void hash_table_destroy(HashTable* ht);

ValueType* hash_table_find_bytid(HashTable* ht, pthread_t tid);

#endif
