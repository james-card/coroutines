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

#ifndef _MSC_VER
#include "PosixCThreads.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
#define ZEROINIT(x) x = {}
#else // __cplusplus not defined
#define ZEROINIT(x) x = {0}
#endif // __cplusplus

#ifdef __cplusplus
extern "C"
{
#endif

// Functions, variables, and types defined in CThreadsMessages.c
extern once_flag thrd_msg_q_storage_initialized;
typedef struct thrd_msg_q_t thrd_msg_q_t;
void thrd_msg_q_storage_init(void);
int thrd_msg_q_create(void);
int thrd_msg_q_destroy(thrd_msg_q_t *queue);

#ifdef __cplusplus
}
#endif

void call_once(once_flag* flag, void(*func)(void)) {
  pthread_once(flag, func);
}

int mtx_init(mtx_t *mtx, int type) {
  int returnValue = thrd_success;
  
  if ((type & mtx_recursive) != 0) {
    pthread_mutexattr_t attribs;
    memset(&attribs, 0, sizeof(attribs));
    int err = pthread_mutexattr_init(&attribs);
    if (err != 0) {
      fputs("pthread_mutexattr_init: ", stderr);
      fputs(strerror(err), stderr);
      fputs("\n", stderr);
      returnValue = thrd_error;
      return returnValue;
    }
    err = pthread_mutexattr_settype(&attribs, PTHREAD_MUTEX_RECURSIVE);
    if (err != 0) {
      fputs("pthread_mutexattr_settype: ", stderr);
      fputs(strerror(err), stderr);
      fputs("\n", stderr);
      returnValue = thrd_error;
      return returnValue;
    }
    
    memset(mtx, 0, sizeof(*mtx));
    returnValue = pthread_mutex_init(mtx, &attribs);
    
    err = pthread_mutexattr_destroy(&attribs);
    if (err != 0) {
      fputs("pthread_mutexattr_destroy: ", stderr);
      fputs(strerror(err), stderr);
      fputs("\n", stderr);
    }
  } else {
     returnValue = pthread_mutex_init(mtx, NULL);
  }
  
  if (returnValue != 0) {
    fputs("pthread_mutex_init: ", stderr);
    fputs(strerror(returnValue), stderr);
    fputs("\n", stderr);
    returnValue = thrd_error;
  }
  
  return returnValue;
}

int mtx_timedlock(mtx_t* mtx, const struct timespec* ts) {
  int returnValue = thrd_success;
  
#ifdef _GNU_SOURCE
  returnValue = pthread_mutex_timedlock(mtx, ts);
  
  if (returnValue == ETIMEDOUT) {
    returnValue = thrd_timedout;
  } else if (returnValue != 0) {
    returnValue = thrd_error;
  }
#else
  // We're on some POSIX system but we're not compiling with gcc.  We can't rely
  // on pthread_mutex_timedlock being implemented, so we're going to have to do
  // a hack.
  struct timespec nowTimespec;
  timespec_get(&nowTimespec, TIME_UTC);
  long long int now64
    = (nowTimespec.tv_sec * 1000000000) + nowTimespec.tv_nsec;
  long long int ts64 = (ts->tv_sec * 1000000000) + ts->tv_nsec;
  returnValue = mtx_trylock(mtx);
  while ((returnValue == thrd_busy) && (now64 < ts64)) {
    timespec_get(&nowTimespec, TIME_UTC);
    now64 = (nowTimespec.tv_sec * 1000000000) + nowTimespec.tv_nsec;
    returnValue = mtx_trylock(mtx);
  }
  
  if (returnValue == thrd_busy) {
    returnValue = thrd_timedout;
  }
#endif
  
  return returnValue;
}

int mtx_trylock(mtx_t *mtx) {
  int returnValue = thrd_success;
  
  returnValue = pthread_mutex_trylock(mtx);
  
  if (returnValue == EBUSY) {
    returnValue = thrd_busy;
  } else if (returnValue != 0) {
    returnValue = thrd_error;
  }
  
  return returnValue;
}

int mtx_lock(mtx_t *mtx) {
  int returnValue = thrd_success;
  
  returnValue = pthread_mutex_lock(mtx);
  
  if (returnValue != 0) {
    returnValue = thrd_error;
  }
  
  return returnValue;
}

int mtx_unlock(mtx_t *mtx) {
  int returnValue = thrd_success;
  
  returnValue = pthread_mutex_unlock(mtx);
  
  if (returnValue != 0) {
    returnValue = thrd_error;
  }
  
  return returnValue;
}

void mtx_destroy(mtx_t* mtx) {
  pthread_mutex_destroy(mtx);
}

#ifndef _WIN32
int timespec_get(struct timespec* spec, int base) {
  clock_gettime(CLOCK_REALTIME, spec);
  return base;
}
#else // We're in MinGW and need to supply a Windows method.
#include <sysinfoapi.h>

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
#endif

typedef struct PthreadCreateWrapperArgs {
  thrd_start_t func;
  void *arg;
} PthreadCreateWrapperArgs;

