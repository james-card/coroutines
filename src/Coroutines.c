////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Copyright (c) 2012-2022 James Card                     //
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

#include "Coroutines.h"

#if !defined(__STDC_NO_THREADS__) && !defined(SINGLE_CORE_COROUTINES) \
  && !defined(THREADSAFE_COROUTINES)
#define THREADSAFE_COROUTINES
#endif

#ifdef THREADSAFE_COROUTINES
#include "CThreads.h"
#include <stdio.h> // For error messages
#endif

// Prototype forward declarations for mutual recursion.
void coroutineStart(void);
void coroutineMain(void*);

/// @def ZEROINIT
///
/// @brief Define the proper way to declare a zeroized variable based on the
/// compiler environment.
#ifdef __cplusplus
#ifdef _MSC_VER
#define ZEROINIT(x) x = {0}
#else // Non-Visual Studio C++
#define ZEROINIT(x) x = {}
#endif // _MSC_VER
#else // __cplusplus not defined
#define ZEROINIT(x) x = {0}
#endif // __cplusplus

/// @union FuncData
///
/// @brief Translation between a function pointer and a data pointer.
///
/// @details Due to the way this library works, we sometimes need to pass and
/// return function pointers to our yield and resume functions, which take and
/// return data pointers.  ISO C doesn't permit casting between these two, so
/// we use a union to do the conversion.
///
/// @param func The function pointer portion of the pointer value.
/// @param data The data pointer portion of the pointer value.
typedef union FuncData {
    void* (*func)(void*);
    void* data;
} FuncData;

#ifndef THREADSAFE_COROUTINES
// Use statically-allocated global variables wihtout threading.

/// @var static Coroutine _first
///
/// @brief Library-private first (main) routine.
ZEROINIT(static Coroutine _first);

/// @var static Coroutine *running
///
/// @brief Library-private head of the running list.
///
/// @details The coroutine at the head of the running LIFO list has the CPU, and
/// all others are suspended inside coroutineResume(). The "first" coro object
/// holds the context for the process's initial stack and also ensures that all
/// externally-visible list elements have non-NULL next pointers.  (The "first"
/// coroutine isn't exposed to the caller.)
static Coroutine *running = &_first;

/// @var static Coroutine *idle
///
/// @brief Library-private head of the idle list.
///
/// @details The idle LIFO list contains coroutines that are suspended in
/// coroutineMain() and available to be associated with a new function. After
/// initialization it is never NULL except briefly while coroutineMain() forks
/// a new idle coroutine.
static Coroutine *idle = NULL;

/// @fn void coroutinePush(Coroutine **list, Coroutine *coroutine)
///
/// @brief Add a coroutine to a list and get the previous head of the list.
///
/// @param list The Coroutine list to push onto.
/// @param coroutine A pointer to the Coroutine to push onto the list.
///
/// @param Returns the previous head of the list.
void coroutinePush(Coroutine** list, Coroutine* coroutine) {
    if ((list != NULL) && (coroutine != NULL)) {
        coroutine->next = *list;
        *list = coroutine;
    }
}

/// @fn Coroutine* coroutinePop(Coroutine **list)
///
/// @brief Remove a coroutine from a list and return it.
///
/// @param list The Coroutine list to pop from.
///
/// @return Returns a pointer to the previous head of the list.
Coroutine* coroutinePop(Coroutine** list) {
    Coroutine* coroutine = NULL;

    if (list != NULL) {
        coroutine = *list;
        *list = coroutine->next;
        coroutine->next = NULL;
    }

    return coroutine;
}

/// @var static void* saved
///
/// @brief Value to be passed between contexts by the coroutinePass() function.
static void* saved = NULL;

#else // THREADSAFE_COROUTINES
// Use dynamic memory and C threads mechanisms.

/// @var static tss_t _first
///
/// @brief Thread-specific first (main) routine.
ZEROINIT(static tss_t _first);

/// @var static tss_t _running
///
/// @brief Thread-specific head of the running list.
///
/// @details The coroutine at the head of the running LIFO list has the CPU, and
/// all others are suspended inside coroutineResume(). The "first" coro object
/// holds the context for the process's initial stack and also ensures that all
/// externally-visible list elements have non-NULL next pointers.  (The "first"
/// coroutine isn't exposed to the caller.)
ZEROINIT(static tss_t _running);

