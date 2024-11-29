///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              11.22.2024
///
/// @file              RadixTree.h
///
/// @brief             This library contains the function and structure
///                    definitions that make up the radix tree data structure.
///
/// @copyright
///                   Copyright (c) 2012-2024 James Card
///
/// Permission is hereby granted, free of charge, to any person obtaining a
/// copy of this software and associated documentation files (the "Software"),
/// to deal in the Software without restriction, including without limitation
/// the rights to use, copy, modify, merge, publish, distribute, sublicense,
/// and/or sell copies of the Software, and to permit persons to whom the
/// Software is furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included
/// in all copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
/// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
/// DEALINGS IN THE SOFTWARE.
///
///                                James Card
///                         http://www.jamescard.org
///
///////////////////////////////////////////////////////////////////////////////

#ifndef RAIDX_TREE_H
#define RADIX_TREE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "CThreads.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define RADIX_TREE_KEY_ELEMENT uint8_t
#define RADIX_TREE_KEY_ELEMENT_BIT_WIDTH (sizeof(RADIX_TREE_KEY_ELEMENT) * 8)
#define RADIX_TREE_NUM_KEYS_BIT_SHIFT 0
#define RADIX_TREE_ARRAY_SIZE (1 << RADIX_TREE_KEY_ELEMENT_BIT_WIDTH)

/// @struct RadixTreeNode
///
/// @brief Individual node within the radix tree.
///
/// @var value A pointer to the value that exactly matches the key.
/// @var radixTreeNodes The array of RadixTreeNodes subordinate to this
///   node.
/// @var radixTreeNodesBitmap The bitmap of allocated nodes within
///   radixTreeNodes.
typedef struct RadixTreeNode {
  volatile void *value;
  struct RadixTreeNode *radixTreeNodes[RADIX_TREE_ARRAY_SIZE];
} RadixTreeNode;

/// @struct RadixTree
///
/// @brief Container for the radix tree.
///
/// @var root A pointer to the top RadixTreeNode tree;
/// @var destructor A tss_dtor_t function used to free the values of the tree.
typedef struct RadixTree {
  RadixTreeNode *root;
  tss_dtor_t destructor;
} RadixTree;

RadixTree* radixTreeCreate(tss_dtor_t destructor);
RadixTree* radixTreeDestroy(RadixTree *tree);
void* radixTreeGetValue(RadixTree *tree,
  const volatile void *key, size_t keySize);
void* radixTreeGetValue2(RadixTree *tree1,
  const volatile void *key1, size_t keySize1,
  const volatile void *key2, size_t keySize2);
void* radixTreeSetValue(RadixTree *tree,
  const volatile void *key, size_t keySize, volatile void *value);
void* radixTreeSetValue2(RadixTree *tree1,
  const volatile void *key1, size_t keySize1,
  const volatile void *key2, size_t keySize2,
  volatile void *value, tss_dtor_t destructor2);
int radixTreeDeleteValue(RadixTree *tree,
  const volatile void *key, size_t keySize);
int radixTreeDeleteValue2(RadixTree *tree1,
  const volatile void *key1, size_t keySize1,
  const volatile void *key2, size_t keySize2);

#ifdef __cplusplus
}
#endif

#endif // RAIDX_TREE_H

