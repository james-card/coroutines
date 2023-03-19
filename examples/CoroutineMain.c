#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <stdint.h>

#include "Coroutines.h"
#ifdef THREAD_SAFE_COROUTINES
#include "CThreads.h"
#endif

#define NUM_COROUTINES 3

/// @fn uint64_t getElapsedMicroseconds(uint64_t previousTime)
///
/// @brief Get the number of microseconds that has elapsed since a previous
/// call to this function or since the start of the epoch.
///
/// @param previousTime A value returned from a previous call to this function
///   or 0 to indicate to get the number of microseconds since the start of the
///   epoch.
///
/// @return Returns the number of microseconds since the time provieded.
uint64_t getElapsedMicroseconds(uint64_t previousTime) {
  struct timespec now;
  timespec_get(&now, TIME_UTC);
  uint64_t nowMicroseconds
    = ((uint64_t) now.tv_sec * 1000000ULL)
    + ((uint64_t) now.tv_nsec / 1000ULL);
  return nowMicroseconds - previousTime;
}

/// @struct CoroutineArgs
///
/// @brief Arguments to be passed to a new coroutine.
///
/// @param comutex A pointer to the Comutex that will synchronize the different
///   coroutines.
/// @param cocondition A pointer to the Cocondition that will gate starting
///   execution of the coroutine.
/// @param globalInt A pointer to the global integer that is updated by each
///   coroutine instance.
/// @param coroutineStorage A pointer to an array of integers that represents
///   "global" coroutine-specific storage.
typedef struct CoroutineArgs {
  Comutex *comutex;
  Cocondition *cocondition;
  int *globalInt;
  int *coroutineStorage;
} CoroutineArgs;

/// @fn void* coroutine(void *args)
///
/// @brief The template coroutine that will be executed in multiple instances
/// by parent functions.
///
/// @details This function is intended to be run in multiple instances by a
/// caller to collect performance metrics on coroutines.  It also demonstrates
/// the basic mechanics of a coroutine such as use of mutexes and conditions
/// for synchronization and using an coroutine-specific identifier to find its
/// information in a provided array of information.  It will run until the
/// provided globalInt parameter reaches a value of at least 20000, at which
/// point it will return NULL to the caller.  It yields once each time it
/// updates the globalInt.  One of those two times, it will maintain a lock on
/// the provided mutex.  The second time, the mutex will be released so that
/// the other instances of this function can resume execution.  It will block
/// on a mutex lock until the lock is available again.  The routine will ensure
/// that the mutex is unlocked when it returns.
///
/// @param args A pointer to a CoroutineArgs instance that has been cast to a
///   void*.
///
/// @param Yields a pointer to the shared globalInt during execution, returns
/// NULL on completion.
void* coroutine(void *args) {
  CoroutineArgs *coroutineArgs = (CoroutineArgs*) args;
  int *globalInt = coroutineArgs->globalInt;
  Comutex *comutex = coroutineArgs->comutex;
  Cocondition *cocondition = coroutineArgs->cocondition;
  int *coroutineStorage = coroutineArgs->coroutineStorage;
  int functionNumber = coroutineStorage[coroutineId(NULL)];
  bool mutexLocked = false;

  comutexLock(comutex);
  coconditionWait(cocondition, comutex);
  // Print out some stats before we unlock the mutex.
  printf("%d signals, %d waiters remaining.\n",
    cocondition->numSignals, cocondition->numWaiters);
  if (cocondition->head != NULL) {
    printf("Coroutine %lld will be signaled next.\n",
      (long long int) cocondition->head->id);
  } else {
    printf("No coroutine will be signaled next.\n");
  }
  comutexUnlock(comutex);

  printf("%s%d:  Starting while loop.\n", __func__, functionNumber);
  while (*globalInt < 20000) {
    if (mutexLocked == false) {
      comutexLock(comutex);
      mutexLocked = true;
    }
    assert(mutexLocked == true);
    void* lastYieldValue = comutexLastYieldValue(comutex);
    if (lastYieldValue != NULL) {
      // We've been passed new arguments.
      // We can't update comutex or cocondition without breaking things.
      // Update our function number and globalInt.
      coroutineArgs = (CoroutineArgs*) lastYieldValue;
      coroutineStorage = coroutineArgs->coroutineStorage;
      functionNumber = coroutineStorage[coroutineId(NULL)];
      globalInt = coroutineArgs->globalInt;
    }

    (*globalInt)++;
    printf("%s%d:  %d\n", __func__, functionNumber, *globalInt);
    if (((*globalInt) & 1) == 0) {
      if (comutexUnlock(comutex) == coroutineSuccess) {
        mutexLocked = false;
      } else {
        fprintf(stderr, "Attempt to unlock comutex failed.\n");
      }
    }
    lastYieldValue = coroutineYield(globalInt);
  }

  if (mutexLocked == true) {
    comutexUnlock(comutex);
  }

  printf("%s%d:  Exiting\n", __func__, functionNumber);
  return NULL;
}