/// @var static tss_t idle
///
/// @brief Thread-specific head of the idle list.
///
/// @details The idle LIFO list contains coroutines that are suspended in
/// coroutineMain() and available to be associated with a new function. After
/// initialization it is never NULL except briefly while coroutineMain() forks
/// a new idle coroutine.
ZEROINIT(static tss_t _idle);

/// @var static tss_t saved
///
/// @brief Value to be passed between contexts by the coroutinePass() function.
ZEROINIT(static tss_t saved);

/// @var static once_flag _threadMetadataSetup
///
/// @brief once_flag to make sure we only initialize the thread-specific storage
/// once.
static once_flag _threadMetadataSetup = ONCE_FLAG_INIT;

/// @fn void coroutineSetupThreadMetadata(void)
///
/// @brief Setup and initialize the thread-specefic storage needed for this
/// library.
///
/// @return This function returns no value.
void coroutineSetupThreadMetadata(void) {
  // _first is the only Coroutine node that will be allocated with dynamic
  // memory, so it's the only one that needs a destructor.  All the other
  // nodes on the _running and _idle lists will be on the stack and do not
  // need destructors.
  int status = tss_create(&_first, free);
  if (status != thrd_success) {
    fprintf(stderr, "Could not initialize _first.\n");
  }
  status = tss_create(&_running, NULL);
  if (status != thrd_success) {
    fprintf(stderr, "Could not initialize _running.\n");
  }
  status = tss_create(&_idle, NULL);
  if (status != thrd_success) {
    fprintf(stderr, "Could not initialize _idle.\n");
  }
  status = tss_create(&saved, NULL);
  if (status != thrd_success) {
    fprintf(stderr, "Could not initialize saved.\n");
  }

  Coroutine* first = (Coroutine*) calloc(1, sizeof(Coroutine));
  if (first == NULL) {
    fputs("Could not allocate first pointer in coroutineSetupThreadMetadata.\n",
      stderr);
  }
  status = tss_set(_first, first);
  if (status != thrd_success) {
    fprintf(stderr, "Could not set _first to first.\n");
  }
  status = tss_set(_running, first);
  if (status != thrd_success) {
    fprintf(stderr, "Could not set _running to first.\n");
  }
  // This *shouldn't* be necessary, but why assume when we can be certain?
  status = tss_set(_idle, NULL);
  if (status != thrd_success) {
    fprintf(stderr, "Could not set _idle to NULL.\n");
  }
  status = tss_set(saved, NULL);
  if (status != thrd_success) {
    fprintf(stderr, "Could not set saved to NULL.\n");
  }
}

/// @fn void coroutinePush(tss_t* list, Coroutine *coroutine)
///
/// @brief Add a coroutine to a list and get the previous head of the list.
///
/// @param list The Coroutine list to push onto.
/// @param coroutine A pointer to the Coroutine to push onto the list.
///
/// @param Returns the previous head of the list.
void coroutinePush(tss_t* list, Coroutine* coroutine) {
  if ((list != NULL) && (coroutine != NULL)) {
    coroutine->next = (Coroutine*) tss_get(*list);
    tss_set(*list, coroutine);
  }
}

/// @fn Coroutine* coroutinePop(tss_t* list)
///
/// @brief Remove a coroutine from a list and return it.
///
/// @param list The Coroutine list to pop from.
///
/// @return Returns a pointer to the previous head of the list.
Coroutine* coroutinePop(tss_t* list) {
  Coroutine* coroutine = NULL;

  if (list != NULL) {
    coroutine = (Coroutine*) tss_get(*list);
    tss_set(*list, coroutine->next);
    coroutine->next = NULL;
  }

  return coroutine;
}

#endif

