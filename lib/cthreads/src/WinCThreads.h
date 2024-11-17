///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              10.25.2019
///
/// @file              WinCThreads.h
///
/// @brief             These are the support functions that allow Visual Studio
///                    to compile using C threads functionality.
///
/// @details
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

#ifndef WIN_C_THREADS_H
#define WIN_C_THREADS_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#include <time.h>
#define localtime_r(timep, result) localtime_s(result, timep)
#define gmtime_r(timep, result) gmtime_s(result, timep)
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C"
{
#endif

// Call once support.
typedef LONG once_flag;
#define ONCE_FLAG_INIT     0
#define ONCE_FLAG_RUNNING  1
#define ONCE_FLAG_COMPLETE 2

void call_once(once_flag* flag, void(*func)(void));


// Mutex support.
typedef struct mtx_t {
    int attribs;
    HANDLE handle;
    CRITICAL_SECTION criticalSection;
    bool initialized;
} mtx_t;

#define mtx_plain     0
#define mtx_recursive 1
#define mtx_timed     2

int mtx_init(mtx_t* mtx, int type);
int mtx_lock(mtx_t* mtx);
int mtx_unlock(mtx_t* mtx);
void mtx_destroy(mtx_t* mtx);
int mtx_timedlock(mtx_t* mtx, const struct timespec* ts);
int mtx_trylock(mtx_t* mtx);


// Condition support.
typedef CONDITION_VARIABLE cnd_t;

int cnd_broadcast(cnd_t* cond);
void cnd_destroy(cnd_t* cond);
int cnd_init(cnd_t* cond);
int cnd_signal(cnd_t* cond);
int cnd_timedwait(cnd_t* cond, mtx_t* mtx, const struct timespec* ts);
int cnd_wait(cnd_t* cond, mtx_t* mtx);


// Thread support.
typedef DWORD thrd_t;
typedef int (*thrd_start_t)(void*);

#define thrd_success    0
#define thrd_busy       1
#define thrd_error      2
#define thrd_nomem      3
#define thrd_timedout   4
#define thrd_terminated 5

int thrd_create(thrd_t* thr, thrd_start_t func, void* arg);
thrd_t thrd_current(void);
int thrd_detach(thrd_t thr);
int thrd_equal(thrd_t thr0, thrd_t thr1);
void thrd_exit(int res);
int thrd_join(thrd_t thr, int* res);
int thrd_sleep(const struct timespec* duration, struct timespec* remaining);
void thrd_yield(void);
int thrd_terminate(thrd_t thr);


// Thread-specific storage support.
#define TSS_DTOR_ITERATIONS 4

typedef void (*tss_dtor_t)(void*);
// A tss_t value is a complete object type that holds an identifier for a
// thread-specific storage pointer.
typedef uint16_t tss_t; 

int tss_create(tss_t* key, tss_dtor_t dtor);
void tss_delete(tss_t key);
void* tss_get(tss_t key);
int tss_set(tss_t key, void* val);

#ifndef TIME_UTC
#define TIME_UTC 0
#endif
int timespec_get(struct timespec* spec, int base);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // WIN_C_THREADS_H