/// @fn int scheduleRoundRobin(Coroutine *coroutineArray[], int numCoroutines)
///
/// @brief Simple round-robin scheduler for coroutines.  Runs until all of the
/// provided coroutines have run to completion.
///
/// @param coroutineArray An array of Coroutine pointers that have been
///   initialized by the caller.
/// @param numCoroutines The number of Coroutine poniters in coroutineArray.
///
/// @return Returns 0 on success, 1 on error.
int scheduleRoundRobin(Coroutine *coroutineArray[], int numCoroutines) {
  void* status = NULL;
  int coroutineIndex = 0;
  int numCoroutinesRun = 0;

  do {
    if (coroutineIndex == numCoroutines) {
      coroutineIndex = 0;
      numCoroutinesRun = 0;
    }
    if ((coroutineResumable(coroutineArray[coroutineIndex]))
      && (!coroutineFinished(coroutineArray[coroutineIndex]))
    ) {
      status = coroutineResume(coroutineArray[coroutineIndex], NULL);
      numCoroutinesRun++;
    }
    if (status == COROUTINE_NOT_RESUMABLE) {
      fprintf(stderr,
        "ERROR:  Coroutine %d was resumable but returned not resumable.\n",
        coroutineIndex);
      return 1;
    }
    coroutineIndex++;
  } while (numCoroutinesRun > 0);

  return 0;
}

#ifdef THREAD_SAFE_COROUTINES
/// @struct LoadAndRunCoroutinesArgs
///
/// @brief In thread-enabled systems, this structure contains the parameters to
/// loadAndRunCoroutines.
///
/// @param threadCondition A pointer to an initialized cnd_t that will gate
///   start of execution.
/// @param threadMutex A pointer to an initialized mtx_t that is used by
///   threadCondition for synchronization.
typedef struct LoadAndRunCoroutinesArgs {
  cnd_t *threadCondition;
  mtx_t *threadMutex;
} LoadAndRunCoroutinesArgs;
#endif

/// @fn int loadAndRunCoroutines(void *args)
///
/// @brief Declare and initialize all of the subordinate coroutines, run them,
/// time the execution, and return the duration.
///
/// @param args A pointer to a LoadAndRunCoroutinesArgs structure that has been
///   cast to a void*.
///
/// @param Returns the number of microseconds required for execution.
int loadAndRunCoroutines(void *args) {
#ifdef THREAD_SAFE_COROUTINES
  LoadAndRunCoroutinesArgs *mutexAndCondition
    = (LoadAndRunCoroutinesArgs*) args;
  if (mutexAndCondition != NULL) {
    // Wait for the caller to signal us.
    mtx_lock(mutexAndCondition->threadMutex);
    cnd_wait(mutexAndCondition->threadCondition,
      mutexAndCondition->threadMutex);
    mtx_unlock(mutexAndCondition->threadMutex);
  }
#else
  (void) args;
#endif

  Coroutine mainCoroutine;

  if (coroutineConfig(1, NULL) != coroutineSuccess) {
    fprintf(stderr, "ERROR:  Could not set coroutine stack size to 1K.\n");
  }
  if (coroutineConfig(4, &mainCoroutine) != coroutineSuccess) {
    fprintf(stderr, "ERROR:  Could not set coroutine stack size to 4K.\n");
  }

  Coroutine *coroutineArray[NUM_COROUTINES];
  // Assign the instances function numbers.
  int coroutineStorage[NUM_COROUTINES] = {1, 2, 3};

  Comutex comutex;
  if (comutexInit(&comutex, comutexPlain) != coroutineSuccess) {
    fprintf(stderr, "Could not initialize comutex.\n");
    return 1;
  }

  Cocondition cocondition;
  if (coconditionInit(&cocondition) != coroutineSuccess) {
    fprintf(stderr, "Could not initialize cocondition.\n");
  }

  int globalInt = 0;
  CoroutineArgs coroutineArgs;
  coroutineArgs.comutex = &comutex;
  coroutineArgs.cocondition = &cocondition;
  coroutineArgs.globalInt = &globalInt;
  coroutineArgs.coroutineStorage = coroutineStorage;

  // Instantiate and initialize the coroutines.
  for (int i = 0; i < NUM_COROUTINES; i++) {
    coroutineArray[i] = coroutineCreate(coroutine);
    if (coroutineArray[i] == NULL) {
      fprintf(stderr, "Could not initialize coroutine %d.\n", i);
      return 1;
    }
    coroutineSetId(coroutineArray[i], i);
    coroutineResume(coroutineArray[i], &coroutineArgs);
  }

  coconditionBroadcast(&cocondition);

  uint64_t startTime = getElapsedMicroseconds(0);
  int status = scheduleRoundRobin(coroutineArray, NUM_COROUTINES);
  if (status != 0) {
    fprintf(stderr,
      "Scheduled coroutines completed with one or more errors.\n");
  }
  uint64_t runTime = getElapsedMicroseconds(startTime);

  return (int) runTime;
}

