////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Copyright (c) 2012-2023 James Card                     //
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

#ifdef _MSC_VER
#include "WinCThreads.h"
#include <stdio.h>

#ifdef LOGGING_ENABLED

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
            localtime_s(&nowStruct, &now.tv_sec); \
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
    ERR, // ERROR conflicts with something in Windows.h
    CRITICAL,
    BOX,
    BANNER,
    NONE,
    NUM_LOG_LEVELS
} LogLevel;

const char *logLevelName[NUM_LOG_LEVELS] = {
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

#else // LOGGING_ENABLED not defined

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


// A note about why the red black tree support data structure is here:
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
// requirements of the standard.  The red black tree is used to achieve that.
//
// In this library, thread-specific storage is implemented as a red black tree
// of red black trees.  In the first level trees, the keys are the tss_t values
// and the values of the nodes are data structures containing function pointers
// to the destructors and a pointer to a second level red black tree.  In the
// second level trees, the keys are the thread IDs and the values are pointers
// to the thread-specific values.
//
// Access to the trees is protected by a mutex.  This mutex is a mtx_t as
// implemented in this library.  This guarantees that the trees do not get
// corrupted by multiple threads trying to access their local storage at the
// same time (which is actually a rare event).
//
// The destructors provided in tss_create are called when a thread directly
// calls thrd_exit or when it returns from its main function, which is wrapped
// by a fucntion that invokes thrd_exit upon the functions's return.

/****************** Begin Support Data Structure Code **************************
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that neither the name of Emin
Martinian nor the names of any contributors are be used to endorse or
promote products derived from this software without specific prior
written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

typedef struct RedBlackNode {
    void* key;
    void* value;
    int red; // if red = 0 then the node is black
    struct RedBlackNode* left;
    struct RedBlackNode* right;
    struct RedBlackNode* parent;
    struct RedBlackNode* next;
    struct RedBlackNode* prev;
} RedBlackNode;

/* Compare(a,b) should return 1 if *a > *b, -1 if *a < *b, and 0 otherwise */
/* Destroy(a) takes a pointer to whatever key might be and frees it accordingly */
typedef struct RedBlackTree {
    int (*compare)(const void* a, const void* b);
    void (*destroyKey)(void* a);
    void (*destroyValue)(void* a);
    /*  A sentinel is used for root and for nil.  These sentinels are */
    /*  created when rbTreeCreate is caled.  root->left should always */
    /*  point to the node which is the root of the tree.  nil points to a */
    /*  node which should always be black but has aribtrary children and */
    /*  parent and no key or info.  The point of using these sentinels is so */
    /*  that the root and nil nodes do not require special cases in the code */
    RedBlackNode* root;
    RedBlackNode* nil;
    RedBlackNode* head;
    RedBlackNode* tail;
} RedBlackTree;

static RedBlackNode* rbTreeSuccessor(RedBlackTree* tree, RedBlackNode* x);
static RedBlackNode* rbTreePredecessor(RedBlackTree* tree, RedBlackNode* x);

/***********************************************************************/
/*  FUNCTION:  rbTreeSafeMalloc */
/**/
/*    INPUTS:  size is the size to malloc */
/**/
/*    OUTPUT:  returns pointer to allocated memory if succesful */
/**/
/*    EFFECT:  mallocs new memory.  If malloc fails, prints error message */
/*             and exits the program. */
/**/
/*    Modifies Input: none */
/**/
/***********************************************************************/

static void* rbTreeSafeMalloc(size_t size) {
    void* result;

    if ((result = calloc(1, size))) { /* assignment intentional */
        return result;
    }
    else {
        LOG_MALLOC_FAILURE();
        exit(-1);
        return NULL;
    }
}


/***********************************************************************/
/*  FUNCTION:  rbTreeCreate */
/**/
/*  INPUTS:  All the inputs are names of functions.  CompFunc takes to */
/*  void pointers to keys and returns > 0 if the first arguement is */
/*  "greater than" the second.   DestFunc takes a pointer to a key and */
/*  destroys it in the appropriate manner when the node containing that */
/*  key is deleted.  InfoDestFunc is similiar to DestFunc except it */
/*  recieves a pointer to the info of a node and destroys it. */
/**/
/*  OUTPUT:  This function returns a pointer to the newly created */
/*  red-black tree. */
/**/
/*  Modifies Input: none */
/***********************************************************************/

static RedBlackTree* rbTreeCreate(int (*CompFunc) (const void*, const void*),
    void (*DestFunc) (void*),
    void (*InfoDestFunc) (void*)) {
    RedBlackTree* newTree;
    RedBlackNode* temp;

    newTree = (RedBlackTree*)rbTreeSafeMalloc(sizeof(RedBlackTree));
    newTree->compare = CompFunc;
    newTree->destroyKey = DestFunc;
    newTree->destroyValue = InfoDestFunc;

    /*  see the comment in the RedBlackTree structure in red_black_tree.h */
    /*  for information on nil and root */
    temp = newTree->nil = (RedBlackNode*)rbTreeSafeMalloc(sizeof(RedBlackNode));
    temp->parent = temp->left = temp->right = temp;
    temp->red = 0;
    temp->key = 0;
    temp = newTree->root = (RedBlackNode*)rbTreeSafeMalloc(sizeof(RedBlackNode));
    temp->parent = temp->left = temp->right = newTree->nil;
    temp->key = 0;
    temp->red = 0;
    newTree->head = newTree->tail = newTree->nil;
    return(newTree);
}

/***********************************************************************/
/*  FUNCTION:  rbTreeLeftRotate */
/**/
/*  INPUTS:  This takes a tree so that it can access the appropriate */
/*           root and nil pointers, and the node to rotate on. */
/**/
/*  OUTPUT:  None */
/**/
/*  Modifies Input: tree, x */
/**/
/*  EFFECTS:  Rotates as described in _Introduction_To_Algorithms by */
/*            Cormen, Leiserson, Rivest (Chapter 14).  Basically this */
/*            makes the parent of x be to the left of x, x the parent of */
/*            its parent before the rotation and fixes other pointers */
/*            accordingly. */
/***********************************************************************/

static void rbTreeLeftRotate(RedBlackTree* tree, RedBlackNode* x) {
    RedBlackNode* y;
    RedBlackNode* nil = tree->nil;

    /*  I originally wrote this function to use the sentinel for */
    /*  nil to avoid checking for nil.  However this introduces a */
    /*  very subtle bug because sometimes this function modifies */
    /*  the parent pointer of nil.  This can be a problem if a */
    /*  function which calls rbTreeLeftRotate also uses the nil sentinel */
    /*  and expects the nil sentinel's parent pointer to be unchanged */
    /*  after calling this function.  For example, when rbTreeDeleteFixUP */
    /*  calls rbTreeLeftRotate it expects the parent pointer of nil to be */
    /*  unchanged. */

    y = x->right;
    x->right = y->left;

    if (y->left != nil) y->left->parent = x; /* used to use sentinel here */
    /* and do an unconditional assignment instead of testing for nil */

    y->parent = x->parent;

    /* instead of checking if x->parent is the root as in the book, we */
    /* count on the root sentinel to implicitly take care of this case */
    if (x == x->parent->left) {
        x->parent->left = y;
    }
    else {
        x->parent->right = y;
    }
    y->left = x;
    x->parent = y;

#ifdef DEBUG_ASSERT
    Assert(!tree->nil->red, "nil not red in rbTreeLeftRotate");
#endif
}


/***********************************************************************/
/*  FUNCTION:  rbTreeRightRotate */
/**/
/*  INPUTS:  This takes a tree so that it can access the appropriate */
/*           root and nil pointers, and the node to rotate on. */
/**/
/*  OUTPUT:  None */
/**/
/*  Modifies Input?: tree, y */
/**/
/*  EFFECTS:  Rotates as described in _Introduction_To_Algorithms by */
/*            Cormen, Leiserson, Rivest (Chapter 14).  Basically this */
/*            makes the parent of x be to the left of x, x the parent of */
/*            its parent before the rotation and fixes other pointers */
/*            accordingly. */
/***********************************************************************/

static void rbTreeRightRotate(RedBlackTree* tree, RedBlackNode* y) {
    RedBlackNode* x;
    RedBlackNode* nil = tree->nil;

    /*  I originally wrote this function to use the sentinel for */
    /*  nil to avoid checking for nil.  However this introduces a */
    /*  very subtle bug because sometimes this function modifies */
    /*  the parent pointer of nil.  This can be a problem if a */
    /*  function which calls rbTreeLeftRotate also uses the nil sentinel */
    /*  and expects the nil sentinel's parent pointer to be unchanged */
    /*  after calling this function.  For example, when rbTreeDeleteFixUP */
    /*  calls rbTreeLeftRotate it expects the parent pointer of nil to be */
    /*  unchanged. */

    x = y->left;
    y->left = x->right;

    if (nil != x->right)  x->right->parent = y; /*used to use sentinel here */
    /* and do an unconditional assignment instead of testing for nil */

    /* instead of checking if x->parent is the root as in the book, we */
    /* count on the root sentinel to implicitly take care of this case */
    x->parent = y->parent;
    if (y == y->parent->left) {
        y->parent->left = x;
    }
    else {
        y->parent->right = x;
    }
    x->right = y;
    y->parent = x;

#ifdef DEBUG_ASSERT
    Assert(!tree->nil->red, "nil not red in rbTreeRightRotate");
#endif
}

/***********************************************************************/
/*  FUNCTION:  rbTreeInsertHelp  */
/**/
/*  INPUTS:  tree is the tree to insert into and z is the node to insert */
/**/
/*  OUTPUT:  none */
/**/
/*  Modifies Input:  tree, z */
/**/
/*  EFFECTS:  Inserts z into the tree as if it were a regular binary tree */
/*            using the algorithm described in _Introduction_To_Algorithms_ */
/*            by Cormen et al.  This funciton is only intended to be called */
/*            by the rbTreeInsert function and not by the user */
/***********************************************************************/

static void rbTreeInsertHelp(RedBlackTree* tree, RedBlackNode* z) {
    /*  This function should only be called by InsertRBTree (see above) */
    RedBlackNode* x;
    RedBlackNode* y;
    RedBlackNode* nil = tree->nil;

    z->left = z->right = nil;
    y = tree->root;
    x = tree->root->left;
    while (x != nil) {
        y = x;
        if (0 < tree->compare(x->key, z->key)) { /* x.key > z.key */
            x = x->left;
        }
        else { /* x,key <= z.key */
            x = x->right;
        }
    }
    z->parent = y;
    if ((y == tree->root) ||
        (0 < tree->compare(y->key, z->key))) { /* y.key > z.key */
        y->left = z;
    }
    else {
        y->right = z;
    }

#ifdef DEBUG_ASSERT
    Assert(!tree->nil->red, "nil not red in rbTreeInsertHelp");
#endif
}

/*  Before calling Insert RBTree the node x should have its key set */

/***********************************************************************/
/*  FUNCTION:  rbTreeInsert */
/**/
/*  INPUTS:  tree is the red-black tree to insert a node which has a key */
/*           pointed to by key and info pointed to by info.  */
/**/
/*  OUTPUT:  This function returns a pointer to the newly inserted node */
/*           which is guarunteed to be valid until this node is deleted. */
/*           What this means is if another data structure stores this */
/*           pointer then the tree does not need to be searched when this */
/*           is to be deleted. */
/**/
/*  Modifies Input: tree */
/**/
/*  EFFECTS:  Creates a node node which contains the appropriate key and */
/*            info pointers and inserts it into the tree. */
/***********************************************************************/

static RedBlackNode* rbTreeInsert(RedBlackTree* tree, void* key, void* info) {
    RedBlackNode* y;
    RedBlackNode* x;
    RedBlackNode* newNode;

    x = (RedBlackNode*)rbTreeSafeMalloc(sizeof(RedBlackNode));
    x->key = key;
    x->value = info;

    rbTreeInsertHelp(tree, x);
    newNode = x;
    x->red = 1;
    while (x->parent->red) { /* use sentinel instead of checking for root */
        if (x->parent == x->parent->parent->left) {
            y = x->parent->parent->right;
            if (y->red) {
                x->parent->red = 0;
                y->red = 0;
                x->parent->parent->red = 1;
                x = x->parent->parent;
            }
            else {
                if (x == x->parent->right) {
                    x = x->parent;
                    rbTreeLeftRotate(tree, x);
                }
                x->parent->red = 0;
                x->parent->parent->red = 1;
                rbTreeRightRotate(tree, x->parent->parent);
            }
        }
        else { /* case for x->parent == x->parent->parent->right */
            y = x->parent->parent->left;
            if (y->red) {
                x->parent->red = 0;
                y->red = 0;
                x->parent->parent->red = 1;
                x = x->parent->parent;
            }
            else {
                if (x == x->parent->left) {
                    x = x->parent;
                    rbTreeRightRotate(tree, x);
                }
                x->parent->red = 0;
                x->parent->parent->red = 1;
                rbTreeLeftRotate(tree, x->parent->parent);
            }
        }
    }
    tree->root->left->red = 0;

    RedBlackNode* neighbor = rbTreePredecessor(tree, newNode);
    if (neighbor != tree->nil) {
        neighbor->next = newNode;
    }
    newNode->prev = neighbor;
    neighbor = rbTreeSuccessor(tree, newNode);
    if (neighbor != tree->nil) {
        neighbor->prev = newNode;
    }
    newNode->next = neighbor;
    if (newNode->prev == tree->nil) {
        // First node in the tree
        tree->head = newNode;
    }
    if (newNode->next == tree->nil) {
        // Last node in the tree
        tree->tail = newNode;
    }

    return(newNode);

#ifdef DEBUG_ASSERT
    Assert(!tree->nil->red, "nil not red in rbTreeInsert");
    Assert(!tree->root->red, "root not red in rbTreeInsert");
#endif
}

/***********************************************************************/
/*  FUNCTION:  rbTreeSuccessor  */
/**/
/*    INPUTS:  tree is the tree in question, and x is the node we want the */
/*             the successor of. */
/**/
/*    OUTPUT:  This function returns the successor of x or tree->nil if no */
/*             successor exists. */
/**/
/*    Modifies Input: none */
/**/
/*    Note:  uses the algorithm in _Introduction_To_Algorithms_ */
/***********************************************************************/
  
static RedBlackNode* rbTreeSuccessor(RedBlackTree* tree, RedBlackNode* x) {
    RedBlackNode* y;
    RedBlackNode* nil = tree->nil;
    RedBlackNode* root = tree->root;

    if (nil != (y = x->right)) { /* assignment to y is intentional */
        while (y->left != nil) { /* returns the minium of the right subtree of x */
            y = y->left;
        }
        return(y);
    }
    else {
        y = x->parent;
        while (x == y->right) { /* sentinel used instead of checking for nil */
            x = y;
            y = y->parent;
        }
        if (y == root) return(nil);
        return(y);
    }
}

/***********************************************************************/
/*  FUNCTION:  rbTreePredecessor  */
/**/
/*    INPUTS:  tree is the tree in question, and x is the node we want the */
/*             the predecessor of. */
/**/
/*    OUTPUT:  This function returns the predecessor of x or tree->nil if no */
/*             predecessor exists. */
/**/
/*    Modifies Input: none */
/**/
/*    Note:  uses the algorithm in _Introduction_To_Algorithms_ */
/***********************************************************************/

static RedBlackNode* rbTreePredecessor(RedBlackTree* tree, RedBlackNode* x) {
    RedBlackNode* y;
    RedBlackNode* nil = tree->nil;
    RedBlackNode* root = tree->root;

    if (nil != (y = x->left)) { /* assignment to y is intentional */
        while (y->right != nil) { /* returns the maximum of the left subtree of x */
            y = y->right;
        }
        return(y);
    }
    else {
        y = x->parent;
        while (x == y->left) {
            if (y == root) return(nil);
            x = y;
            y = y->parent;
        }
        return(y);
    }
}


/***********************************************************************/
/*  FUNCTION:  rbTreeDestroyHelper */
/**/
/*    INPUTS:  tree is the tree to destroy and x is the current node */
/**/
/*    OUTPUT:  none  */
/**/
/*    EFFECTS:  This function recursively destroys the nodes of the tree */
/*              postorder using the destroyKey and destroyValue functions. */
/**/
/*    Modifies Input: tree, x */
/**/
/*    Note:    This function should only be called by rbTreeDestroy */
/***********************************************************************/

static void rbTreeDestroyHelper(RedBlackTree* tree, RedBlackNode* x) {
    RedBlackNode* nil = tree->nil;
    if (x != nil) {
        rbTreeDestroyHelper(tree, x->left);
        rbTreeDestroyHelper(tree, x->right);
        tree->destroyKey(x->key);
        tree->destroyValue(x->value);
        free(x);
    }
}


/***********************************************************************/
/*  FUNCTION:  rbTreeDestroy */
/**/
/*    INPUTS:  tree is the tree to destroy */
/**/
/*    OUTPUT:  none */
/**/
/*    EFFECT:  Destroys the key and frees memory */
/**/
/*    Modifies Input: tree */
/**/
/***********************************************************************/

static void rbTreeDestroy(RedBlackTree* tree) {
    rbTreeDestroyHelper(tree, tree->root->left);
    free(tree->root);
    free(tree->nil);
    free(tree);
}


/***********************************************************************/
/*  FUNCTION:  rbTreeExactQuery */
/**/
/*    INPUTS:  tree is the tree to print and q is a pointer to the key */
/*             we are searching for */
/**/
/*    OUTPUT:  returns the a node with key equal to q.  If there are */
/*             multiple nodes with key equal to q this function returns */
/*             the one highest in the tree */
/**/
/*    Modifies Input: none */
/**/
/***********************************************************************/
  
static RedBlackNode* rbTreeExactQuery(RedBlackTree* tree, void* q) {
    RedBlackNode* x = tree->root->left;
    RedBlackNode* nil = tree->nil;
    int compVal;
    if (x == nil) return(nil);
    compVal = tree->compare(x->key, (int*)q);
    while (0 != compVal) {/*assignemnt*/
        if (0 < compVal) { /* x->key > q */
            x = x->left;
        }
        else {
            x = x->right;
        }
        if (x == nil) return(nil);
        compVal = tree->compare(x->key, (int*)q);
    }
    return(x);
}


/***********************************************************************/
/*  FUNCTION:  rbTreeDeleteFixUp */
/**/
/*    INPUTS:  tree is the tree to fix and x is the child of the spliced */
/*             out node in rbTreeDelete. */
/**/
/*    OUTPUT:  none */
/**/
/*    EFFECT:  Performs rotations and changes colors to restore red-black */
/*             properties after a node is deleted */
/**/
/*    Modifies Input: tree, x */
/**/
/*    The algorithm from this function is from _Introduction_To_Algorithms_ */
/***********************************************************************/

static void rbTreeDeleteFixUp(RedBlackTree* tree, RedBlackNode* x) {
    RedBlackNode* root = tree->root->left;
    RedBlackNode* w;

    while ((!x->red) && (root != x)) {
        if (x == x->parent->left) {
            w = x->parent->right;
            if (w->red) {
                w->red = 0;
                x->parent->red = 1;
                rbTreeLeftRotate(tree, x->parent);
                w = x->parent->right;
            }
            if ((!w->right->red) && (!w->left->red)) {
                w->red = 1;
                x = x->parent;
            }
            else {
                if (!w->right->red) {
                    w->left->red = 0;
                    w->red = 1;
                    rbTreeRightRotate(tree, w);
                    w = x->parent->right;
                }
                w->red = x->parent->red;
                x->parent->red = 0;
                w->right->red = 0;
                rbTreeLeftRotate(tree, x->parent);
                x = root; /* this is to exit while loop */
            }
        }
        else { /* the code below is has left and right switched from above */
            w = x->parent->left;
            if (w->red) {
                w->red = 0;
                x->parent->red = 1;
                rbTreeRightRotate(tree, x->parent);
                w = x->parent->left;
            }
            if ((!w->right->red) && (!w->left->red)) {
                w->red = 1;
                x = x->parent;
            }
            else {
                if (!w->left->red) {
                    w->right->red = 0;
                    w->red = 1;
                    rbTreeLeftRotate(tree, w);
                    w = x->parent->left;
                }
                w->red = x->parent->red;
                x->parent->red = 0;
                w->left->red = 0;
                rbTreeRightRotate(tree, x->parent);
                x = root; /* this is to exit while loop */
            }
        }
    }
    x->red = 0;

#ifdef DEBUG_ASSERT
    Assert(!tree->nil->red, "nil not black in rbTreeDeleteFixUp");
#endif
}


/***********************************************************************/
/*  FUNCTION:  rbTreeDelete */
/**/
/*    INPUTS:  tree is the tree to delete node z from */
/**/
/*    OUTPUT:  none */
/**/
/*    EFFECT:  Deletes z from tree and frees the key and info of z */
/*             using DestoryKey and DestoryInfo.  Then calls */
/*             rbTreeDeleteFixUp to restore red-black properties */
/**/
/*    Modifies Input: tree, z */
/**/
/*    The algorithm from this function is from _Introduction_To_Algorithms_ */
/***********************************************************************/

static void rbTreeDelete(RedBlackTree* tree, RedBlackNode* z) {
    RedBlackNode* y;
    RedBlackNode* x;
    RedBlackNode* nil = tree->nil;
    RedBlackNode* root = tree->root;

    if ((z->prev != NULL) && (z->prev != nil)) {
        z->prev->next = z->next;
    }
    if ((z->next != NULL) && (z->next != nil)) {
        z->next->prev = z->prev;
    }

    y = ((z->left == nil) || (z->right == nil)) ? z : rbTreeSuccessor(tree, z);
    x = (y->left == nil) ? y->right : y->left;
    if (root == (x->parent = y->parent)) { /* assignment of y->p to x->p is intentional */
        root->left = x;
    }
    else {
        if (y == y->parent->left) {
            y->parent->left = x;
        }
        else {
            y->parent->right = x;
        }
    }
    if (y != z) { /* y should not be nil in this case */

#ifdef DEBUG_ASSERT
        Assert((y != tree->nil), "y is nil in rbTreeDelete\n");
#endif
        /* y is the node to splice out and x is its child */

        if (!(y->red)) rbTreeDeleteFixUp(tree, x);

        // Fix the linked-list portions.
        if (tree->head == z) {
            tree->head = z->next;
        }
        if (tree->tail == z) {
            tree->tail = z->prev;
        }
        if (z->prev != tree->nil) {
            z->prev->next = z->next;
        }
        if (z->next != tree->nil) {
            z->next->prev = z->prev;
        }

        tree->destroyKey(z->key);
        tree->destroyValue(z->value);
        y->left = z->left;
        y->right = z->right;
        y->parent = z->parent;
        y->red = z->red;
        z->left->parent = z->right->parent = y;
        if (z == z->parent->left) {
            z->parent->left = y;
        }
        else {
            z->parent->right = y;
        }
        free(z);
    }
    else {
        // We're removing y in this case.
        // Fix the linked-list portions.
        if (tree->head == y) {
            tree->head = y->next;
        }
        if (tree->tail == y) {
            tree->tail = y->prev;
        }
        if (y->prev != tree->nil) {
            y->prev->next = y->next;
        }
        if (y->next != tree->nil) {
            y->next->prev = y->prev;
        }

        tree->destroyKey(y->key);
        tree->destroyValue(y->value);
        if (!(y->red)) rbTreeDeleteFixUp(tree, x);
        free(y);
    }

#ifdef DEBUG_ASSERT
    Assert(!tree->nil->red, "nil not black in rbTreeDelete");
#endif
}


/******************** End Support Data Structure Code *************************/

static int compareU32(const void* u32PointerA, const void* u32PointerB) {
    uint32_t u32A = *((uint32_t*)u32PointerA);
    uint32_t u32B = *((uint32_t*)u32PointerB);

    if (u32A < u32B) {
        return -1;
    }
    else if (u32A > u32B) {
        return 1;
    }
    else { // u32A == u32B
        return 0;
    }
}

static void NullFunction(void* parameter) {
    (void)parameter;
    return;
}

static mtx_t callOnceMutex = { 0 };

// Call once support.
void call_once(once_flag* flag, void(*func)(void)) {
    mtx_lock(&callOnceMutex);
    if (*flag == 0) {
        (*flag)++;
        func();
    }
    mtx_unlock(&callOnceMutex);

    return;
}


// Mutex support.
int mtx_init(mtx_t* mtx, int type) {
    mtx->attribs = type;
    // Timed mutexes need the handle.  Everything else uses only the criticalSection.
    mtx->handle = CreateMutex(
        NULL,  // default security attributes
        FALSE, // initially not owned
        NULL); // unnamed mutex
    InitializeCriticalSection(&mtx->criticalSection);

    return thrd_success;
}

int mtx_lock(mtx_t* mtx) {
    int returnValue = thrd_success;

    // See if we need to atomically initialize the mutex
    if (mtx->handle == NULL) {
        // Initialize the HANDLE
        // Idea for this procedure came from Dr. Alex RE's answer to
        // https://stackoverflow.com/questions/3555859/is-it-possible-to-do-static-initialization-of-mutexes-in-windows
        HANDLE mtxInit = CreateMutex(NULL, FALSE, NULL);
        if (InterlockedCompareExchangePointer((PVOID*) &mtx->handle, (PVOID)mtxInit, NULL) != NULL) {
            // mtx->handle was already initialized.  Close the mutex we just created.
            CloseHandle(mtxInit);
        }
        else {
            // Initialize the CRITICAL_SECTION
            InitializeCriticalSection(&mtx->criticalSection);
        }
    }

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

    // See if we need to atomically initialize the mutex
    if (mtx->handle == NULL) {
        // Initialize the HANDLE
        // Idea for this procedure came from Dr. Alex RE's answer to
        // https://stackoverflow.com/questions/3555859/is-it-possible-to-do-static-initialization-of-mutexes-in-windows
        HANDLE mtxInit = CreateMutex(NULL, FALSE, NULL);
        if (InterlockedCompareExchangePointer((PVOID*)&mtx->handle, (PVOID)mtxInit, NULL) != NULL) {
            // mtx->handle was already initialized.  Close the mutex we just created.
            CloseHandle(mtxInit);
        }
        else {
            // Initialize the CRITICAL_SECTION
            InitializeCriticalSection(&mtx->criticalSection);
        }
    }

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
    
    // See if we need to atomically initialize the mutex
    if (mtx->handle == NULL) {
        // Initialize the HANDLE
        // Idea for this procedure came from Dr. Alex RE's answer to
        // https://stackoverflow.com/questions/3555859/is-it-possible-to-do-static-initialization-of-mutexes-in-windows
        HANDLE mtxInit = CreateMutex(NULL, FALSE, NULL);
        if (InterlockedCompareExchangePointer((PVOID*)&mtx->handle, (PVOID)mtxInit, NULL) != NULL) {
            // mtx->handle was already initialized.  Close the mutex we just created.
            CloseHandle(mtxInit);
        }
        else {
            // Initialize the CRITICAL_SECTION
            InitializeCriticalSection(&mtx->criticalSection);
        }
    }

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

    LeaveCriticalSection(&mtx->criticalSection);

    if (((mtx->attribs & mtx_timed) != 0) && (ReleaseMutex(mtx->handle) == 0)) {
        returnValue = thrd_error;
    }
    
    return returnValue;
}

void mtx_destroy(mtx_t* mtx) {
    CloseHandle(mtx->handle);
    DeleteCriticalSection(&mtx->criticalSection);
}


// Condition support.
int cnd_broadcast(cnd_t* cond) {
    WakeAllConditionVariable(cond);
    return thrd_success;
}

void cnd_destroy(cnd_t* cond) {
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
static RedBlackTree* tssStorageByKey = NULL;
static RedBlackTree* tssStorageByThread = NULL;
typedef struct DataAndDestructor {
    RedBlackTree* data;
    tss_dtor_t destructor;
} DataAndDestructor;
uint32_t tssIndex = 1;
mtx_t* tssStorageByKeyMutex = NULL;
mtx_t* tssStorageByThreadMutex = NULL;

int tss_create(tss_t* key, tss_dtor_t dtor) {
    if (tssStorageByKey == NULL) {
        tssStorageByKey = rbTreeCreate(compareU32, free, NullFunction);
        if (tssStorageByKey == NULL) {
            // No tree.  Can't proceed.
            LOG_MALLOC_FAILURE();
            exit(-1);
            return thrd_error;
        }
        tssStorageByKeyMutex = calloc(1, sizeof(mtx_t));
        if (tssStorageByKeyMutex == NULL) {
            LOG_MALLOC_FAILURE();
            exit(-1);
            return thrd_error;
        }
        if (mtx_init(tssStorageByKeyMutex, mtx_plain | mtx_recursive) != thrd_success) {
            printLog(CRITICAL, "Could not initialize tssStorageByKeyMutex\n");
            exit(-1);
            return thrd_error;
        }
    }
    if (tssStorageByThread == NULL) {
        tssStorageByThread = rbTreeCreate(compareU32, NullFunction, NullFunction);
        if (tssStorageByThread == NULL) {
            // No tree.  Can't proceed.
            LOG_MALLOC_FAILURE();
            exit(-1);
            return thrd_error;
        }
        tssStorageByThreadMutex = calloc(1, sizeof(mtx_t));
        if (tssStorageByThreadMutex == NULL) {
            LOG_MALLOC_FAILURE();
            exit(-1);
            return thrd_error;
        }
        if (mtx_init(tssStorageByThreadMutex, mtx_plain | mtx_recursive) != thrd_success) {
            printLog(CRITICAL, "Could not initialize tssStorageByThreadMutex\n");
            exit(-1);
            return thrd_error;
        }
    }

    if (dtor == NULL) {
        dtor = NullFunction;
    }

    DataAndDestructor* dd = (DataAndDestructor*) malloc(sizeof(DataAndDestructor));
    if (dd == NULL) {
        LOG_MALLOC_FAILURE();
        exit(-1);
        return thrd_error;
    }
    dd->data = rbTreeCreate(compareU32, free, NullFunction);
    dd->destructor = dtor;

    uint32_t* thisIndex = (uint32_t*) malloc(sizeof(uint32_t));
    if (thisIndex == NULL) {
        LOG_MALLOC_FAILURE();
        exit(-1);
        return thrd_error;
    }
    *thisIndex = tssIndex;
    mtx_lock(tssStorageByKeyMutex);
    rbTreeInsert(tssStorageByKey, thisIndex, dd);
    mtx_unlock(tssStorageByKeyMutex);
    *key = tssIndex;
    tssIndex++;

    return thrd_success;
}

void tss_delete(tss_t key) {
    if (tssStorageByKey == NULL) {
        // Thread-specific storage has not been initialized.  Nothing to do.
        printLog(DEBUG, "Key storage not initialized.\n");
        return;
    }

    mtx_lock(tssStorageByKeyMutex);

    RedBlackNode* keyData = rbTreeExactQuery(tssStorageByKey, &key);
    if (keyData == tssStorageByKey->nil) {
        // Storage for this key has not been set up.  Nothing to do.
        printLog(DEBUG, "Non-existent key.\n");
        mtx_unlock(tssStorageByKeyMutex);
        return;
    } else if (keyData->value == NULL) {
        printLog(DEBUG, "No value for key.\n");
        mtx_unlock(tssStorageByKeyMutex);
        return;
    }

    // Remove metadata for all threads for this key.
    RedBlackTree *data = ((DataAndDestructor*) keyData->value)->data;
    if (data == NULL) {
        printLog(DEBUG, "No metadata for value.\n");
        mtx_unlock(tssStorageByKeyMutex);
        return;
    }
    for (RedBlackNode* next = data->head; next != data->nil;) {
        RedBlackNode* node = next;
        next = next->next;
        rbTreeDelete(data, node);
    }
    rbTreeDelete(tssStorageByKey, keyData);

    mtx_unlock(tssStorageByKeyMutex);

    return;
}

void* tss_get(tss_t key) {
    printLog(FLOOD, "ENTER tss_get(key=%u)\n", key);

    if (tssStorageByKey == NULL) {
        // Thread-specific storage has not been initialized.  Nothing to do.
        printLog(DEBUG, "Key storage not initialized.\n");
        printLog(FLOOD, "EXIT tss_get(key=%u) = {NULL}\n", key);
        return NULL;
    }

    mtx_lock(tssStorageByKeyMutex);

    RedBlackNode* keyData = rbTreeExactQuery(tssStorageByKey, &key);
    if (keyData == tssStorageByKey->nil) {
        // Storage for this key has not been set up.  Nothing to do.
        printLog(DEBUG, "Non-existent key.\n");
        mtx_unlock(tssStorageByKeyMutex);
        printLog(FLOOD, "EXIT tss_get(key=%u) = {NULL}\n", key);
        return NULL;
    } else if (keyData->value == NULL) {
        printLog(DEBUG, "No value for key.\n");
        mtx_unlock(tssStorageByKeyMutex);
        printLog(FLOOD, "EXIT tss_get(key=%u) = {NULL}\n", key);
        return NULL;
    }

    RedBlackTree *data = ((DataAndDestructor*) keyData->value)->data;
    if (data == NULL) {
        printLog(DEBUG, "No data for value.\n");
        mtx_unlock(tssStorageByKeyMutex);
        printLog(FLOOD, "EXIT tss_get(key=%u) = {NULL}\n", key);
        return NULL;
    }

    thrd_t thisThread = thrd_current();
    RedBlackNode* threadData = rbTreeExactQuery(data, &thisThread);
    if (threadData == data->nil) {
        printLog(TRACE, "No threadData for thisThread.\n");
        mtx_unlock(tssStorageByKeyMutex);
        printLog(FLOOD, "EXIT tss_get(key=%u) = {NULL}\n", key);
        return NULL;
    }

    mtx_unlock(tssStorageByKeyMutex);
    printLog(FLOOD, "EXIT tss_get(key=%u) = {%p}\n", key, threadData->value);
    return threadData->value;
}

int tss_set(tss_t key, void* val) {
    printLog(FLOOD, "ENTER tss_set(key=%u, val=%p)\n", key, val);

    if (tssStorageByKey == NULL) {
        // Thread-specific storage has not been initialized.  Nothing to do.
        printLog(DEBUG, "Key storage not initialized.\n");
        printLog(FLOOD, "EXIT tss_set(key=%u, val=%p) = {%d}\n",
            key, val, thrd_error);
        return thrd_error;
    }

    // Set the lookup by key.  This is what tss_get will use.
    mtx_lock(tssStorageByKeyMutex);

    RedBlackNode* keyData = rbTreeExactQuery(tssStorageByKey, &key);
    if (keyData == tssStorageByKey->nil) {
        // Storage for this key has not been set up.  Cannot proceed because we don't
        // know what destructor to call.
        printLog(DEBUG, "Non-existent key.\n");
        mtx_unlock(tssStorageByKeyMutex);
        printLog(FLOOD, "EXIT tss_set(key=%u, val=%p) = {%d}\n",
            key, val, thrd_error);
        return thrd_error;
    } else if (keyData->value == NULL) {
        printLog(DEBUG, "No value for key.\n");
        mtx_unlock(tssStorageByKeyMutex);
        printLog(FLOOD, "EXIT tss_set(key=%u, val=%p) = {%d}\n",
            key, val, thrd_error);
        return thrd_error;
    }

    DataAndDestructor *dd = (DataAndDestructor*) keyData->value;
    RedBlackTree *data = dd->data;
    if (data == NULL) {
        printLog(DEBUG, "No metadata for value.\n");
        mtx_unlock(tssStorageByKeyMutex);
        printLog(FLOOD, "EXIT tss_set(key=%u, val=%p) = {%d}\n",
            key, val, thrd_error);
        return thrd_error;
    }

    thrd_t *thisThread = (thrd_t*) malloc(sizeof(thrd_t));
    if (thisThread == NULL) {
        LOG_MALLOC_FAILURE();
        mtx_unlock(tssStorageByKeyMutex);
        exit(-1);
        printLog(FLOOD, "EXIT tss_set(key=%u, val=%p) = {%d}\n",
            key, val, thrd_error);
        return thrd_error;
    }

    *thisThread = thrd_current();
    RedBlackNode* prevData = rbTreeExactQuery(data, thisThread);
    if (prevData != data->nil) {
        // We have old metadata to remove.
        rbTreeDelete(data, prevData);
    }
    rbTreeInsert(data, thisThread, val);

    mtx_unlock(tssStorageByKeyMutex);

    // Set the lookup by thread.  This is what thrd_exit will use.
    mtx_lock(tssStorageByThreadMutex);

    RedBlackNode* threadData = rbTreeExactQuery(tssStorageByThread, thisThread);
    RedBlackTree* keys = NULL;
    if (threadData != tssStorageByThread->nil) {
        keys = (RedBlackTree*) threadData->value;
    } else {
        // No reverse lookup has been set yet.  This is the first time this key
        // has been set.  Allocate the tree.
        keys = rbTreeCreate(compareU32, free, NullFunction);
        rbTreeInsert(tssStorageByThread, thisThread, keys);
    }

    keyData = rbTreeExactQuery(keys, &key);
    if (keyData != keys->nil) {
        // Remove the old metadata.
        rbTreeDelete(keys, keyData);
    }
    tss_t *keyCopy = (tss_t*) malloc(sizeof(tss_t));
    *keyCopy = key;
    rbTreeInsert(keys, keyCopy, val);

    mtx_unlock(tssStorageByThreadMutex);

    printLog(FLOOD, "EXIT tss_set(key=%u, val=%p) = {%d}\n",
        key, val, thrd_success);
    return thrd_success;
}


// Thread support.
static RedBlackTree* attachedThreads = NULL;
mtx_t *attachedThreadsMutex = NULL;

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

    int returnValue = thrd_success;
    WindowsCreateWrapperArgs *wrapper_args
        = calloc(1, sizeof(WindowsCreateWrapperArgs));
    if (wrapper_args == NULL) {
        // Can't allocate enough memory to start the thread.
        LOG_MALLOC_FAILURE();
        exit(-1);
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
            attachedThreads = rbTreeCreate(compareU32, free, NullFunction);
            if (attachedThreads == NULL) {
                LOG_MALLOC_FAILURE();
                exit(-1);
                return thrd_error;
            }
            attachedThreadsMutex = calloc(1, sizeof(mtx_t));
            if (attachedThreadsMutex == NULL) {
                LOG_MALLOC_FAILURE();
                exit(-1);
                return thrd_error;
            }
            if (mtx_init(attachedThreadsMutex, mtx_plain) != thrd_success) {
                printLog(CRITICAL, "Could not initialize attachedThreadsMutex.\n");
                printLog(TRACE, "EXIT thrd_create(thr=%p, func=%p, arg=%p) = {%d}\n",
                    thr, func, arg, thrd_error);
                exit(-1);
                return thrd_error;
            }
        }
        if (thr != NULL) {
            uint32_t* threadId = (uint32_t*) malloc(sizeof(uint32_t));
            if (threadId == NULL) {
                LOG_MALLOC_FAILURE();
                exit(-1);
                return thrd_error;
            }
            *threadId = *thr;
            mtx_lock(attachedThreadsMutex);
            rbTreeInsert(attachedThreads, threadId, threadHandle);
            mtx_unlock(attachedThreadsMutex);
        }
        else {
            // Can't store the thread ID anywhere.  Detach.
            CloseHandle(threadHandle);
        }
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
    mtx_lock(attachedThreadsMutex);

    RedBlackNode* node = rbTreeExactQuery(attachedThreads, &thr);
    if (node != attachedThreads->nil) {
        threadHandle = (HANDLE) node->value;
    }
    if (threadHandle != NULL) {
        CloseHandle(threadHandle);
        rbTreeDelete(attachedThreads, node);
    }
    else {
        returnValue = thrd_error;
    }

    mtx_unlock(attachedThreadsMutex);
    return returnValue;
}

int thrd_equal(thrd_t thr0, thrd_t thr1) {
    return (thr0 == thr1);
}

void thrd_exit(int res) {
    printLog(TRACE, "ENTER thrd_exit(res=%d)\n", res);

    thrd_t thisThread = thrd_current();

    // Destroy all the thread local storage.
    if (tssStorageByThread != NULL) {
        mtx_lock(tssStorageByThreadMutex);
        RedBlackNode *threadData = rbTreeExactQuery(tssStorageByThread, &thisThread);
        if ((threadData != tssStorageByThread->nil) && (threadData->value != NULL)) {
            RedBlackTree *data = (RedBlackTree*) threadData->value;
            RedBlackNode *nil = data->nil;
            // Walk through all the keys this thread has used and delete the
            // thread's local copy.
            for (RedBlackNode *node = data->head; node != nil;) {
                thrd_t* key = (thrd_t*)node->key;
                tss_dtor_t destructor = NullFunction;
                mtx_lock(tssStorageByKeyMutex);
                RedBlackNode* keyNode = rbTreeExactQuery(tssStorageByKey, key);
                if ((keyNode != tssStorageByKey->nil) && (keyNode->value != NULL)) {
                    DataAndDestructor* dd = (DataAndDestructor*)keyNode->value;
                    if (dd->destructor != NULL) {
                        destructor = dd->destructor;
                    }
                }
                mtx_unlock(tssStorageByKeyMutex);
                RedBlackNode* next = node->next;
                // This will call the destructor for the node's value but not its key.
                destructor(node->value);
                rbTreeDelete(data, node);
                node = next;
            }
            rbTreeDelete(tssStorageByThread, threadData);
        }
        mtx_unlock(tssStorageByThreadMutex);
    }

    // Clean up all of the lookups.
    if (tssStorageByKey != NULL) {
        mtx_lock(tssStorageByKeyMutex);
        RedBlackNode *nil = tssStorageByKey->nil;
        // Walk through all the keys and delete the metadata for this thread.
        for (RedBlackNode* storage = tssStorageByKey->head; storage != nil; storage = storage->next) {
            DataAndDestructor *dd = (DataAndDestructor*) storage->value;
            if ((dd != NULL) && (dd->data != NULL)) {
                RedBlackTree *data = dd->data;
                RedBlackNode* threadData = rbTreeExactQuery(data, &thisThread);
                if (threadData != data->nil) {
                    // This will call the destructor for the node's key but not its value.
                    rbTreeDelete(data, threadData);
                }
            }
        }
        mtx_unlock(tssStorageByKeyMutex);
    }

    printLog(TRACE, "EXIT thrd_exit(res=%d) = {}\n", res);
    ExitThread(res);
}

int thrd_join(thrd_t thr, int* res) {
    int returnValue = thrd_success;
    HANDLE threadHandle = NULL;
    mtx_lock(attachedThreadsMutex);

    RedBlackNode* node = rbTreeExactQuery(attachedThreads, &thr);
    if (node != attachedThreads->nil) {
        threadHandle = (HANDLE)node->value;
    }
    mtx_unlock(attachedThreadsMutex);

    if (threadHandle != NULL) {
        DWORD waitResult = WaitForSingleObject(
            threadHandle,  // handle to thread
            INFINITE);     // no time-out interval
        if (waitResult != WAIT_OBJECT_0) {
            returnValue = thrd_error;
        }
        else {
            if ((res != NULL) && (GetExitCodeThread(threadHandle, res) == 0)) {
                returnValue = thrd_error;
            }
        }
        mtx_lock(attachedThreadsMutex);
        rbTreeDelete(attachedThreads, node);
        mtx_unlock(attachedThreadsMutex);
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
    HANDLE threadHandle = NULL;
    mtx_lock(attachedThreadsMutex);

    RedBlackNode* node = rbTreeExactQuery(attachedThreads, &thr);
    if (node != attachedThreads->nil) {
        threadHandle = (HANDLE)node->value;
    }
    mtx_unlock(attachedThreadsMutex);

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

/*
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
*/

#endif // _MSC_VER
