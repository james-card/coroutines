///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              01.12.2022
///
/// @file              Coroutines.h
///
/// @brief             Minimal coroutines for C.
///
/// @details           Originally written by Tony Finch <dot@dotat.at> and
///                    released to the public domain (see
///                    http://creativecommons.org/publicdomain/zero/1.0/).
///                    The API was modeled after Lua's coroutines (see
///                    http://www.lua.org/manual/5.1/manual.html#2.11).
///                    Source code cloned from
///                    git://git.chiark.greenend.org.uk/~fanf/picoro.git
///
/// @copyright
///                   Copyright (c) 2012-2022 James Card
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

#ifndef COROUTINES_H
#define COROUTINES_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <setjmp.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <stdint.h>

// Base coroutine support.

// Coroutine status values.
#define coroutineSuccess  0
#define coroutineBusy     1
#define coroutineError    2
#define coroutineNomem    3
#define coroutineTimedout 4

/// @def COROUTINE_NOT_RESUMABLE
///
/// @brief Special value to indicate to a caller of coroutineResume() that the
/// provided coroutine cannot be resumed, either because it is blocked or
/// because it has completed.
#define COROUTINE_NOT_RESUMABLE ((void*) ((intptr_t) -1))

/// @def COROUTINE_BLOCKED
///
/// @brief Special value to indicate to a caller of coroutineResume() that the
/// provided coroutine is blocked within a blocking coroutine operation.
#define COROUTINE_BLOCKED ((void*) ((intptr_t) -2))

/// @def COROUTINE_STACK_SIZE
///
/// @brief The size of the stack for a coroutine.  MSVC requires that the stack
/// be 16-byte aligned, so make sure we adhere to that.
#ifndef COROUTINE_STACK_SIZE
#define COROUTINE_STACK_SIZE (16 * 1024)
#endif
#if (COROUTINE_STACK_SIZE & 15) != 0
#error COROUTINE_STACK_SIZE must be divisible by 16.
#endif

/// @def COROUTINE_ID_NOT_SET
///
/// @brief Special value to indicate that a coroutine's ID value is not set.
/// This is the initial value just after the coroutine constructor completes.
#define COROUTINE_ID_NOT_SET ((int64_t) 0x8000000000000000)

/// @enum CoroutineState
///
/// @brief States that a Coroutine can be in.
typedef enum CoroutineState {
  NOT_RUNNING,
  RUNNING,
  BLOCKED,
  NUM_COROUTINE_STATES
} CoroutineState;

/// @struct Coroutine
///
/// @brief Data structure to manage an individual coroutine.
///
/// @param next Pointer to the next Coroutine on the list.
/// @param context The jmp_buf to hold the context of the coroutine.
/// @param id The ID of the coroutine.
/// @param state The state of the coroutine.  (See enum above.)
typedef struct Coroutine {
  struct Coroutine *next;
  jmp_buf context;
  int64_t id;
  CoroutineState state;
} Coroutine;

// Coroutine function prototypes.  Doxygen inline in source file.
Coroutine* coroutineCreate(void* func(void *arg));
bool coroutineResumable(Coroutine *coroutine);
void* coroutineResume(Coroutine *targetCoroutine, void *arg);
void* coroutineYield(void *arg);
int coroutineSetId(Coroutine* coroutine, int64_t id);
int64_t coroutineId(Coroutine* coroutine);
CoroutineState coroutineState(Coroutine* coroutine);


// Coroutine mutex support.

// Comutex types
#define comutexPlain     0
#define comutexRecursive 1
#define comutexTimed     2

/// @struct Comutex
///
/// @brief Definition for a coroutine mutex to provide mutual exclusion between
/// coroutines.
///
/// @param lastYieldValue The last value returned by a yield call if a comutex
///   lock function blocked while acquiring the lock, or NULL if the lock was
///   acquired on the first attempt.
/// @param type The type of the mutex (see above type definitions)
/// @param coroutine The coroutine that has the lock.
/// @param recursionLevel The number of times this mutex has been successfully
///   locked in this coroutine.
typedef struct Comutex {
  void *lastYieldValue;
  int type;
  Coroutine *coroutine;
  int recursionLevel;
} Comutex;

// Coroutine mutex function prototypes.  Doxygen inline in source file.
int comutexInit(Comutex *mtx, int type);
int comutexLock(Comutex *mtx);
int comutexUnlock(Comutex *mtx);
void comutexDestroy(Comutex *mtx);
int comutexTimedlock(Comutex *mtx, const struct timespec *ts);
int comutexTrylock(Comutex *mtx);
void* comutexLastYieldValue(Comutex *mtx);


// Coroutine condition support.

/// @struct Cocondition
///
/// @brief Definition for a coroutine condition for signaling between
/// coroutines.
///
/// @param lastYieldValue The last value returned by a yield call while a
///   cocondition wait function was blocked.
/// @param numWaiters The number of coroutines blocked waiting on this
///   condition.
/// @param numSignal The number of signals emitted for unblocking waiting
///   coroutines.
typedef struct Cocondition {
  void *lastYieldValue;
  int numWaiters;
  int numSignals;
} Cocondition;

// Coroutine condition function prototypes.  Doxygen inline in source file.
int coconditionBroadcast(Cocondition *cond);
void coconditionDestroy(Cocondition *cond);
int coconditionInit(Cocondition *cond);
int coconditionSignal(Cocondition *cond);
int conditionTimedwait(Cocondition *cond, Comutex *mtx,
  const struct timespec *ts);
int coconditionWait(Cocondition *cond, Comutex *mtx);
void* coconditionLastYieldValue(Cocondition *cond);


#ifdef __cplusplus
} // extern "C"
#endif

#endif // COROUTINES_H
