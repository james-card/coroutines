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
#include "RadixTree.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

// Functions, variables and types defined in CThreadsMessages.c
extern once_flag thrd_msg_q_storage_initialized;
typedef struct thrd_msg_q_t thrd_msg_q_t;
void thrd_msg_q_storage_init(void);
int thrd_msg_q_create(void);
int thrd_msg_q_destroy(thrd_msg_q_t *queue);

#ifdef __cplusplus
}
#endif

#define TSS_T_BIT_WIDTH (sizeof(tss_t) * 8)
#define ARRAY_OF_RADIX_TREES_SIZE (1 << TSS_T_BIT_WIDTH)

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

static RadixTree **tssStorageByKey = NULL;
static RadixTree * tssStorageByThread = NULL;
static tss_t tssIndex = 1;
static once_flag tssMetadataOnceFlag = ONCE_FLAG_INIT;
static bool tssMetadataInitialized = false;

void tssIdDestroy(TssId *tssId) {
    if (tssId != NULL) {
        radixTreeDeleteValue(
            tssStorageByKey[tssId->key], &tssId->thread, sizeof(tssId->thread));
        free(tssId); tssId = NULL;
    }
}

void initializeTssMetadata(void) {
    tssStorageByKey = (RadixTree**) calloc(
        1, ARRAY_OF_RADIX_TREES_SIZE * sizeof(RadixTree*));
    if (tssStorageByKey == NULL) {
        // No tree.  Can't proceed.
        LOG_MALLOC_FAILURE();
        exit(1);
    }

    tssStorageByThread = radixTreeCreate((tss_dtor_t) radixTreeDestroy);
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

    tssStorageByKey[tssIndex] = radixTreeCreate(dtor);

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
        = radixTreeDestroy(tssStorageByKey[key]);

    return;
}

void* tss_get(tss_t key) {
    if (tssMetadataInitialized == false) {
        // Thread-specific storage has not been initialized.  Nothing to do.
        printLog(DEBUG, "Key storage not initialized.\n");
        return NULL;
    }

    thrd_t thisThread = thrd_current();
    void *returnValue = radixTreeGetValue(
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
        if (radixTreeSetValue(
            tssStorageByKey[key], &thisThread, sizeof(thisThread), val) < 0
        ) {
            // returnValue is thrd_error
            break;
        }

        TssId *tssId = (TssId*) radixTreeGetValue2(tssStorageByThread,
            &thisThread, sizeof(thisThread), &key, sizeof(key));
        if (tssId == NULL) {
            TssId *newTssId = (TssId*) malloc(sizeof(TssId));
            if (radixTreeSetValue2(tssStorageByThread,
                &thisThread, sizeof(thisThread), &key, sizeof(key),
                newTssId, (tss_dtor_t) tssIdDestroy) < 0
            ) {
                // This should be impossible.  Bail.
                // returnValue is thrd_error
                break;
            }
            tssId = (TssId*) radixTreeGetValue2(tssStorageByThread,
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
static RadixTree* attachedThreads = NULL;

typedef struct WindowsCreateWrapperArgs {
  thrd_start_t func;
  void *arg;
} WindowsCreateWrapperArgs;

DWORD __stdcall windows_create_wrapper(LPVOID wrapper_args) {
    printLog(TRACE, "ENTER windows_create_wrapper(wrapper_args=%p)\n",
        wrapper_args);

    // Create the message queue for this thread.
    thrd_msg_q_create();

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

    call_once(&thrd_msg_q_storage_initialized, thrd_msg_q_storage_init);

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
                = radixTreeCreate(winCThreadsNullFunction);
            if (attachedThreads == NULL) {
                LOG_MALLOC_FAILURE();
                exit(1);
                return thrd_error;
            }
        }

        radixTreeSetValue(
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

    threadHandle = (HANDLE) radixTreeGetValue(
        attachedThreads, &thr, sizeof(thr));
    if (threadHandle != NULL) {
        CloseHandle(threadHandle);
        radixTreeDeleteValue(attachedThreads, &thr, sizeof(thr));
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
    thrd_t thisThread = thrd_current();

    // Destroy all the thread local storage.
    if (tssMetadataInitialized == true) {
        radixTreeDeleteValue(
            tssStorageByThread, &thisThread, sizeof(thisThread));
    }

    // Destroy the message queue for this thread.
    if (thrd_msg_q_destroy(NULL) != thrd_success) {
        printLog(WARN, "Could not destroy message queue for thread %d.\n",
             thisThread);
    }

    printLog(TRACE, "EXIT thrd_exit(res=%d) = {}\n", res);
    ExitThread(res);
}

int thrd_join(thrd_t thr, int* res) {
    int returnValue = thrd_success;
    HANDLE threadHandle = (HANDLE) radixTreeGetValue(
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

        radixTreeDeleteValue(attachedThreads, &thr, sizeof(thr));
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
    HANDLE threadHandle = (HANDLE) radixTreeGetValue(
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

/**/
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
/**/

#endif // _WIN32