/// @fn void* coroutinePass(Coroutine currentCoroutine, void *arg)
///
/// @brief Pass a value and control from one coroutine to another.  The target
/// coroutine is at the head of the "running" list.
///
/// @param currentCoroutine A pointer to the current coroutine's state.
/// @param arg The value to pass to the target coroutine.
///
/// @return Returns the target's returned or yielded value.
void* coroutinePass(Coroutine *currentCoroutine, void *arg) {
  if (currentCoroutine != NULL) {
#ifdef THREADSAFE_COROUTINES
    tss_set(saved, arg);
#else
    saved = arg;
#endif

    if (!setjmp(currentCoroutine->context)) {
#ifdef THREADSAFE_COROUTINES
      Coroutine* targetCoroutine = (Coroutine*) tss_get(_running);
#else
      Coroutine *targetCoroutine = running;
#endif
#if defined(_MSC_VER) && defined(_M_X64)
      // This should *NOT* be necessary.  The intent of longjmp is to restore
      // the context of the registers captured at setjmp, however the MSVC x64
      // implementation of longjmp only does this if the value of
      // _JUMP_BUFFER.Frame is 0.  This is a non-standard and broken
      // implementation, but thankfully a workaround does exist, so I won't
      // complain beyond this comment.
      _JUMP_BUFFER* context = (_JUMP_BUFFER*) &targetCoroutine->context;
      context->Frame = 0;
#endif // _MSC_VER
      longjmp(targetCoroutine->context, 1);
    }
  } else {
#ifdef THREADSAFE_COROUTINES
    tss_set(saved, NULL);
#else
    saved = NULL;
#endif
  }

#ifdef THREADSAFE_COROUTINES
  return tss_get(saved);
#else
  return saved;
#endif
}

/// @fn void* coroutineResume(Coroutine *targetCoroutine, void *arg)
///
/// @brief Transfer control to another coroutine.  A coroutine that is blocked
/// inside coroutineResume() is not resumable.
///
/// @param targetCoroutine A pointer to the Coroutine to resume.
/// @param arg Returned by coroutineYield() inside the target coroutine or
///   passed as the coroutine's parameter on the first call to coroutineResume
///   for the coroutine.
///
/// @return If the coroutine is resuamable, returns the value provided to the
///   yield call from within the coroutine or the coroutine's return value if it
///   has run to completion.  If the coroutine is not resumable, returns the
///   special value COROUTINE_NOT_RESUMABLE.
void* coroutineResume(Coroutine *targetCoroutine, void *arg) {
#ifdef THREADSAFE_COROUTINES
  call_once(&_threadMetadataSetup, coroutineSetupThreadMetadata);
#endif

  if (coroutineResumable(targetCoroutine)) {
#ifdef THREADSAFE_COROUTINES
    Coroutine* currentCoroutine = (Coroutine*) tss_get(_running);
    coroutinePush(&_running, targetCoroutine);
#else
    Coroutine *currentCoroutine = running;
    coroutinePush(&running, targetCoroutine);
#endif
    // The target coroutine is now at the head of the running list as is
    // expected by coroutinePass().
    void *calledCoroutineReturn = coroutinePass(currentCoroutine, arg);
    return calledCoroutineReturn;
  }

  return COROUTINE_NOT_RESUMABLE;
}

