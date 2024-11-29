////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Copyright (c) 2012-2024 James Card                     //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included    //
// in all copies or substantial portions of the Software.                     //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//                                 James Card                                 //
//                          http://www.jamescard.org                          //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

// Doxygen marker
/// @file

#include "RadixTree.h"
#ifdef DS_LOGGING_ENABLED
#include "LoggingLib.h"
#else
#undef printLog
#define printLog(...) {}
#define logFile stderr
#define LOG_MALLOC_FAILURE(...) {}
#endif


#if defined(_WIN32)

#define exchangePointer(destinationPointerP, sourcePointer) \
  InterlockedExchangePointer(destinationPointerP, sourcePointer)
#define compareExchangePointer( \
    destinationPointerP, sourcePointer, comparePointer) \
  InterlockedCompareExchangePointer( \
    destinationPointerP, sourcePointer, comparePointer)

#elif defined(__GNUC__)

#define exchangePointer(destinationPointerP, sourcePointer) \
  __atomic_exchange_n(destinationPointerP, sourcePointer, __ATOMIC_SEQ_CST)
static inline void* compareExchangePointer(
  void * volatile *destinationPointerP,
  void *sourcePointer, void *comparePointer
) {
  void **expectedPointerP = &comparePointer;
  __atomic_compare_exchange_n(
    destinationPointerP, expectedPointerP, sourcePointer,
    false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);

  // Either the value of *destinationPointerP == *expectedPointerP at the time
  // of the call and *expectedPointerP was left unmodified -OR- they were not
  // equal and the value of *destinationPointerP was written to
  // *expectedPointerP.  Either way, the original value of *destinationPointerP
  // at the time of the call is now in *expectedPointerP.  Return that.
  return *expectedPointerP;
}
  
#endif

/// @fn RadixTree* radixTreeCreate(tss_dtor_t destructor)
///
/// @brief Create a radix tree to be used with the thread-specific storage
/// calls.
///
/// @param destructor The destructor to call on the values in the tree when one
/// of them is deleted or the tree is destroyed.
///
/// @return Returns a pointer to a newly-allocated RadixTree on success,
/// NULL on failure.
RadixTree* radixTreeCreate(tss_dtor_t destructor) {
  RadixTree *tree
    = (RadixTree*) malloc(sizeof(RadixTree));
  if (tree == NULL) {
    LOG_MALLOC_FAILURE();
    return NULL;
  }

  tree->root
    = (RadixTreeNode*) calloc(1, sizeof(RadixTreeNode));
  if (tree->root == NULL) {
    LOG_MALLOC_FAILURE();
    free(tree); tree = NULL;
    return NULL;
  }

  tree->destructor = destructor;

  return tree;
}

/// @fn RadixTreeNode* radixTreeDestroyNode(
///                        RadixTreeNode *node, tss_dtor_t destructor)
///
/// @brief Detroy a RadixTreeNode, its value, and all its sub-nodes.
///
/// @param node A pointer to a previously-allocated RadixTreeNode.
/// @param destructor A tss_dtor_t that can be used to destroy the value held by
///   the node.
///
/// @return This function always returns NULL.
static inline RadixTreeNode* radixTreeDestroyNode(
  RadixTreeNode *node, tss_dtor_t destructor
) {
  if (node != NULL) {
    if (destructor != NULL) {
      destructor(exchangePointer(
        (void * volatile*) &node->value, NULL));
    } else {
      node->value = NULL;
    }

    RadixTreeNode **radixTreeNodes = node->radixTreeNodes;
    // Recursively destroy any sub-nodes.
    for (uint32_t ii = 0; ii < RADIX_TREE_ARRAY_SIZE; ii++) {
      if (radixTreeNodes[ii] != NULL) {
        radixTreeDestroyNode(
          (RadixTreeNode*) exchangePointer(
            (void * volatile*) &radixTreeNodes[ii], NULL),
          destructor);
      }
    }
    free(node); node = NULL;
  }

  return node;
}

/// @fn RadixTree* radixTreeDestroy(RadixTree *tree)
///
/// @brief Destroy a RadixTree.
///
/// @param tree A pointer to a previously-allocated RadixTree.
///
/// @return This function always returns NULL.
RadixTree* radixTreeDestroy(RadixTree *tree) {
  if (tree != NULL) {
    radixTreeDestroyNode(
      (RadixTreeNode*) exchangePointer(
        (void * volatile*) &tree->root, NULL),
      tree->destructor);
    free(tree); tree = NULL;
  }
  return tree;
}

