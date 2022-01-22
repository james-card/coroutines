#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <stdint.h>

#include "Coroutines.h"
#ifdef THREADSAFE_COROUTINES
#include "CThreads.h"
#endif

#define NUM_COROUTINES 3
int globalInt = 0;

uint64_t getElapsedMicroseconds(uint64_t previousTime) {
  struct timespec now;
  timespec_get(&now, TIME_UTC);
  uint64_t nowMicroseconds = ((uint64_t) now.tv_sec * 1000000ULL) + ((uint64_t) now.tv_nsec / 1000ULL);
  return nowMicroseconds - previousTime;
}

typedef struct CoroutineArgs {
  Comutex *comutex;
  Cocondition *cocondition;
  int functionNumber;
  int *globalInt;
  int *coroutineStorage;
} CoroutineArgs;

void* func(void *args) {
  CoroutineArgs *coroutineArgs = (CoroutineArgs*) args;
  int functionNumber = coroutineArgs->functionNumber;
  int *globalInt = coroutineArgs->globalInt;
  Comutex *comutex = coroutineArgs->comutex;
  Cocondition *cocondition = coroutineArgs->cocondition;
  int *coroutineStorage = coroutineArgs->coroutineStorage;
  bool mutexLocked = false;
  coroutineStorage[coroutineId(NULL)] = functionNumber;

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
      // Update our function number.
      coroutineArgs = (CoroutineArgs*)lastYieldValue;
      functionNumber = coroutineArgs->functionNumber;
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

int coroutineRoundRobin(Coroutine *coroutineArray[], int numCoroutines) {
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

typedef struct ThreadMutexAndCondition {
#ifdef THREADSAFE_COROUTINES
  cnd_t* threadCondition;
  mtx_t* threadMutex;
#else
  int dummyArg;
#endif
} ThreadMutexAndCondition;

int loadAndRunCoroutines(void *args) {
#ifdef THREADSAFE_COROUTINES
    ThreadMutexAndCondition *mutexAndCondition
        = (ThreadMutexAndCondition*) args;
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

    Coroutine *coroutineArray[NUM_COROUTINES];
    int coroutineStorage[NUM_COROUTINES] = {0};

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

    coroutineArray[0] = coroutineCreate(func);
    if (coroutineArray[0] == NULL) {
        fprintf(stderr, "Could not initialize coroutine 0.\n");
        return 1;
    }
    coroutineSetId(coroutineArray[0], 0);
    coroutineArgs.functionNumber = 1;
    coroutineResume(coroutineArray[0], &coroutineArgs);

    coroutineArray[1] = coroutineCreate(func);
    if (coroutineArray[1] == NULL) {
        fprintf(stderr, "Could not initialize coroutine 1.\n");
        return 1;
    }
    coroutineSetId(coroutineArray[1], 1);
    coroutineArgs.functionNumber = 2;
    coroutineResume(coroutineArray[1], &coroutineArgs);

    coroutineArray[2] = coroutineCreate(func);
    if (coroutineArray[2] == NULL) {
        fprintf(stderr, "Could not initialize coroutine 2.\n");
        return 1;
    }
    coroutineSetId(coroutineArray[2], 2);
    coroutineArgs.functionNumber = 3;
    coroutineResume(coroutineArray[2], &coroutineArgs);

    coconditionBroadcast(&cocondition);

    uint64_t startTime = getElapsedMicroseconds(0);
    int status = coroutineRoundRobin(coroutineArray, NUM_COROUTINES);
    if (status != 0) {
        fprintf(stderr,
            "Scheduled coroutines completed with one or more errors.\n");
    }
    uint64_t runTime = getElapsedMicroseconds(startTime);

    return (int) runTime;
}

int main(int argc, char **argv) {
  (void) argc;
  (void) argv;

  // Have to run this once and throw away the time to prime branch prediction.
  loadAndRunCoroutines(NULL);

  // Coroutine threading support is disabled by default.
  globalInt = 0;
  int noThreadingRunTime = loadAndRunCoroutines(NULL);

#ifdef THREADSAFE_COROUTINES
  // Get baseline with threading enabled but no concurrent threads.
  coroutineSetThreadingSupportEnabled(true);
  globalInt = 0;
  int threadingRunTimeBaseline = loadAndRunCoroutines(NULL);

  // Get timing for threading with concurrent threads.
  cnd_t threadCondition;
  cnd_init(&threadCondition);
  mtx_t threadMutex;
  mtx_init(&threadMutex, mtx_plain);
  ThreadMutexAndCondition threadMutexAndCondition;
  threadMutexAndCondition.threadCondition = &threadCondition;
  threadMutexAndCondition.threadMutex = &threadMutex;

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

#ifdef THREADSAFE_COROUTINES
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