/// @fn int main(int argc, char **argv)
///
/// @brief Main entry point of the program.
///
/// @details Runs a full set of three coroutines once without threading support
/// and discards the return value.  This is to prime branch prediction which
/// improves over time.  Runs the full set of coroutines a second time and
/// captures the time requried for execution.
///
/// If running in a system that supports threading (THREAD_SAFE_COROUTINES is
/// defined), enables support for threading in the Coroutines library and
/// re-runs the set of coroutines and again captures the duration of execution.
///
/// If running in a system that supports threading, launches three threads,
/// each of which will run the same three coroutines in its own stack.  Each
/// thread will return the number of microseconds required for execution.  These
/// three times will be collected and averaged into one time.
///
/// The duration of the run without threading support will be printed.  If
/// running in a system that supports threading, the duration of the single-
/// threaded will be run, followed by an analysis of how this time compared to
/// the run without threading support.  Following that, the averaged time for
/// the three threads will be printed follwed by an analys of how that time
/// compared to the single-threaded run.
///
/// @param argc The number of arguments provided in argv.
/// @param argv An array of char* that represent the provided command line
///   parameters.
///
/// @return Returns 0 on success, 1 on failure.
int main(int argc, char **argv) {
  (void) argc;
  (void) argv;

  // Have to run this once and throw away the time to prime branch prediction.
  loadAndRunCoroutines(NULL);

  // Coroutine threading support is disabled by default.
  int noThreadingRunTime = loadAndRunCoroutines(NULL);

#ifdef THREAD_SAFE_COROUTINES
  // Get baseline with threading enabled but no concurrent threads.
  coroutineSetThreadingSupportEnabled(true);
  int threadingRunTimeBaseline = loadAndRunCoroutines(NULL);

  // Get timing for threading with concurrent threads.
  cnd_t threadCondition;
  cnd_init(&threadCondition);
  mtx_t threadMutex;
  mtx_init(&threadMutex, mtx_plain);
  LoadAndRunCoroutinesArgs threadMutexAndCondition;
  threadMutexAndCondition.threadCondition = &threadCondition;
  threadMutexAndCondition.threadMutex = &threadMutex;

  // Start three threads, each of which will run the set of coroutines.
  thrd_t threads[3];
  if (thrd_create(&threads[0], loadAndRunCoroutines, &threadMutexAndCondition)
    != thrd_success
  ) {
    fprintf(stderr, "Could not initialize thread 0.\n");
    return 1;
  }
  if (thrd_create(&threads[1], loadAndRunCoroutines, &threadMutexAndCondition)
    != thrd_success
  ) {
    fprintf(stderr, "Could not initialize thread 1.\n");
    return 1;
  }
  if (thrd_create(&threads[2], loadAndRunCoroutines, &threadMutexAndCondition)
    != thrd_success
  ) {
    fprintf(stderr, "Could not initialize thread 2.\n");
    return 1;
  }
  // Small delay to make sure the threads start and get to their waits.
  for (volatile unsigned int i = 10000000; i > 0; i--) {
    (void) i;
  }
  printf("Threads created.  Signaling start.\n");
  cnd_broadcast(&threadCondition);

  // Collect the duration from each thread and average the values.
  int threadRunTimes[3];
  if (thrd_join(threads[0], &threadRunTimes[0]) != thrd_success) {
    fprintf(stderr, "Could not join thread 0.\n");
  }
  printf("Thread 0 complete.  Waiting for thread 1.\n");
  if (thrd_join(threads[1], &threadRunTimes[1]) != thrd_success) {
    fprintf(stderr, "Could not join thread 1.\n");
  }
  printf("Thread 1 complete.  Waiting for thread 2.\n");
  if (thrd_join(threads[2], &threadRunTimes[2]) != thrd_success) {
    fprintf(stderr, "Could not join thread 2.\n");
  }
  printf("Thread 2 complete.  Computing average runtime.\n\n");
  int multithreadedRunTime
    = (threadRunTimes[0] + threadRunTimes[1] + threadRunTimes[2]) / 3;
#endif

  printf(
    "Scheduled tasks completed in %u.%06u seconds without threading.\n",
    (unsigned int) noThreadingRunTime / 1000000,
    (unsigned int) noThreadingRunTime % 1000000);

#ifdef THREAD_SAFE_COROUTINES
  printf(
    "Scheduled tasks completed in %u.%06u seconds with threading.\n",
    (unsigned int) threadingRunTimeBaseline / 1000000,
    (unsigned int) threadingRunTimeBaseline % 1000000);
  int64_t deltaFromBaseline = ((int64_t) threadingRunTimeBaseline * 10000LL)
    / (int64_t) noThreadingRunTime;
  printf("* %u.%02u%% of non-threading baseline.\n",
    (unsigned int) (deltaFromBaseline / 100),
    (unsigned int) (deltaFromBaseline % 100));

  printf(
    "Scheduled tasks completed in an average of %u.%06u seconds with "
      "multithreading.\n",
    (unsigned int) multithreadedRunTime / 1000000,
    (unsigned int) multithreadedRunTime % 1000000);
  deltaFromBaseline = ((int64_t) multithreadedRunTime * 10000LL)
    / (int64_t) threadingRunTimeBaseline;
  printf("* %u.%02u%% of threading baseline.\n",
    (unsigned int) (deltaFromBaseline / 100),
    (unsigned int) (deltaFromBaseline % 100));
#endif

  return 0;
}