/// @fn void* radixTreeNodeGetValue(RadixTreeNode *node,
///               const volatile RADIX_TREE_KEY_ELEMENT *key, size_t numKeys)
///
/// @brief Get a previously-set value from a RadixTreeNode.
///
/// @param node A pointer to a previously-allocated RadixTreeNode or
///   NULL.
/// @param key A pointer to key element values.
/// @param numKeys The number of keys pointed to by the key.
///
/// @return Returns the value in the tree if it exists, NULL if not.
static inline void* radixTreeNodeGetValue(RadixTreeNode *node,
  const volatile RADIX_TREE_KEY_ELEMENT *key, size_t numKeys
) {
  const volatile void *returnValue = NULL;
  RadixTreeNode **radixTreeNodes = NULL;
  RADIX_TREE_KEY_ELEMENT currentKeyIndex = 0;

  // There are two ways we can go about this search:  We can start from the
  // beginning of the key and go to the end or we can start from the end of it
  // and go to the beginning.  It is more space efficient to start with the end
  // that is most likely to have data that's common to the keys because that
  // will result in the fewest branches.  (The number of leaves remains
  // unchanged either way.)  Because we're searching on integer values, this
  // means that it's most optimal to start from the end that is more likely to
  // have zeros in it, i.e. the most-significant bytes.  This means that we need
  // to take endianness into account here.  If I was interested in supporting
  // big-endian systems then I would put in some code here that would optimize
  // this search depending on the system.  However, as of the time of this
  // comment, I have no plans to support big-endian systems.  So, I am therefore
  // only providing the optimization for little-endian systems.
  //
  // One additional note on this:  It is also marginally more performant to do
  // this optimization.
  //
  // JBC 2024-11-15
  while ((numKeys > 0) && (node != NULL)) {
    currentKeyIndex = key[numKeys - 1];
    radixTreeNodes = node->radixTreeNodes;
    node = radixTreeNodes[currentKeyIndex];
    numKeys--;
  }

  if (node != NULL) {
    returnValue = node->value;
  }

  return (void*) returnValue;
}

/// @fn void* radixTreeGetValue(RadixTree *tree,
///               const volatile void *key, size_t keySize)
///
/// @brief Get a value from a RadixTree.
///
/// @param tree A pointer to a previously-allocated RadixTree.
/// @param key A pointer to the key to look for.
/// @param keySize The number of bytes in the key.
///
/// @return Returns the value found on success, NULL if not found or on error.
void* radixTreeGetValue(RadixTree *tree,
  const volatile void *key, size_t keySize
) {
  void *returnValue = NULL;
  if ((tree == NULL) || ((key == NULL) && (keySize != 0))) {
    return returnValue;
  }

  returnValue = radixTreeNodeGetValue(
    tree->root,
     (RADIX_TREE_KEY_ELEMENT*) key,
     (keySize >> RADIX_TREE_NUM_KEYS_BIT_SHIFT)
  );

  return returnValue;
}

/// @fn void* radixTreeGetValue2(RadixTree *tree1,
///               const volatile void *key1, size_t keySize1,
///               const volatile void *key2, size_t keySize2)
///
/// @brief Get a value from a second-level RadixTree.
///
/// @param tree1 A pointer to the top-level RadixTree.
/// @param key1 A pointer to the key of the second-level tree.
/// @param keySize1 The number of bytes in key1.
/// @param key1 A pointer to the key of value in the second-level tree.
/// @param keySize1 The number of bytes in key2.
///
/// @return Returns the found element on success, NULL on failure.
void* radixTreeGetValue2(RadixTree *tree1,
  const volatile void *key1, size_t keySize1,
  const volatile void *key2, size_t keySize2
) {
  void *returnValue = NULL;
  if ((tree1 == NULL)
    || ((key1 == NULL) && (keySize1 != 0))
    || ((key2 == NULL) && (keySize2 != 0))
  ) {
    return returnValue; // NULL
  }

  RadixTree *tree2
    = (RadixTree*) radixTreeGetValue(tree1, key1, keySize1);
  if (tree2 != NULL) {
    returnValue = radixTreeGetValue(tree2, key2, keySize2);
  }

  return returnValue;
}