/// @fn void* coroutineYield(void *arg)
///
/// @brief Transfer control back to the coroutine that resumed this one.  A
/// coroutine that is blocked inside coroutineYield() may be resumed by any
/// other coroutine.
///
/// @param arg Value that will be returned by coroutineResume().
///
/// @return Returns the value passed into the next call to coroutineResume()
/// for this coroutine.
void* coroutineYield(void *arg) {
  void *returnValue = NULL;
#ifdef THREADSAFE_COROUTINES
  call_once(&_threadMetadataSetup, coroutineSetupThreadMetadata);
  Coroutine* first = (Coroutine*) tss_get(_first);
#else
  Coroutine* first = &_first;
#endif

#ifdef THREADSAFE_COROUTINES
  if (((Coroutine*) tss_get(_running)) != first) {
    Coroutine* currentCoroutine = coroutinePop(&_running);
#else
  if (running != first) {
    Coroutine *currentCoroutine = coroutinePop(&running);
#endif
    currentCoroutine->state = COROUTINE_STATE_BLOCKED;
    void *callingCoroutineArgument = coroutinePass(currentCoroutine, arg);
    currentCoroutine->state = COROUTINE_STATE_RUNNING;
    returnValue = callingCoroutineArgument;
  } // else, can't yield from the first coroutine.  Return NULL in this case.

  return returnValue;
}

/// @fn Coroutine* coroutineCreate(void* func(void *arg))
///
/// @brief The coroutine constructor function.
///
/// @details Create a coroutine that will run func(). The coroutine starts off
/// suspended.  When it is first resumed, the argument to coroutineResume() is
/// passed to func().  If func() returns, its return value is returned by
/// coroutineResume() as if the coroutine yielded, except that the coroutine is
/// then no longer resumable and may be discarded (*NOT* freed since its
/// allocation is on the stack and not the heap).
///
/// On the first invocation there are no idle coroutines, so fork the first one,
/// which will immediately yield back to us after becoming idle. When there are
/// idle coroutines, we pass one the function pointer and return the activated
/// coroutine's address.
///
/// @param Returns a newly-initialized Coroutine.
Coroutine* coroutineCreate(void* func(void *arg)) {
  FuncData funcData;
  funcData.func = func;
  if (funcData.data == NULL) {
    // Can't pass a NULL function pointer (this would crash).  Return a NULL
    // Coroutine.  This will cause any future calls with this returned value to
    // coroutineResumable to return false and calls to coroutineResume to return
    // COROUTINE_NOT_RESUMABLE.
    return NULL;
  }

#ifdef THREADSAFE_COROUTINES
  call_once(&_threadMetadataSetup, coroutineSetupThreadMetadata);
  Coroutine* idle = (Coroutine*) tss_get(_idle);
  Coroutine* running = (Coroutine*) tss_get(_running);
#endif
  // The current coroutine is at the head of the running list.
  if ((idle == NULL) && (!setjmp(running->context))) {
    // We've just been called from the calling function and need to create a
    // new Coroutine instance, including its stack.
    coroutineStart();
  }
  // Either there was an idle coroutine on the idle list or we just returned
  // from coroutineMain (called by coroutineStart).  Either way, the Coroutine
  // instance we want to use is now at the head of the idle list.

  // The head of the idle list has the Coroutine allocated in coroutineMain.
#ifdef THREADSAFE_COROUTINES
  Coroutine* newCoroutine = coroutinePop(&_idle);
#else
  Coroutine *newCoroutine = coroutinePop(&idle);
#endif

  // The head of the running list is the current coroutine.
  newCoroutine = (Coroutine*) coroutineResume(newCoroutine, funcData.data);
  return newCoroutine;
}

/// void coroutineMain(void *ret)
///
/// @brief The main loop responsible for managing the "idle" list.
///
/// @details When we start the idle list is empty, so we put ourself on it to
/// ensure it remains non-NULL. Then we immediately suspend ourself waiting for
/// the first function we are to run. (The head of the running list is the
/// coroutine that forked us.) We pass the stack pointer to prevent it from
/// being optimised away. The first time we are called we will return to the
/// fork in the coroutineCreate() constructor function (above); on subsequent
/// calls we will resume the parent coroutineMain(). In both cases the passed
/// value is lost when coroutinePass() longjmp()s to the forking setjmp().
///
/// When we are resumed, the idle list is empty again, so we fork another
/// coroutine. When the child coroutineMain() passes control back to us, we drop
/// into our main loop.
///
/// We are now head of the running list with a function to call. We immediately
/// yield a pointer to our context object so our creator can identify us. The
/// creator can then resume us at which point we pass the argument to the
/// function to start executing.
///
/// When the function returns, we move ourself from the running list to the idle
/// list, before passing the result back to the resumer. (This is just like
/// coroutineYield() except for adding the coroutine to the idle list.) We can
/// then only be resumed by the coroutineCreate() constructor function which
/// will put us back on the running list and pass us a new function to call.
void coroutineMain(void *ret) {
  ZEROINIT(Coroutine me);
  me.id = COROUTINE_ID_NOT_SET;
#ifdef THREADSAFE_COROUTINES
  coroutinePush(&_idle, &me);
#else
  coroutinePush(&idle, &me);
#endif

  // The target of coroutinePass() (the caller) is at the head of the running
  // list.  The return point for that Coroutine was either set in the setjmp
  // call in the coroutine constructor or in the setjmp call below.  In the
  // former case, calling coroutinePass() here returns to the constructor and
  // waits for the constructor to provide the function pointer to call.  In the
  // latter case, we allocate the next Coroutine and its stack that will be
  // pushed onto the idle list (above).  When we call pass here, we will release
  // the Coroutine currently being constructed from the setjmp below, allowing
  // it to drop into its main loop and we will be on the idle stack ready to
  // take in a new function pointer when we're resumed.
  FuncData funcData;
  funcData.data = coroutinePass(&me, ret);
  void *(*func)(void *arg);
  func = funcData.func;

  // At this point, we've been passed execution from the constructor calling
  // coroutineResume().  coroutineResume() pushed the new coroutine (the one
  // we're in the middle of constructing that was declared as "me" above) onto
  // the running list before returning control to us.  So the return point
  // we're about to set is for ourself.  The call to coroutineStart here will
  // allocate the next Coroutine on the idle list to be used in the next call
  // to the constructor.
#ifdef THREADSAFE_COROUTINES
  Coroutine* running = (Coroutine*) tss_get(_running);
#endif
  if (!setjmp(running->context)) {
    coroutineStart();
  }
  // We have just been passed execution from the coroutinePass() statement
  // above.  The stack is now configured and we're ready to begin execution.
  // We will first yield the Coroutine allocated above that the constructor is
  // waiting on and then call the function we were passed.  When the function
  // ends, we will place ourselves on the idle list and can be reused by a
  // future invocation of the coroutineCreate() constructor.
  while (1) {
    // Return our Coroutine and get the function argument from the constructor.
    // coroutineYield will set our state to BLOCKED on call and RUNNING on
    // return.
    void* callingArgument = coroutineYield(&me);

    // Call the target function with the calling argument.
    ret = func(callingArgument);

    // Deallocate the currently running coroutine and make it available to the
    // next iteration of the constructor.
    Coroutine *currentCoroutine = coroutinePop(&_running);
    currentCoroutine->id = COROUTINE_ID_NOT_SET;
    currentCoroutine->state = COROUTINE_STATE_NOT_RUNNING;
#ifdef THREADSAFE_COROUTINES
    coroutinePush(&_idle, currentCoroutine);
#else
    coroutinePush(&idle, currentCoroutine);
#endif

    // Block until we're called from the constructor again.
    funcData.data = coroutinePass(&me, ret);
    func = funcData.func;
  }
}

/// void coroutineStart(void)
///
/// @brief Allocate space for the current stack to grow before creating the
/// initial stack frame for the next coroutine.
///
/// @return This function returns no value.
void coroutineStart(void) {
  ZEROINIT(char stack[COROUTINE_STACK_SIZE]);
  coroutineMain(stack);
}

/// @fn int coroutineSetId(Coroutine* coroutine, int64_t id)
///
/// @brief Set the ID associated with a coroutine.
///
/// @param coroutine A pointer to the coroutine whose ID is to be set.  If this
///   value is NULL then the ID of the currently running coroutine will be set.
/// @param id A 64-bit signed integer to set as the coroutine's ID.
///
/// @return This function always returns coroutineSuccess.
int coroutineSetId(Coroutine* coroutine, int64_t id) {
  if (coroutine == NULL) {
#ifdef THREADSAFE_COROUTINES
    call_once(&_threadMetadataSetup, coroutineSetupThreadMetadata);
    coroutine = (Coroutine*) tss_get(_running);
#else
    coroutine = running;
#endif
    // running should always be non-NULL, so we shouldn't need to check again.
  }
  coroutine->id = id;

  return coroutineSuccess;
}

/// @fn int64_t coroutineId(Coroutine* coroutine)
///
/// @brief Get the ID associated with a coroutine.
///
/// @param coroutine A pointer to the coroutine of interest.  If this value is
///   NULL then the ID of the currently running coroutine will be returned.
///
/// @return Returns the ID of the specified or current coroutine.  The ID
/// returned will be COROUTINE_ID_NOT_SET if the ID of the coroutine has not
/// been previously set with a call to coroutineSetId.
int64_t coroutineId(Coroutine* coroutine) {
  if (coroutine == NULL) {
#ifdef THREADSAFE_COROUTINES
    call_once(&_threadMetadataSetup, coroutineSetupThreadMetadata);
    coroutine = (Coroutine*) tss_get(_running);
#else
    coroutine = running;
#endif
    // running should always be non-NULL, so we shouldn't need to check again.
  }

  return coroutine->id;
}

/// @fn CoroutineState coroutineState(Coroutine* coroutine)
///
/// @brief Get the state of a specified coroutine.
///
/// @param coroutine The Coroutine to examine.
///
/// @return Returns the state of the coroutine on success, NOT_RUNNING if the
/// provided pointer is NULL.
CoroutineState coroutineState(Coroutine* coroutine) {
  CoroutineState state = COROUTINE_STATE_NOT_RUNNING;

  if (coroutine != NULL) {
    state = coroutine->state;
  }

  return state;
}

/// @fn int comutexInit(Comutex* mtx, int type)
///
/// @brief Initialize a coroutine mutex.
///
/// @param mtx A pointer to the allocated coroutine mutex to initialize.
/// @param type A bitwise-ored integer of the type of mutex this is to be.
///
/// @return This function always returns coroutine success in the current
/// implementation.
int comutexInit(Comutex *mtx, int type) {
  int returnValue = coroutineSuccess;

  if (mtx != NULL) {
    mtx->lastYieldValue = NULL;
    mtx->type = type;
    mtx->coroutine = NULL;
    mtx->recursionLevel = 0;
  } else {
    returnValue = coroutineError;
  }

  return returnValue;
}

/// @fn int comutexLock(Comutex* mtx)
///
/// @brief Lock a coroutine mutex.
///
/// @details This function blocks the current coroutine, yielding each time it
/// tries and fails to acquire the lock.  The special value COROUTINE_BLOCKED
/// will be yielded to the caller each time control is yielded.
///
/// @param mtx A pointer to the coroutine mutex to lock.
///
/// @return Returns coroutineSuccess when the lock is acquired, coroutineError
/// if the provided pointer is NULL.
int comutexLock(Comutex *mtx) {
  if (mtx == NULL) {
    // Cannot honor the request.
    return coroutineError;
  }

  // Clear the lastYieldValue before we do anything else.
  mtx->lastYieldValue = NULL;

  while (comutexTrylock(mtx) != coroutineSuccess) {
    mtx->lastYieldValue = coroutineYield(COROUTINE_BLOCKED);
  }

  return coroutineSuccess;
}

/// @fn int comutexUnlock(Comutex* mtx)
///
/// @brief Unlock a previously-locked coroutine mutex.
///
/// @param mtx A pointer to the he coroutine mutex to unlock.
///
/// @return Returns coroutineSuccess if the currently-running coroutine has the
/// lock, coroutineError otherwise.  If the currently-running coroutine has the
/// lock and the depth of the unlock calls matches the depth of the depth of the
/// lock calls (mtx->recursionLevel reaches 0 with this call), the mutex is
/// unlocked.
int comutexUnlock(Comutex *mtx) {
  int returnValue = coroutineSuccess;

#ifdef THREADSAFE_COROUTINES
  call_once(&_threadMetadataSetup, coroutineSetupThreadMetadata);
  Coroutine* running = (Coroutine*) tss_get(_running);
#endif
  if ((mtx != NULL) && (mtx->coroutine == running)) {
    mtx->recursionLevel--;
    if (mtx->recursionLevel == 0) {
      mtx->coroutine = NULL;
    }
  } else {
    returnValue = coroutineError;
  }

  return returnValue;
}

/// @fn void comutexDestroy(Comutex* mtx)
///
/// @brief Destroy a previously-initialized coroutine mutex.
///
/// @param mtx A pointer to the mutex to destory.
///
/// @return This function returns no value.
void comutexDestroy(Comutex *mtx) {
  if (mtx != NULL) {
    mtx->lastYieldValue = NULL;
    mtx->type = 0;
    mtx->coroutine = NULL;
    mtx->recursionLevel = 0;
  }
}

/// @fn int comutexTimedlock(Comutex* mtx, const struct timespec* ts)
///
/// @brief Attempt to lock a coroutine mutex until the lock is acquired or a
/// specified time is reached, whichever comes first.
///
/// @param mtx A pointer to the mutex to destory.
/// @param ts A pointer to a struct timespec instance that specifies the future
///   deadline for abandoning attempts to lock the mutex.
///
/// @return Returns coroutineSuccess if the lock is acquired before the timeout
/// is reached, coroutineTimeout if the timeout is reached before the lock is
/// acquired, and coroutineError if the coroutine mutex is not a timed mutex,
/// if the current system time could not be acquired, or if one of the provided
/// parameters is NULL.
int comutexTimedlock(Comutex *mtx, const struct timespec *ts) {
  if ((mtx == NULL) || (ts == NULL)) {
    // Cannot honor the request.
    return coroutineError;
  }

  // Clear the lastYieldValue before we do anything else.
  mtx->lastYieldValue = NULL;

  if (!(mtx->type & comutexTimed)) {
    // This is not a timed mutex.  It does not support timeouts.  We fail.
    return coroutineError;
  }

  int returnValue = comutexTrylock(mtx);
  while (returnValue != coroutineSuccess) {
    struct timespec now;
    if (timespec_get(&now, TIME_UTC) == 0) {
      uint64_t nowns = (now.tv_sec * 1000000000) + now.tv_nsec;
      uint64_t tsns = (ts->tv_sec * 1000000000) + ts->tv_nsec;
      if (nowns > tsns) {
        returnValue = coroutineTimedout;
        break;
      }
      mtx->lastYieldValue = coroutineYield(COROUTINE_BLOCKED);
      returnValue = comutexTrylock(mtx);
    } else {
      // timespec_get returned an error.  We have no valid time to wait.  We've
      // already tried to lock once and that's the best we can do.
      returnValue = coroutineError;
      break;
    }
  }

  return returnValue;
}

/// @fn int comutexTrylock(Comutex* mtx)
///
/// @brief Make one attempt to lock a coroutine mutex.
///
/// @param mtx A pointer to the coroutine mutex to attempt to lock.
///
/// @return Returns coroutineSuccess if the mutex is unlocked or if the current
/// coroutine has the lock and the mutex is recursive, coroutineBusy if the
/// mutex is locked by antoher coroutine, and coroutineError under any other
/// conditions.
int comutexTrylock(Comutex *mtx) {
  if (mtx == NULL) {
    // Cannot honor the request.
    return coroutineError;
  }

  int returnValue = coroutineError;

#ifdef THREADSAFE_COROUTINES
  call_once(&_threadMetadataSetup, coroutineSetupThreadMetadata);
  Coroutine* running = (Coroutine*) tss_get(_running);
#endif
  if (mtx->coroutine == NULL) {
    mtx->coroutine = running;
    mtx->recursionLevel = 1;
    returnValue = coroutineSuccess;
  } else if ((mtx->coroutine == running) && (mtx->type & comutexRecursive)) {
    mtx->recursionLevel++;
    returnValue = coroutineSuccess;
  } else if (mtx->coroutine != running) {
    returnValue = coroutineBusy;
  } // else any other situation is an error, which is the value of returnValue

  return returnValue;
}

/// @fn void* comutexLastYieldValue(Comutex* mtx)
///
/// @brief Get the last value returned by a yield call in a blocking comutex
/// lock function.
///
/// @param mtx A pointer to the comutex to interrogate.
///
/// @return Returns the last value returned by a blocking comutex lock function
/// performed on the specified if there is such a value.  NULL if the lock
/// function succeeded without yielding or if the provided comutex is NULL.
void* comutexLastYieldValue(Comutex* mtx) {
  void *returnValue = NULL;

  if (mtx != NULL) {
    returnValue = mtx->lastYieldValue;
  }

  return returnValue;
}

/// @fn int coconditionBroadcast(Cocondition *cond)
///
/// @brief Broadcast a condition to all coroutines blocked on it.
///
/// @param cond A pointer to the coroutine condition to broadcast.
///
/// @return Returns coroutineSuccess on success, coroutineError if the call
/// could not be honored (cond is NULL).
int coconditionBroadcast(Cocondition *cond) {
  int returnValue = coroutineSuccess;

  if (cond != NULL) {
    cond->numSignals = cond->numWaiters;
  } else {
    returnValue = coroutineError;
  }

  return returnValue;
}

/// @fn void coconditionDestroy(Cocondition* cond)
///
/// @brief Destroy a previously initialized coroutine condition.
///
/// @param cond A pointer to the coroutine condition to destroy.
///
/// @return This function returns no value.
void coconditionDestroy(Cocondition *cond) {
  if (cond != NULL) {
    cond->lastYieldValue = NULL;
    cond->numWaiters = 0;
    cond->numSignals = -1;
  }
}

/// @fn int coconditionInit(Cocondition* cond)
///
/// @brief Initialize a coroutine condition variable.
///
/// @param cond A pointer to the coroutine condition to destroy.
///
/// @return Returns coroutineSuccess on successful initialization,
/// coroutineError if the request could not be honored (cond is NULL).
int coconditionInit(Cocondition* cond) {
  int returnValue = coroutineSuccess;

  if (cond != NULL) {
    cond->lastYieldValue = NULL;
    cond->numWaiters = 0;
    cond->numSignals = 0;
  } else {
    returnValue = coroutineError;
  }

  return returnValue;
}

/// @fn int coconditionSignal(Cocondition *cond)
///
/// @brief Signal a single coroutiune blocked on a condition.
///
/// @param cond A pointer to the coroutine condition to signal.
///
/// @return Returns coroutineSuccess on successful signalling, coroutineError
/// if the call could not be honored (cond is NULL).
int coconditionSignal(Cocondition *cond) {
  int returnValue = coroutineSuccess;

  if (cond != NULL) {
    cond->numSignals++;
  } else {
    returnValue = coroutineError;
  }

  return returnValue;
}

/// @fn int conditionTimedwait(Cocondition* cond, Comutex* mtx, const struct timespec* ts)
///
/// @brief Wait for a condition to be signalled or until a specified time,
/// whichever comes first.
///
/// @param cond A pointer to the condition to wait on.
/// @param mtx A mutex for the condition that must be locked before this call
///   is made.  It will be unlocked before blocking on the condition and locked
///   again before the function returns.
/// @param ts A struct timespec that specifies the future deadline of the wait.
///
/// @return Returns coroutineSuccess on success, coroutineTimedout if the
/// deadline is reached before the condition is signalled, or coroutineError
/// if the request could not be honored (a parameter is NULL or timespec_get
/// fails).
int conditionTimedwait(Cocondition *cond, Comutex *mtx,
  const struct timespec *ts
) {
  if ((cond == NULL) || (mtx == NULL) || (ts == NULL)) {
    // Cannot honor the request.
    return coroutineError;
  }

  // Clear the lastYieldValue before we do anything else.
  cond->lastYieldValue = NULL;

  comutexUnlock(mtx);

  cond->numWaiters++;
  int returnValue = coroutineSuccess;
  while (cond->numSignals == 0) {
    cond->lastYieldValue = coroutineYield(COROUTINE_BLOCKED);

    struct timespec now;
    if (timespec_get(&now, TIME_UTC) == 0) {
      uint64_t nowns = (now.tv_sec * 1000000000) + now.tv_nsec;
      uint64_t tsns = (ts->tv_sec * 1000000000) + ts->tv_nsec;
      if (nowns > tsns) {
        returnValue = coroutineTimedout;
        break;
      }
    } else if (cond->numSignals == 0) {
      // timespec_get returned an error.  We have no valid time to wait.  We've
      // already tried to wait once and that's the best we can do.
      returnValue = coroutineError;
      break;
    }
  }
  if (cond->numSignals > 0) {
    cond->numSignals--;
    cond->numWaiters--;
    returnValue = coroutineSuccess;
  } else {
    // The condition has been destroyed out from under us.  Invalid state.
    returnValue = coroutineError;
  }

  comutexLock(mtx);
  return returnValue;
}

/// @fn int coconditionWait(Cocondition* cond, Comutex* mtx)
///
/// @brief Wait for the specified condition to be signalled.
///
/// @param cond A pointer to the condition to wait on.
/// @param mtx A mutex for the condition that must be locked before this call
///   is made.  It will be unlocked before blocking on the condition and locked
///   again before the function returns.
///
/// @return Returns coroutineSuccess on success or coroutineError if the request
/// could not be honored (one or more NULL parameters).
int coconditionWait(Cocondition *cond, Comutex *mtx) {
  if ((cond == NULL) || (mtx == NULL)) {
    // Cannot honor the request.
    return coroutineError;
  }

  // Clear the lastYieldValue before we do anything else.
  cond->lastYieldValue = NULL;

  comutexUnlock(mtx);

  cond->numWaiters++;
  int returnValue = coroutineSuccess;
  while (cond->numSignals == 0) {
    cond->lastYieldValue = coroutineYield(COROUTINE_BLOCKED);
  }
  if (cond->numSignals > 0) {
    cond->numSignals--;
    cond->numWaiters--;
  } else {
    // The condition has been destroyed out from under us.  Invalid state.
    returnValue = coroutineError;
  }

  comutexLock(mtx);
  return returnValue;
}

/// @fn void* coconditionLastYieldValue(Cocondition* cond)
///
/// @brief Retrieve the last value yielded to a condition wait call.
///
/// @returns The last value yielded on the conditon on success, NULL if the
/// provided condition pointer is NULL.
void* coconditionLastYieldValue(Cocondition* cond) {
  void *returnValue = NULL;

  if (cond != NULL) {
    returnValue = cond->lastYieldValue;
  }

  return returnValue;
}