void *posix_c_threads_create_wrapper(void* wrapper_args) {
  // We want to be able to kill this thread if we need to.
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  
  // Create the message queue for this thread.
  thrd_msg_q_create();
  
  PthreadCreateWrapperArgs *cthread_args
    = (PthreadCreateWrapperArgs*) wrapper_args;
  thrd_start_t func = cthread_args->func;
  void *arg = cthread_args->arg;
  // Free our args so that the called function has as much memory as possible.
  free(cthread_args); cthread_args = NULL;
  
  // The C17 specification changed the definition of errno to be thread-local
  // storage.  However, it says that the value is initialized to zero only on
  // the main thread.  It is undefined on all other threads.  Fix this.
  errno = 0;
  
  int return_value = func(arg);
  thrd_exit(return_value);
  return (void*) ((intptr_t) return_value);
}

int thrd_create(thrd_t *thr, thrd_start_t func, void *arg) {
  if (thr == NULL) {
    return thrd_error;
  }

  int returnValue = thrd_success;
  
  call_once(&thrd_msg_q_storage_initialized, thrd_msg_q_storage_init);
  
  PthreadCreateWrapperArgs *wrapper_args
    = (PthreadCreateWrapperArgs*) malloc(sizeof(PthreadCreateWrapperArgs));
  if (wrapper_args == NULL) {
    return thrd_error;
  }
  
  wrapper_args->func = func;
  wrapper_args->arg = arg;
  
  returnValue = pthread_create(thr, NULL,
    posix_c_threads_create_wrapper, wrapper_args);
  
  if (returnValue != 0) {
    returnValue = thrd_error;
    free(wrapper_args); wrapper_args = NULL;
  }
  
  return returnValue;
}

thrd_t thrd_current(void) {
  return pthread_self();
}

int thrd_detach(thrd_t thr) {
  int returnValue = thrd_success;
  
  returnValue = pthread_detach(thr);
  
  if (returnValue != 0) {
    returnValue = thrd_error;
  }
  
  return returnValue;
}

int thrd_equal(thrd_t thr0, thrd_t thr1) {
  return pthread_equal(thr0, thr1);
}

void thrd_exit(int res) {
  // Destroy the message queue for this thread.
  thrd_msg_q_destroy(NULL);

  pthread_exit((void*) ((intptr_t) res));
}

int thrd_join(thrd_t thr, int *res) {
  int returnValue = thrd_success;
  intptr_t threadReturnValue = 0;
  
  // The return value of the function is an int cast to a void pointer,
  // however pthread_join writes to a void pointer, so the variable we provide
  // has to be large enough to hold a void pointer value.  We can then downcast
  // the value to a standard int.
  returnValue = pthread_join(thr, (void**) &threadReturnValue);
  if (res != NULL) {
    *res = (int) threadReturnValue;
  }
  
  if (returnValue != 0) {
    returnValue = thrd_error;
  }
  
  return returnValue;
}

int thrd_sleep(const struct timespec* duration, struct timespec* remaining) {
  return nanosleep(duration, remaining);
}

void thrd_yield(void) {
  sched_yield();
}

int thrd_terminate(thrd_t thr) {
  int returnValue = thrd_success;
  
  returnValue = pthread_cancel(thr);
  
  if (returnValue != 0) {
    returnValue = thrd_error;
  }
  
  return returnValue;
}

int tss_create(tss_t *key, tss_dtor_t dtor) {
  int returnValue = thrd_success;
  
  returnValue = pthread_key_create(key, dtor);
  
  if (returnValue != 0) {
    returnValue = thrd_error;
  }
  
  return returnValue;
}

void tss_delete(tss_t key) {
  pthread_key_delete(key);
}

void* tss_get(tss_t key) {
  return pthread_getspecific(key);
}

int tss_set(tss_t key, void *val) {
  int returnValue = thrd_success;
  
  returnValue = pthread_setspecific(key, val);
  
  if (returnValue != 0) {
    returnValue = thrd_error;
  }
  
  return returnValue;
}

int cnd_broadcast(cnd_t *cond) {
  int returnValue = thrd_success;
  
  returnValue = pthread_cond_broadcast(cond);
  
  if (returnValue != 0) {
    returnValue = thrd_error;
  }
  
  return returnValue;
}

void cnd_destroy(cnd_t* cond) {
  pthread_cond_destroy(cond);
}

int cnd_init(cnd_t *cond) {
  int returnValue = thrd_success;
  
  returnValue = pthread_cond_init(cond, NULL);
  
  if (returnValue != 0) {
    returnValue = thrd_error;
  }
  
  return returnValue;
}

int cnd_signal(cnd_t *cond) {
  int returnValue = thrd_success;
  
  returnValue = pthread_cond_signal(cond);
  
  if (returnValue != 0) {
    returnValue = thrd_error;
  }
  
  return returnValue;
}

int cnd_timedwait(cnd_t* cond, mtx_t* mtx, const struct timespec* ts) {
  int returnValue = thrd_success;
  
  returnValue = pthread_cond_timedwait(cond, mtx, ts);
  
  if (returnValue == ETIMEDOUT) {
    returnValue = thrd_timedout;
  } else if (returnValue != 0) {
    returnValue = thrd_error;
  }
  
  return returnValue;
}

int cnd_wait(cnd_t *cond, mtx_t *mtx) {
  int returnValue = thrd_success;
  
  returnValue = pthread_cond_wait(cond, mtx);
  
  if (returnValue != 0) {
    returnValue = thrd_error;
  }
  
  return returnValue;
}

#endif // _MSC_VER

