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

#ifdef _WIN32
#include "WinCThreads.h"
#include <stdio.h>

#if LOGGING_ENABLED && WIN_CTHREADS_LOGGING_ENABLED

#include <process.h> // for getpid() and the exec..() family
#include <string.h>
#define getpid _getpid

#define printLog(logLevel, formatString, ...) \
    do { \
        if (logLevel >= logThreshold) { \
            FILE *log = (logFile != NULL) ? logFile : stderr; \
            struct timespec now; \
            timespec_get(&now, TIME_UTC); \
            struct tm nowStruct; \
            gmtime_s(&nowStruct, &now.tv_sec); \
            int year = nowStruct.tm_year + 1900; \
            int month = nowStruct.tm_mon + 1; \
            int day = nowStruct.tm_mday; \
            int hour = nowStruct.tm_hour; \
            int minute = nowStruct.tm_min; \
            int second = nowStruct.tm_sec; \
            int nanosecond = now.tv_nsec; \
            char *session = "xxxx..9999 "; \
            char *hostname = getenv("COMPUTERNAME"); \
            if (hostname == NULL) { \
                hostname = "localhost"; \
            } \
            int pid = getpid(); \
            int threadId = thrd_current(); \
            const char *fileName = strrchr(__FILE__, '\\'); \
            if (fileName != NULL) { \
                fileName++; \
            } else { \
                fileName = __FILE__; \
            } \
            fprintf(log, \
                "[%d-%02d-%02d %02d:%02d:%02d.%09d %s%s:%d.%d %s:%s.%d %s] " \
                formatString, year, month, day, hour, minute, second, nanosecond, \
                session, hostname, pid, threadId, fileName, __func__, \
                __LINE__, logLevelName[logLevel], ##__VA_ARGS__); \
            fflush(log); \
        } \
    } while (0)

typedef enum LogLevel {
    NEVER,
    FLOOD,
    TRACE,
    DEBUG,
    DETAIL,
    INFO,
    WARN,
    ERR, // ERROR conflicts with something in windows.h
    CRITICAL,
    BOX,
    BANNER,
    NONE,
    NUM_LOG_LEVELS
} LogLevel;

static const char *logLevelName[NUM_LOG_LEVELS] = {
    "NEVER",
    "FLOOD",
    "TRACE",
    "DEBUG",
    "DETAIL",
    "INFO",
    "WARNING",
    "ERROR",
    "CRITICAL",
    "BOX",
    "BANNER",
    "NONE",
};

extern LogLevel logThreshold;
extern FILE *logFile;

#else // LOGGING_ENABLED or WIN_CTHREADS_LOGGING_ENABLED not defined

#define logFile stderr
#define printLog(...) {}

#endif

// Defines for LOG_MALLOC_FAILURE
#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)
#define MALLOC_FAILURE_MESSAGE "malloc failure\n"

/// @def LOG_MALLOC_FAILURE
/// Attempt to log a memory allocation failure to whatever log file we can.
/// Cannot make use of anything that would allocate memory.  Do everything in
/// our power to print a message SOMEWHERE.  This is intended to be a last gasp
/// for the program.
#define LOG_MALLOC_FAILURE() \
    do { \
        FILE *errLog = logFile; \
        if (errLog == NULL) { \
            errLog = stderr; \
        } \
        int fputsStatus = fputs(__FILE__, errLog); \
        if ((fputsStatus == EOF) && (errLog != stderr)) { \
            /* Try again from the beginning with errLog set to stderr. */ \
            errLog = stderr; \
            fputsStatus = fputs(__FILE__, errLog); \
        } \
        fputsStatus = fputs(":", errLog); \
        if ((fputsStatus == EOF) && (errLog != stderr)) { \
            /* Try again from the beginning with errLog set to stderr. */ \
            errLog = stderr; \
            fputsStatus = fputs(__FILE__, errLog); \
            fputsStatus = fputs(":", errLog); \
        } \
        fputsStatus = fputs(__func__, errLog); \
        if ((fputsStatus == EOF) && (errLog != stderr)) { \
            /* Try again from the beginning with errLog set to stderr. */ \
            errLog = stderr; \
            fputsStatus = fputs(__FILE__, errLog); \
            fputsStatus = fputs(":", errLog); \
            fputsStatus = fputs(__func__, errLog); \
        } \
        fputsStatus = fputs(".", errLog); \
        if ((fputsStatus == EOF) && (errLog != stderr)) { \
            /* Try again from the beginning with errLog set to stderr. */ \
            errLog = stderr; \
            fputsStatus = fputs(__FILE__, errLog); \
            fputsStatus = fputs(":", errLog); \
            fputsStatus = fputs(__func__, errLog); \
            fputsStatus = fputs(".", errLog); \
        } \
        fputsStatus = fputs(STRINGIFY(__LINE__) ": " MALLOC_FAILURE_MESSAGE, errLog); \
        if ((fputsStatus == EOF) && (errLog != stderr)) { \
            /* Try again from the beginning with errLog set to stderr. */ \
            errLog = stderr; \
            fputsStatus = fputs(__FILE__, errLog); \
            fputsStatus = fputs(":", errLog); \
            fputsStatus = fputs(__func__, errLog); \
            fputsStatus = fputs(".", errLog); \
            fputsStatus = fputs(STRINGIFY(__LINE__) ": " MALLOC_FAILURE_MESSAGE, errLog); \
        } \
        fflush(errLog); \
    } while (0)


// A note about why the radix tree support data structure is here:
//
// When the C standard incorporated threading in C11, they based their model
// on pthreads.  The 'p' in "pthreads" stands for "POSIX", which means that the
// model very closely resembles what was already incorporated into POSIX-based
// systems.  The PosixCThreads library is really just a very thin wrapper around
// the ptherads library.  This is true even in gcc, which requires that pthreads
// be linked to an executable making use of C threads in gcc 8.0 and beyond.
//
// The Windows model was not designed with pthreads in mind.  While Windows
// supports the same concepts in general, the specifics of the Windows
// implementations of those mechanisms are very different.  For the most part,
// this can be dealt with by defining types and functions that abstract the
// details of the internals of Windows mechanisms away from the user of this
// library.
//
// There is, however, one very fundamental difference that can't easily be
// abstracted by types:  Thread-specific storage.  In Windows, thread-specific
// storage is intended to be achieved by "Thread Local Storage" slots which
// (a) don't align well to the POSIX/C threads model and (b) don't have any
// mechanism for the destructors defined by POSIX/C threads.  Because of this,
// I needed a way to keep track of thread-specific storage that would meet the
// requirements of the standard.  The data structures in this library are used
// to achieve that.
//
// In this library, thread-specific storage is implemented as two lookups: One
// that is an array by key and then a radix tree by thread ID (this is where the
// actual storage is) and one that is a radix tree by thread ID and then a radix
// tree key (which holds a reference to the values in the first trees).
//
// The reason for the second lookup is that all of a thread's storage has to be
// deleted when the thread exits.  So, when a thread exits, its second level
// tree under its thread ID is deleted, which deletes all the elements in the
// first tree as it is destroyed.
//
// The trees are implemented in a thread-safe, lock-free way that makes use of
// atomic exchange functions to ensure that freed pointers aren't accidentally
// used by other threads as something is being deleted from a tree.  Mutexes are
// *NOT* used by thread-specific storage to keep the access time as low as
// possible.
//
// The destructors provided in tss_create are called when a thread directly
// calls thrd_exit or when it returns from its main function, which is wrapped
// by a fucntion that invokes thrd_exit upon the functions's return.  The
// destructors are also called in the event that the thread-specific storage is
// deleted by a call to tss_delete.

/****************** Begin Support Data Structure Code *************************/

#define RADIX_TREE_KEY_ELEMENT uint8_t
#define RADIX_TREE_KEY_ELEMENT_BIT_WIDTH (sizeof(RADIX_TREE_KEY_ELEMENT) * 8)
#define RADIX_TREE_NUM_KEYS_BIT_SHIFT 0
#define RADIX_TREE_ARRAY_SIZE (1 << RADIX_TREE_KEY_ELEMENT_BIT_WIDTH)
#define TSS_T_BIT_WIDTH (sizeof(tss_t) * 8)
#define ARRAY_OF_RADIX_TREES_SIZE (1 << TSS_T_BIT_WIDTH)

/// @def RADIX_TREE_FIND_NODE
///
/// @brief Use the local variables 'numKeys', 'node', 'currentKeyIndex', 'key',
/// and 'radixTreeNodes' to find the target node of the key.  This is the search
/// algorithm used in windowsRadixTreeNodeGetValue and
/// windowsRadixTreeNodeDeleteValue.  windowsRadixTreeNodeSetValue has special
/// logic.
///
/// @details
/// There are two ways we can go about this search:  We can start from the
/// beginning of the key and go to the end or we can start from the end of it
/// and go to the beginning.  It is more space efficient to start with the end
/// that is most likely to have data that's common to the keys because that will
/// result in the fewest branches.  (The number of leaves remains unchanged
/// either way.)  Because we're searching on integer values, this means that
/// it's most optimal to start from the end that is more likely to have zeros
/// in it, i.e. the most-significant bytes.  This means that we need to take
/// endianness into account here.  If I was interested in supporting big-endian
/// systems then I would put in some code here that would optimize this search
/// depending on the system.  However, as of the time of this comment, I have no
/// plans to support big-endian systems.  So, I am therefore only providing the
/// optimization for little-endian systems.
///
/// One additional note on this:  It is also marginally more performant to do
/// this optimization.
///
/// JBC 2024-11-15
#define RADIX_TREE_FIND_NODE() \
    do { \
        while ((numKeys > 0) && (node != NULL)) { \
            currentKeyIndex = key[numKeys - 1]; \
            radixTreeNodes = node->radixTreeNodes; \
            node = radixTreeNodes[currentKeyIndex]; \
            numKeys--; \
        } \
    } while (0)

/// @struct WindowsRadixTreeNode
///
/// @brief Individual node within the radix tree.
///
/// @var value A pointer to the value that exactly matches the key.
/// @var radixTreeNodes The array of WindowsRadixTreeNodes subordinate to this
///   node.
/// @var radixTreeNodesBitmap The bitmap of allocated nodes within
///   radixTreeNodes.
typedef struct WindowsRadixTreeNode {
    volatile void *value;
    struct WindowsRadixTreeNode *radixTreeNodes[RADIX_TREE_ARRAY_SIZE];
} WindowsRadixTreeNode;

/// @struct WindowsRadixTree
///
/// @brief Container for the radix tree.
///
/// @var root A pointer to the top WindowsRadixTreeNode tree;
/// @var destructor A tss_dtor_t function used to free the values of the tree.
typedef struct WindowsRadixTree {
    WindowsRadixTreeNode *root;
    tss_dtor_t destructor;
} WindowsRadixTree;

/// @fn WindowsRadixTree* windowsRadixTreeCreate(tss_dtor_t destructor)
///
/// @brief Create a radix tree to be used with the thread-specific storage
/// calls.
///
/// @param destructor The destructor to call on the values in the tree when one
/// of them is deleted or the tree is destroyed.
///
/// @return Returns a pointer to a newly-allocated WindowsRadixTree on success,
/// NULL on failure.
static inline WindowsRadixTree* windowsRadixTreeCreate(tss_dtor_t destructor) {
    WindowsRadixTree *tree
        = (WindowsRadixTree*) malloc(sizeof(WindowsRadixTree));
    if (tree == NULL) {
        LOG_MALLOC_FAILURE();
        return NULL;
    }

    tree->root
        = (WindowsRadixTreeNode*) calloc(1, sizeof(WindowsRadixTreeNode));
    if (tree->root == NULL) {
        LOG_MALLOC_FAILURE();
        free(tree); tree = NULL;
        return NULL;
    }

    tree->destructor = destructor;

    return tree;
}

/// @fn WindowsRadixTreeNode* windowsRadixTreeDestroyNode(
///         WindowsRadixTreeNode *node, tss_dtor_t destructor)
///
/// @brief Detroy a WindowsRadixTreeNode, its value, and all its sub-nodes.
///
/// @param node A pointer to a previously-allocated WindowsRadixTreeNode.
/// @param destructor A tss_dtor_t that can be used to destroy the value held by
///   the node.
///
/// @return This function always returns NULL.
static inline WindowsRadixTreeNode* windowsRadixTreeDestroyNode(
    WindowsRadixTreeNode *node, tss_dtor_t destructor
) {
    if (node != NULL) {
        // destructor is guaranteed to be non-NULL by the thread-specific
        // storage functions.
        destructor(InterlockedExchangePointer(
            (void * volatile*) &node->value, NULL));

        WindowsRadixTreeNode** radixTreeNodes = node->radixTreeNodes;
        // Recursively destroy any sub-nodes.
        for (uint32_t ii = 0; ii < RADIX_TREE_ARRAY_SIZE; ii++) {
            if (radixTreeNodes[ii] != NULL) {
                windowsRadixTreeDestroyNode(
                    InterlockedExchangePointer(
                        (void * volatile*) &radixTreeNodes[ii], NULL),
                    destructor);
            }
        }
        free(node); node = NULL;
    }

    return node;
}

/// @fn WindowsRadixTree* windowsRadixTreeDestroy(WindowsRadixTree *tree)
///
/// @brief Destroy a WindowsRadixTree.
///
/// @note We can't make this a static inline function because we need to pass
/// it as the tss_dtor_t pointer to windowsRadixTreeCreate for the first-level
/// trees.
///
/// @param tree A pointer to a previously-allocated WindowsRadixTree.
///
/// @return This function always returns NULL.
WindowsRadixTree* windowsRadixTreeDestroy(WindowsRadixTree *tree) {
    if (tree != NULL) {
        windowsRadixTreeDestroyNode(
            InterlockedExchangePointer((void * volatile*) &tree->root, NULL),
            tree->destructor);
        free(tree); tree = NULL;
    }
    return tree;
}

/// @fn void* windowsRadixTreeNodeGetValue(WindowsRadixTreeNode *node,
///         const volatile RADIX_TREE_KEY_ELEMENT *key, size_t numKeys)
///
/// @brief Get a previously-set value from a WindowsRadixTreeNode.
///
/// @param node A pointer to a previously-allocated WindowsRadixTreeNode or
///   NULL.
/// @param key A pointer to key element values.
/// @param numKeys The number of keys pointed to by the key.
///
/// @return Returns the value in the tree if it exists, NULL if not.
static inline void* windowsRadixTreeNodeGetValue(WindowsRadixTreeNode *node,
    const volatile RADIX_TREE_KEY_ELEMENT *key, size_t numKeys
) {
    const volatile void *returnValue = NULL;
    WindowsRadixTreeNode** radixTreeNodes = NULL;
    RADIX_TREE_KEY_ELEMENT currentKeyIndex = 0;

    RADIX_TREE_FIND_NODE();

    if (node != NULL) {
        returnValue = node->value;
    }

    return (void*) returnValue;
}

/// @fn void* windowsRadixTreeGetValue(WindowsRadixTree *tree,
///         const volatile void *key, size_t keySize)
///
/// @brief Get a value from a WindowsRadixTree.
///
/// @param tree A pointer to a previously-allocated WindowsRadixTree.
/// @param key A pointer to the key to look for.
/// @param keySize The number of bytes in the key.
///
/// @return Returns the value found on success, NULL if not found or on error.
static inline void* windowsRadixTreeGetValue(WindowsRadixTree *tree,
    const volatile void *key, size_t keySize
) {
    void *returnValue = NULL;
    if ((tree == NULL) || ((key == NULL) && (keySize != 0))) {
        return returnValue;
    }

    returnValue = windowsRadixTreeNodeGetValue(
        tree->root,
       (RADIX_TREE_KEY_ELEMENT*) key,
       (keySize >> RADIX_TREE_NUM_KEYS_BIT_SHIFT)
    );

    return returnValue;
}

/// @fn void* windowsRadixTreeGetValue2(WindowsRadixTree *tree1,
///         const volatile void *key1, size_t keySize1,
///         const volatile void *key2, size_t keySize2)
///
/// @brief Get a value from a second-level WindowsRadixTree.
///
/// @param tree1 A pointer to the top-level WindowsRadixTree.
/// @param key1 A pointer to the key of the second-level tree.
/// @param keySize1 The number of bytes in key1.
/// @param key1 A pointer to the key of value in the second-level tree.
/// @param keySize1 The number of bytes in key2.
///
/// @return Returns the found element on success, NULL on failure.
static inline void* windowsRadixTreeGetValue2(WindowsRadixTree *tree1,
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

    WindowsRadixTree *tree2
        = (WindowsRadixTree*) windowsRadixTreeGetValue(tree1, key1, keySize1);
    if (tree2 != NULL) {
        returnValue = windowsRadixTreeGetValue(tree2, key2, keySize2);
    }

    return returnValue;
}

/// @fn int windowsRadixTreeNodeSetValue(WindowsRadixTreeNode *node,
///         const volatile RADIX_TREE_KEY_ELEMENT *key, size_t numKeys,
///         volatile void *value)
///
/// @brief Set a value in the radix tree.  Allocate any intermediate nodes if
/// they don't exist.
///
/// @param node A pointer to a WindowsRadixTreeNode in the tree.
/// @param key An array of key elements.
/// @param numKeys The number of keys in the keys array.
/// @param value A pointer to the value to set in the tree.
///
/// @return Returns 0 if the value replaces an existing value in the tree, 1 if
/// the value is a new value in the tree.
static inline int windowsRadixTreeNodeSetValue(WindowsRadixTreeNode *node,
    const volatile RADIX_TREE_KEY_ELEMENT *key, size_t numKeys,
    volatile void *value
) {
    int returnValue = 0;
    RADIX_TREE_KEY_ELEMENT currentKeyIndex = 0;
    WindowsRadixTreeNode** radixTreeNodes = NULL;
    while (numKeys > 0) {
        currentKeyIndex = key[numKeys - 1];
        radixTreeNodes = node->radixTreeNodes;
        if (radixTreeNodes[currentKeyIndex] == NULL) {
            WindowsRadixTreeNode *radixTreeNode = (WindowsRadixTreeNode*)
                calloc(1, sizeof(WindowsRadixTreeNode));
            if (InterlockedCompareExchangePointer(
                (PVOID*) &radixTreeNodes[currentKeyIndex],
                (PVOID) radixTreeNode,
                NULL) != NULL
            ) {
                free(radixTreeNode); radixTreeNode = NULL;
            }
        }

        node = radixTreeNodes[currentKeyIndex];
        numKeys--;
    }

    if (node->value == NULL) {
        returnValue = 1;
    }
    node->value = value;

    return returnValue;
}

/// @fn int windowsRadixTreeSetValue(WindowsRadixTree *tree,
///         const volatile void *key, size_t keySize,
///         volatile void *value)
///
/// @brief Set a value in the radix tree.
///
/// @param tree A pointer to a previously-allocated WindowsRadixTree.
/// @param key A pointer to a key value.
/// @param keySize The number of bytes of the key.
/// @param value A pointer to the value to set.
///
/// @return Returns the number of new elements added on success, -1 on error.
static inline int windowsRadixTreeSetValue(WindowsRadixTree *tree,
    const volatile void *key, size_t keySize, volatile void *value
) {
    int returnValue = -1;
    if ((tree == NULL) || ((key == NULL) && (keySize != 0))) {
        return returnValue; // -1
    }

    returnValue = windowsRadixTreeNodeSetValue(
        tree->root,
        (RADIX_TREE_KEY_ELEMENT*) key,
        (keySize >> RADIX_TREE_NUM_KEYS_BIT_SHIFT),
        value
    );

    return returnValue;
}

/// @fn int windowsRadixTreeSetValue2(WindowsRadixTree *tree1,
///         const volatile void *key1, size_t keySize1,
///         const volatile void *key2, size_t keySize2,
///         volatile void *value, tss_dtor_t destructor2)
///
/// @brief Set a value in a second-level WindowsRadixTree.
///
/// @param tree1 A pointer to the top-level WindowsRadixTree.
/// @param key1 A pointer to the key of the second-level tree.
/// @param keySize1 The number of bytes in key1.
/// @param key1 A pointer to the key of value in the second-level tree.
/// @param keySize1 The number of bytes in key2.
/// @param destructor2 The tss_dtor_t for the second-level tree if a second-
///    level tree doesn't already exist for key1.
///
/// @return Returns the number of new elements added on success, -1 on error.
static inline int windowsRadixTreeSetValue2(WindowsRadixTree *tree1,
    const volatile void *key1, size_t keySize1,
    const volatile void *key2, size_t keySize2,
    volatile void *value, tss_dtor_t destructor2
) {
    int returnValue = -1;
    if ((tree1 == NULL)
        || ((key1 == NULL) && (keySize1 != 0))
        || ((key2 == NULL) && (keySize2 != 0))
    ) {
        return returnValue; // -1
    }

    WindowsRadixTree *tree2
        = (WindowsRadixTree*) windowsRadixTreeGetValue(tree1, key1, keySize1);
    if ((tree2 == NULL) && (destructor2 != NULL)) {
        WindowsRadixTree *newTree = windowsRadixTreeCreate(destructor2);
        windowsRadixTreeSetValue(tree1, key1, keySize1, newTree);
        tree2 = (WindowsRadixTree*) windowsRadixTreeGetValue(
            tree1, key1, keySize1);
        if (tree2 != newTree) {
            // We were in a race condition and something else overwrote our
            // tree.  Delete the one we created.
            newTree = windowsRadixTreeDestroy(newTree);
            // Get it again just to be sure.
            tree2 = (WindowsRadixTree*) windowsRadixTreeGetValue(
                tree1, key1, keySize1);
        }
    }
    
    if (tree2 == NULL) {
        // There's no tree and we have nothing to construct the missing tree
        // with.  We can't work like this.  Fail.
        return returnValue; // -1
    }

    returnValue = windowsRadixTreeSetValue(tree2, key2, keySize2, value);

    return returnValue;
}

/// @fn int windowsRadixTreeNodeDeleteValue(WindowsRadixTreeNode *node,
///         const volatile RADIX_TREE_KEY_ELEMENT *key, size_t numKeys,
///         tss_tdor_t destructor)
///
/// @brief Delete a previously-set value from a WindowsRadixTreeNode.
///
/// @param node A pointer to a previously-allocated WindowsRadixTreeNode or
///   NULL.
/// @param key A pointer to key element values.
/// @param numKeys The number of keys pointed to by the key.
/// @param destructor The destructor to call on the value, if found.
///
/// @return Returns the number of elements found and deleted (0 or 1).
static inline int windowsRadixTreeNodeDeleteValue(WindowsRadixTreeNode *node,
    const volatile RADIX_TREE_KEY_ELEMENT *key, size_t numKeys,
    tss_dtor_t destructor
) {
    int returnValue = 0;
    RADIX_TREE_KEY_ELEMENT currentKeyIndex = 0;
    WindowsRadixTreeNode** radixTreeNodes = NULL;

    RADIX_TREE_FIND_NODE();

    if (node != NULL) {
        // destructor is guaranteed to be non-NULL by the thread-specific
        // storage functions.
        destructor(InterlockedExchangePointer(
            (void * volatile*) &node->value, NULL)
        );
        returnValue = 1;
    }

    return returnValue;
}

/// @fn int windowsRadixTreeDeleteValue(WindowsRadixTree *tree,
///         const volatile void *key, size_t keySize)
///
/// @brief Delete a value from a WindowsRadixTree.
///
/// @param tree A pointer to a previously-allocated WindowsRadixTree.
/// @param key A pointer to the key to look for.
/// @param keySize The number of bytes in the key.
///
/// @return Returns the number of elements found and deleted on success, -1 on
/// error.
static inline int windowsRadixTreeDeleteValue(WindowsRadixTree *tree,
    const volatile void *key, size_t keySize
) {
    int returnValue = -1;
    if ((tree == NULL) || ((key == NULL) && (keySize != 0))) {
        return returnValue; // -1
    }

    returnValue = windowsRadixTreeNodeDeleteValue(
        tree->root,
        (RADIX_TREE_KEY_ELEMENT*) key,
        (keySize >> RADIX_TREE_NUM_KEYS_BIT_SHIFT),
        tree->destructor
    );

    return returnValue;
}

/// @fn int windowsRadixTreeDeleteValue2(WindowsRadixTree *tree1,
///         const volatile void *key1, size_t keySize1,
///         const volatile void *key2, size_t keySize2)
///
/// @brief Delete a value from a second-level WindowsRadixTree.
///
/// @param tree1 A pointer to the top-level WindowsRadixTree.
/// @param key1 A pointer to the key of the second-level tree.
/// @param keySize1 The number of bytes in key1.
/// @param key1 A pointer to the key of value in the second-level tree.
/// @param keySize1 The number of bytes in key2.
///
/// @return Returns the number of elements found and deleted on success, -1 on
/// error.
static inline int windowsRadixTreeDeleteValue2(WindowsRadixTree *tree1,
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

    WindowsRadixTree *tree2
        = (WindowsRadixTree*) windowsRadixTreeGetValue(tree1, key1, keySize1);
    if (tree2 != NULL) {
        returnValue = windowsRadixTreeDeleteValue(tree2, key2, keySize2);
    }

    return returnValue;
}

/******************** End Support Data Structure Code *************************/

// Support tss_dtor_t function that does nothing.
static void winCThreadsNullFunction(void *parameter) {
    (void) parameter;
    return;
}


// Call once support.
void call_once(once_flag* flag, void(*func)(void)) {
    LONG currentFlagValue
        = InterlockedCompareExchange(flag, ONCE_FLAG_RUNNING, ONCE_FLAG_INIT);

    if (currentFlagValue == ONCE_FLAG_COMPLETE) {
        // This is the expected case, so put it first.
        return;
    }
    else if (currentFlagValue == ONCE_FLAG_INIT) {
        func();
        *flag = ONCE_FLAG_COMPLETE;
    }
    else if (currentFlagValue == ONCE_FLAG_RUNNING) {
        while (*flag == ONCE_FLAG_RUNNING);
    }

    return;
}


// Mutex support.
int mtx_init(mtx_t* mtx, int type) {
    if (mtx == NULL) {
        return thrd_error;
    }

    mtx->attribs = type;
    // Timed mutexes need the handle.  Everything else uses only the criticalSection.
    mtx->handle = CreateMutex(
        NULL,  // default security attributes
        FALSE, // initially not owned
        NULL); // unnamed mutex
    // Initialize the CRITICAL_SECTION
    InitializeCriticalSection(&mtx->criticalSection);
    mtx->initialized = true;

    return thrd_success;
}

// See if we need to atomically initialize the mutex
static inline void ensureMutexInitialized(mtx_t* mtx) {
    if (mtx->initialized == false) {
        // Initialize the HANDLE
        // Idea for this procedure came from Dr. Alex RE's answer to
        // https://stackoverflow.com/questions/3555859/is-it-possible-to-do-static-initialization-of-mutexes-in-windows
        HANDLE mtxInit = CreateMutex(NULL, FALSE, NULL);
        if (InterlockedCompareExchangePointer((PVOID*)&mtx->handle, (PVOID)mtxInit, NULL) == NULL) {
            // Initialize the CRITICAL_SECTION
            InitializeCriticalSection(&mtx->criticalSection);
            mtx->initialized = true;
        }
        else {
            // mtx->handle was already initialized.  Close the mutex we just created.
            CloseHandle(mtxInit);
            while (!mtx->initialized);
        }
    }
}

int mtx_lock(mtx_t* mtx) {
    int returnValue = thrd_success;

    ensureMutexInitialized(mtx);

    if ((mtx->attribs & mtx_timed) != 0) {
        if ((mtx->attribs & mtx_recursive) == 0) {
            // We're not a recurisve mutex, so block on the lock.
            DWORD waitResult = WaitForSingleObject(
                mtx->handle, // handle to mutex
                INFINITE);   // no time-out interval
            if (waitResult != WAIT_OBJECT_0) {
                returnValue = thrd_error;
            }
        }
        else {
            // We're a recursive mutex.  We could be locked by another function
            // on this thread.  Try to lock, but don't block on it.  This is
            // strictly for the purposes of compatibility with mtx_timedlock,
            // so if it fails, we don't care.  If we're truly locked by another
            // thread, the criticalSection below will gate us.
            WaitForSingleObject(
                mtx->handle, // handle to mutex
                0);          // do not block
        }
    }
    
    if (returnValue == thrd_success) {
        EnterCriticalSection(&mtx->criticalSection);
    }

    return returnValue;
}

int mtx_timedlock(mtx_t* mtx, const struct timespec* ts) {
    struct timespec now;
    DWORD waitms = 0;

    if ((mtx == NULL) || (ts == NULL) || ((mtx->attribs & mtx_timed) == 0)) {
        // We can't work with this.
        return thrd_error;
    }

    ensureMutexInitialized(mtx);

    if (timespec_get(&now, TIME_UTC) == 0) {
        uint64_t nowns = (now.tv_sec * 1000000000) + now.tv_nsec;
        uint64_t tsns = (ts->tv_sec * 1000000000) + ts->tv_nsec;
        uint64_t waitns = tsns - nowns;
        waitms = (DWORD)(waitns / 1000000);
    }
    else {
        // timespec_get returned an error.  We have no valid time to wait.
        waitms = 0;
    }
    
    DWORD waitResult = WaitForSingleObject(mtx->handle, waitms);

    if (waitResult == WAIT_OBJECT_0) {
        EnterCriticalSection(&mtx->criticalSection);
        return thrd_success;
    }
    else if (waitResult == WAIT_TIMEOUT) {
        return thrd_timedout;
    }
    else {
        return thrd_error;
    }
}

int mtx_trylock(mtx_t* mtx) {
    DWORD waitResult = WAIT_OBJECT_0;
    
    ensureMutexInitialized(mtx);

    if ((mtx->attribs & mtx_timed) != 0) {
        waitResult = WaitForSingleObject(mtx->handle, 0);
    }

    if (waitResult == WAIT_OBJECT_0) {
        EnterCriticalSection(&mtx->criticalSection);
        return thrd_success;
    }
    else if (waitResult == WAIT_TIMEOUT) {
        return thrd_busy;
    }
    else {
        return thrd_error;
    }
}

int mtx_unlock(mtx_t* mtx) {
    int returnValue = thrd_success;

    if (mtx->initialized == true) {
        LeaveCriticalSection(&mtx->criticalSection);
        
        if (((mtx->attribs & mtx_timed) != 0) && (ReleaseMutex(mtx->handle) == 0)) {
            returnValue = thrd_error;
        }
    }
    else {
        returnValue = thrd_error;
    }
    
    return returnValue;
}

void mtx_destroy(mtx_t* mtx) {
    mtx->initialized = false;
    CloseHandle(mtx->handle);
    DeleteCriticalSection(&mtx->criticalSection);
}


// Condition support.
int cnd_broadcast(cnd_t* cond) {
    WakeAllConditionVariable(cond);
    return thrd_success;
}

void cnd_destroy(cnd_t* cond) {
    (void) cond;
    // No-op.
}

int cnd_init(cnd_t* cond) {
    InitializeConditionVariable(cond);
    return thrd_success;
}

int cnd_signal(cnd_t* cond) {
    WakeConditionVariable(cond);
    return thrd_success;
}

int cnd_timedwait(cnd_t* cond, mtx_t* mtx, const struct timespec* ts) {
    int returnValue = thrd_success;
    uint64_t durationns = (ts->tv_sec * 1000000000) + ts->tv_nsec;
    DWORD durationms = (DWORD)(durationns / 1000000);

    if (SleepConditionVariableCS(cond, &mtx->criticalSection, durationms) == 0) {
        if (GetLastError() == ERROR_TIMEOUT) {
            returnValue = thrd_timedout;
        }
        else {
            returnValue = thrd_error;
        }
    }

    return returnValue;
}

int cnd_wait(cnd_t* cond, mtx_t* mtx) {
    int returnValue = thrd_success;

    if (SleepConditionVariableCS(cond, &mtx->criticalSection, INFINITE) == 0) {
        returnValue = thrd_error;
    }

    return returnValue;
}


// Thread-specific storage support.
typedef struct TssId {
    thrd_t thread;
    tss_t key;
} TssId;

static WindowsRadixTree **tssStorageByKey = NULL;
static WindowsRadixTree * tssStorageByThread = NULL;
static tss_t tssIndex = 1;
static once_flag tssMetadataOnceFlag = ONCE_FLAG_INIT;
static bool tssMetadataInitialized = false;

void tssIdDestroy(TssId *tssId) {
    if (tssId != NULL) {
        windowsRadixTreeDeleteValue(
            tssStorageByKey[tssId->key], &tssId->thread, sizeof(tssId->thread));
        free(tssId); tssId = NULL;
    }
}

void initializeTssMetadata(void) {
    tssStorageByKey = (WindowsRadixTree**) calloc(
        1, ARRAY_OF_RADIX_TREES_SIZE * sizeof(WindowsRadixTree*));
    if (tssStorageByKey == NULL) {
        // No tree.  Can't proceed.
        LOG_MALLOC_FAILURE();
        exit(1);
    }

    tssStorageByThread = windowsRadixTreeCreate(
        (tss_dtor_t) ((void*) windowsRadixTreeDestroy));
    if (tssStorageByThread == NULL) {
        // No tree.  Can't proceed.
        LOG_MALLOC_FAILURE();
        exit(1);
    }

    tssMetadataInitialized = true;
}

int tss_create(tss_t* key, tss_dtor_t dtor) {
    call_once(&tssMetadataOnceFlag, initializeTssMetadata);

    if (tssIndex == 0) {
        // We've created all the thread-specific storage we can.  Fail.
        return thrd_error;
    }

    if (dtor == NULL) {
        dtor = winCThreadsNullFunction;
    }

    tssStorageByKey[tssIndex] = windowsRadixTreeCreate(dtor);

    *key = tssIndex;
    tssIndex++;

    return thrd_success;
}

void tss_delete(tss_t key) {
    if (tssMetadataInitialized == false) {
        // Thread-specific storage has not been initialized.  Nothing to do.
        printLog(DEBUG, "Key storage not initialized.\n");
        return;
    }

    tssStorageByKey[key]
        = windowsRadixTreeDestroy(tssStorageByKey[key]);

    return;
}

void* tss_get(tss_t key) {
    if (tssMetadataInitialized == false) {
        // Thread-specific storage has not been initialized.  Nothing to do.
        printLog(DEBUG, "Key storage not initialized.\n");
        return NULL;
    }

    thrd_t thisThread = thrd_current();
    void *returnValue = windowsRadixTreeGetValue(
        tssStorageByKey[key], &thisThread, sizeof(thisThread));

    return returnValue;
}

int tss_set(tss_t key, void* val) {
    int returnValue = thrd_error;
    if (tssMetadataInitialized == false) {
        // Thread-specific storage has not been initialized.  Nothing to do.
        printLog(DEBUG, "Key storage not initialized.\n");
        return returnValue; // thrd_error
    }

    thrd_t thisThread = thrd_current();
    do {
        if (windowsRadixTreeSetValue(
            tssStorageByKey[key], &thisThread, sizeof(thisThread), val) < 0
        ) {
            // returnValue is thrd_error
            break;
        }

        TssId *tssId = (TssId*) windowsRadixTreeGetValue2(tssStorageByThread,
            &thisThread, sizeof(thisThread), &key, sizeof(key));
        if (tssId == NULL) {
            TssId *newTssId = (TssId*) malloc(sizeof(TssId));
            if (windowsRadixTreeSetValue2(tssStorageByThread,
                &thisThread, sizeof(thisThread), &key, sizeof(key),
                newTssId, (tss_dtor_t) ((void*) tssIdDestroy)) < 0
            ) {
                // This should be impossible.  Bail.
                // returnValue is thrd_error
                break;
            }
            tssId = (TssId*) windowsRadixTreeGetValue2(tssStorageByThread,
                &thisThread, sizeof(thisThread), &key, sizeof(key));
            if (tssId == NULL) {
                // Something is very wrong.  Fail.
                // returnValue is thrd_error
                break;
            }

            tssId->thread = thisThread;
            tssId->key = key;
        }
        returnValue = thrd_success;
    } while (0);

    return returnValue;
}


// Thread support.
static WindowsRadixTree* attachedThreads = NULL;

typedef struct WindowsCreateWrapperArgs {
  thrd_start_t func;
  void *arg;
} WindowsCreateWrapperArgs;

DWORD __stdcall windows_create_wrapper(LPVOID wrapper_args) {
    printLog(TRACE, "ENTER windows_create_wrapper(wrapper_args=%p)\n",
        wrapper_args);

    WindowsCreateWrapperArgs *cthread_args
        = (WindowsCreateWrapperArgs*) wrapper_args;
    thrd_start_t func = cthread_args->func;
    void *arg = cthread_args->arg;
    free(cthread_args); cthread_args = NULL;
    
    int return_value = func(arg);

    printLog(TRACE, "EXIT windows_create_wrapper(wrapper_args=%p) = {%d}\n",
        wrapper_args, return_value);
    thrd_exit(return_value);
    return (DWORD) return_value;
}

int thrd_create(thrd_t* thr, thrd_start_t func, void* arg) {
    printLog(TRACE, "ENTER thrd_create(thr=%p, func=%p, arg=%p)\n",
        thr, func, arg);

    if (thr == NULL) {
      printLog(TRACE, "EXIT thrd_create(thr=%p, func=%p, arg=%p) = {%d}\n",
        thr, func, arg, thrd_error);
      return thrd_error;
    }

    int returnValue = thrd_success;
    WindowsCreateWrapperArgs *wrapper_args
        = calloc(1, sizeof(WindowsCreateWrapperArgs));
    if (wrapper_args == NULL) {
        // Can't allocate enough memory to start the thread.
        LOG_MALLOC_FAILURE();
        exit(1);
        printLog(TRACE, "EXIT thrd_create(thr=%p, func=%p, arg=%p) = {%d}\n",
            thr, func, arg, thrd_error);
        return thrd_error;
    }
    wrapper_args->func = func;
    wrapper_args->arg = arg;

    HANDLE threadHandle = NULL;
    if (func != NULL) {
        threadHandle = CreateThread(
            NULL,                         //lpThreadAttributes - NULL indicates the handle returned cannot be inherited by child processes
            0,                            // dwStackSize - 0 is default stack size
            windows_create_wrapper,       // lpStartAddress
            wrapper_args,                 // lpParameter
            0,                            // dwCreationFlags - A value of 0 means the thread starts immediately
            thr                           // lpThreadId
        );
    }

    if (threadHandle != NULL) {
        if (attachedThreads == NULL) {
            attachedThreads
                = windowsRadixTreeCreate(winCThreadsNullFunction);
            if (attachedThreads == NULL) {
                LOG_MALLOC_FAILURE();
                exit(1);
                return thrd_error;
            }
        }

        windowsRadixTreeSetValue(
            attachedThreads, thr, sizeof(*thr), threadHandle);
    } else {
        returnValue = thrd_error;
    }

    printLog(TRACE, "EXIT thrd_create(thr=%p, func=%p, arg=%p) = {%d}\n",
        thr, func, arg, returnValue);
    return returnValue;
}

thrd_t thrd_current(void) {
    return GetCurrentThreadId();
}

int thrd_detach(thrd_t thr) {
    int returnValue = thrd_success;
    HANDLE threadHandle = NULL;

    threadHandle = (HANDLE) windowsRadixTreeGetValue(
        attachedThreads, &thr, sizeof(thr));
    if (threadHandle != NULL) {
        CloseHandle(threadHandle);
        windowsRadixTreeDeleteValue(attachedThreads, &thr, sizeof(thr));
    }
    else {
        returnValue = thrd_error;
    }

    return returnValue;
}

int thrd_equal(thrd_t thr0, thrd_t thr1) {
    return (thr0 == thr1);
}

void thrd_exit(int res) {
    printLog(TRACE, "ENTER thrd_exit(res=%d)\n", res);

    // Destroy all the thread local storage.
    if (tssMetadataInitialized == true) {
        thrd_t thisThread = thrd_current();
        windowsRadixTreeDeleteValue(
            tssStorageByThread, &thisThread, sizeof(thisThread));
    }

    printLog(TRACE, "EXIT thrd_exit(res=%d) = {}\n", res);
    ExitThread(res);
}

int thrd_join(thrd_t thr, int* res) {
    int returnValue = thrd_success;
    HANDLE threadHandle = (HANDLE) windowsRadixTreeGetValue(
        attachedThreads, &thr, sizeof(thr));

    if (threadHandle != NULL) {
        DWORD waitResult = WaitForSingleObject(
            threadHandle,  // handle to thread
            INFINITE);     // no time-out interval
        if (waitResult == WAIT_OBJECT_0) {
            if (res != NULL) {
                DWORD dwres = 0;
                if (GetExitCodeThread(threadHandle, &dwres) == TRUE) {
                    *res = (int)dwres;
                }
                else {
                    returnValue = thrd_error;
                }
            }
        }
        else {
            returnValue = thrd_error;
        }

        windowsRadixTreeDeleteValue(attachedThreads, &thr, sizeof(thr));
        CloseHandle(threadHandle);
    }
    else {
        returnValue = thrd_error;
    }

    return returnValue;
}

int thrd_sleep(const struct timespec* duration, struct timespec* remaining) {
    if (duration == NULL) {
        // Error case.
        return -1;
    }

    uint64_t durationns = (duration->tv_sec * 1000000000) + duration->tv_nsec;
    DWORD durationms = (DWORD)(durationns / 1000000);
    Sleep(durationms);
    if (remaining != NULL) {
        remaining->tv_sec = 0;
        remaining->tv_nsec = 0;
    }

    return 0;
}

void thrd_yield(void) {
    Sleep(0);
}

int thrd_terminate(thrd_t thr) {
    int returnValue = thrd_success;
    HANDLE threadHandle = (HANDLE) windowsRadixTreeGetValue(
      attachedThreads, &thr, sizeof(thr));

    if (threadHandle != NULL) {
        BOOL success = TerminateThread(threadHandle, thrd_terminated);
        if (success == 0) {
            returnValue = thrd_error;
        }
    } else {
        // Thread not found.
        returnValue = thrd_error;
    }

    return returnValue;
}

/// @fn int timespec_get(struct timespec* spec, int base)
///
/// @brief Get the system time in seconds and nanoseconds.
/// This was taken from:
/// https://stackoverflow.com/questions/5404277/porting-clock-gettime-to-windows
///
/// @param spec The struct timespec structure to fill from the calling function.
/// @param base The base to use as defined on page 287 of the ISO/IEC 9899_2018
///   spec.
///
/// @return This function always returns base.
///
/// @note This only produces a time value down to 1/10th of a microsecond.
int timespec_get(struct timespec* spec, int base) {
    __int64 wintime = 0;
    FILETIME filetime = { 0, 0 };

    GetSystemTimeAsFileTime(&filetime);
    wintime = (((__int64) filetime.dwHighDateTime) << 32)
        | ((__int64) filetime.dwLowDateTime);

    wintime -= 116444736000000000LL;       // 1-Jan-1601 to 1-Jan-1970
    spec->tv_sec = wintime / 10000000LL;       // seconds
    spec->tv_nsec = wintime % 10000000LL * 100; // nano-seconds

    return base;
}

#endif // _WIN32