/// @fn void* radixTreeNodeSetValue(RadixTreeNode *node,
///             const volatile RADIX_TREE_KEY_ELEMENT *key, size_t numKeys,
///             volatile void *value)
///
/// @brief Set a value in the radix tree.  Allocate any intermediate nodes if
/// they don't exist.
///
/// @param node A pointer to a RadixTreeNode in the tree.
/// @param key An array of key elements.
/// @param numKeys The number of keys in the keys array.
/// @param value A pointer to the value to set in the tree.
///
/// @return Returns the previous value of the node if there was one, NULL
/// otherwise.
static inline void* radixTreeNodeSetValue(RadixTreeNode *node,
  const volatile RADIX_TREE_KEY_ELEMENT *key, size_t numKeys,
  volatile void *value
) {
  void* returnValue = NULL;
  RADIX_TREE_KEY_ELEMENT currentKeyIndex = 0;
  RadixTreeNode **radixTreeNodes = NULL;

  if (node != NULL) {
    while ((numKeys > 0) && (node != NULL)) {
      currentKeyIndex = key[numKeys - 1];
      radixTreeNodes = node->radixTreeNodes;
      if (radixTreeNodes[currentKeyIndex] == NULL) {
        RadixTreeNode *radixTreeNode = (RadixTreeNode*)
          calloc(1, sizeof(RadixTreeNode));
        if (compareExchangePointer(
          (void* volatile*) &radixTreeNodes[currentKeyIndex],
          (void*) radixTreeNode,
          NULL) != NULL
        ) {
          free(radixTreeNode); radixTreeNode = NULL;
        }
      }

      node = radixTreeNodes[currentKeyIndex];
      numKeys--;
    }

    if (node != NULL) {
      returnValue
        = exchangePointer((void* volatile*) &node->value, (void*) value);
    }
  }

  return returnValue;
}

/// @fn void* radixTreeSetValue(RadixTree *tree,
///             const volatile void *key, size_t keySize,
///             volatile void *value)
///
/// @brief Set a value in the radix tree.
///
/// @param tree A pointer to a previously-allocated RadixTree.
/// @param key A pointer to a key value.
/// @param keySize The number of bytes of the key.
/// @param value A pointer to the value to set.
///
/// @return Returns the previous value at the node in the tree, if any.
void* radixTreeSetValue(RadixTree *tree,
  const volatile void *key, size_t keySize, volatile void *value
) {
  void *returnValue = NULL;
  if ((tree == NULL) || ((key == NULL) && (keySize != 0))) {
    return returnValue; // NULL
  }

  returnValue = radixTreeNodeSetValue(
    tree->root,
    (RADIX_TREE_KEY_ELEMENT*) key,
    (keySize >> RADIX_TREE_NUM_KEYS_BIT_SHIFT),
    value
  );

  return returnValue;
}

/// @fn void* radixTreeSetValue2(RadixTree *tree1,
///             const volatile void *key1, size_t keySize1,
///             const volatile void *key2, size_t keySize2,
///             volatile void *value, tss_dtor_t destructor2)
///
/// @brief Set a value in a second-level RadixTree.
///
/// @param tree1 A pointer to the top-level RadixTree.
/// @param key1 A pointer to the key of the second-level tree.
/// @param keySize1 The number of bytes in key1.
/// @param key1 A pointer to the key of value in the second-level tree.
/// @param keySize1 The number of bytes in key2.
/// @param destructor2 The tss_dtor_t for the second-level tree if a second-
///  level tree doesn't already exist for key1.
///
/// @return Returns the number of new elements added on success, -1 on error.
void* radixTreeSetValue2(RadixTree *tree1,
  const volatile void *key1, size_t keySize1,
  const volatile void *key2, size_t keySize2,
  volatile void *value, tss_dtor_t destructor2
) {
  void *returnValue = NULL;
  if ((tree1 == NULL)
    || ((key1 == NULL) && (keySize1 != 0))
    || ((key2 == NULL) && (keySize2 != 0))
  ) {
    return returnValue; // -1
  }

  RadixTree *tree2
    = (RadixTree*) radixTreeGetValue(tree1, key1, keySize1);
  if (tree2 == NULL) {
    RadixTree *newTree = radixTreeCreate(destructor2);
    radixTreeSetValue(tree1, key1, keySize1, newTree);
    tree2 = (RadixTree*) radixTreeGetValue(
      tree1, key1, keySize1);
    if (tree2 != newTree) {
      // We were in a race condition and something else overwrote our
      // tree.  Delete the one we created.
      newTree = radixTreeDestroy(newTree);
      // Get it again just to be sure.
      tree2 = (RadixTree*) radixTreeGetValue(
        tree1, key1, keySize1);
    }
  }
  
  if (tree2 == NULL) {
    // There's no tree and we have nothing to construct the missing tree
    // with.  We can't work like this.  Fail.
    return returnValue; // NULL
  }

  returnValue = radixTreeSetValue(tree2, key2, keySize2, value);

  return returnValue;
}

/// @fn int radixTreeNodeDeleteValue(RadixTreeNode *node,
///             const volatile RADIX_TREE_KEY_ELEMENT *key, size_t numKeys,
///             tss_tdor_t destructor)
///
/// @brief Delete a previously-set value from a RadixTreeNode.
///
/// @param node A pointer to a previously-allocated RadixTreeNode or
///   NULL.
/// @param key A pointer to key element values.
/// @param numKeys The number of keys pointed to by the key.
/// @param destructor The destructor to call on the value, if found.
///
/// @return Returns 0 if the value was not found, 1 if the value was found and
/// deleted but there are still things in the node's array, 2 if the value was
/// found and deleted and the node's array is now empty, -1 on error.
static inline int radixTreeNodeDeleteValue(
  RadixTreeNode *node,
  const volatile RADIX_TREE_KEY_ELEMENT *key, size_t numKeys,
  tss_dtor_t destructor
) {
  int returnValue = 0;
  RADIX_TREE_KEY_ELEMENT currentKeyIndex = 0;
  RadixTreeNode **radixTreeNodes = NULL;

  if (node != NULL) {
    bool valueDeleted = false;
    radixTreeNodes = node->radixTreeNodes;
    if (numKeys > 0) {
      currentKeyIndex = key[numKeys - 1];
      int secondLevelReturnValue = radixTreeNodeDeleteValue(
        radixTreeNodes[currentKeyIndex], key, numKeys - 1, destructor);
      if (secondLevelReturnValue > 1) {
        // There's nothing left in this node.  Delete it.
        free(exchangePointer(
          (void* volatile*) &radixTreeNodes[currentKeyIndex], NULL));
        valueDeleted = true;
        returnValue = 1;
      } else {
        returnValue = secondLevelReturnValue;
      }
    } else if (destructor != NULL) {
      // Delete the value.
      destructor(exchangePointer((void* volatile*)&node->value, NULL));
      valueDeleted = true;
      returnValue = 1;
    } else {
      // destructor is NULL.  Just set the pointer to NULL.
      node->value = NULL;
      valueDeleted = true;
      returnValue = 1;
    }

    if ((valueDeleted == true) && (node->value == NULL)) {
      // See if there's anything left in the array.
      unsigned int ii = 0;
      for (; ii < RADIX_TREE_ARRAY_SIZE; ii++) {
        if (radixTreeNodes[ii] != NULL) {
          break;
        }
      }
      if (ii == RADIX_TREE_ARRAY_SIZE) {
        // There's nothing left in this node.  Get rid of it.
        returnValue = 2;
      }
    }
  } else {
    returnValue = -1;
  }

  return returnValue;
}

/// @fn int radixTreeDeleteValue(RadixTree *tree,
///             const volatile void *key, size_t keySize)
///
/// @brief Delete a value from a RadixTree.
///
/// @param tree A pointer to a previously-allocated RadixTree.
/// @param key A pointer to the key to look for.
/// @param keySize The number of bytes in the key.
///
/// @return Returns 0 on success, -1 on error.
int radixTreeDeleteValue(RadixTree *tree,
  const volatile void *key, size_t keySize
) {
  int returnValue = -1;
  if ((tree == NULL) || ((key == NULL) && (keySize != 0))) {
    return returnValue; // -1
  }

  radixTreeNodeDeleteValue(
    tree->root,
    (RADIX_TREE_KEY_ELEMENT*) key,
    (keySize >> RADIX_TREE_NUM_KEYS_BIT_SHIFT),
    tree->destructor
  );

  returnValue = 0;
  return returnValue;
}

/// @fn int radixTreeDeleteValue2(RadixTree *tree1,
///             const volatile void *key1, size_t keySize1,
///             const volatile void *key2, size_t keySize2)
///
/// @brief Delete a value from a second-level RadixTree.
///
/// @param tree1 A pointer to the top-level RadixTree.
/// @param key1 A pointer to the key of the second-level tree.
/// @param keySize1 The number of bytes in key1.
/// @param key1 A pointer to the key of value in the second-level tree.
/// @param keySize1 The number of bytes in key2.
///
/// @return Returns 0 on success, -1 on error.
int radixTreeDeleteValue2(RadixTree *tree1,
  const volatile void *key1, size_t keySize1,
  const volatile void *key2, size_t keySize2
) {
  int returnValue = -1;
  if ((tree1 == NULL)
    || ((key1 == NULL) && (keySize1 != 0))
    || ((key2 == NULL) && (keySize2 != 0))
  ) {
    return returnValue; // -1
  }

  RadixTree *tree2
    = (RadixTree*) radixTreeGetValue(tree1, key1, keySize1);
  if (tree2 != NULL) {
    returnValue = radixTreeDeleteValue(tree2, key2, keySize2);
  }

  return returnValue;
}

