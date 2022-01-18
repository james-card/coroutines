# Coroutines
This is a C coroutines library based on the work of [Tony Finch](http://www.dotat.at/cgi/git/picoro.git).  It aims to provide a full-featured set of functionality for coroutines in pure C (no inline assembly or dependencies on any external libraries).  Relative to what Tony originally wrote, the changes and extensions are as follows:
* Removed use of assert.  Functions now check for invalid parameters and have special return values for errors.
* Fixed non-ISO C compliant passing of function pointers as parameters to calls that take void pointers.
* Added ID property to coroutine object and accompanying setter/getter.
* Added coroutine mutexes.
* Added coroutine conditions.
* Added Doxygen comments.
* Added clarifying comments in some of the less-intuitive code.
* Provided a work-around for a bug in MSVC's 64-bit version of longjmp.

# Example Usage
```C
#include <stdio.h>
#include <assert.h>
#include "Coroutines.h"

#define NUM_COROUTINES 3
Comutex comutex;
int coroutineStorage[NUM_COROUTINES] = { 0 };

void* func1(void *args) {
  int *intArg = (int*) args;
  int localInt = *intArg;
  bool mutexLocked = false;
  coroutineStorage[coroutineId(NULL)] = 1;
  int localVariable = 1;
  while (1) {
    if (mutexLocked == false) {
      comutexLock(&comutex);
      mutexLocked = true;
    }
    assert(localVariable == 1);
    assert(coroutineStorage[coroutineId(NULL)] == 1);
    assert(mutexLocked == true);
    void* lastYieldValue = comutexLastYieldValue(&comutex);
    if (lastYieldValue != NULL) {
      intArg = (int*)lastYieldValue;
      localInt = *intArg;
    }
    localInt++;
    printf("%s:  %d\n", __func__, localInt);
    if ((localInt & 1) == 0) {
      if (comutexUnlock(&comutex) == coroutineSuccess) {
        mutexLocked = false;
      } else {
          fprintf(stderr, "Attempt to unlock comutex failed.\n");
      }
    }
    intArg = (int*)coroutineYield(&localInt);
    localInt = *intArg;
  }

  return NULL;
}

void* func2(void *args) {
  int* intArg = (int*)args;
  int localInt = *intArg;
  bool mutexLocked = false;
  coroutineStorage[coroutineId(NULL)] = 2;
  int localVariable = 2;
  while (1) {
    if (mutexLocked == false) {
      comutexLock(&comutex);
      mutexLocked = true;
    }
    assert(localVariable == 2);
    assert(coroutineStorage[coroutineId(NULL)] == 2);
    assert(mutexLocked == true);
    void* lastYieldValue = comutexLastYieldValue(&comutex);
    if (lastYieldValue != NULL) {
      intArg = (int*)lastYieldValue;
      localInt = *intArg;
    }
    localInt++;
    printf("%s:  %d\n", __func__, localInt);
    if ((localInt & 1) == 0) {
      if (comutexUnlock(&comutex) == coroutineSuccess) {
        mutexLocked = false;
      }
      else {
        fprintf(stderr, "Attempt to unlock comutex failed.\n");
      }
    }
    intArg = (int*)coroutineYield(&localInt);
    localInt = *intArg;
  }

  return NULL;
}

void* func3(void* args) {
  int* intArg = (int*)args;
  int localInt = *intArg;
  bool mutexLocked = false;
  coroutineStorage[coroutineId(NULL)] = 3;
  int localVariable = 3;
  while (1) {
    if (mutexLocked == false) {
      comutexLock(&comutex);
      mutexLocked = true;
    }
    assert(localVariable == 3);
    assert(coroutineStorage[coroutineId(NULL)] == 3);
    assert(mutexLocked == true);
    void* lastYieldValue = comutexLastYieldValue(&comutex);
    if (lastYieldValue != NULL) {
      intArg = (int*)lastYieldValue;
      localInt = *intArg;
    }
    localInt++;
    printf("%s:  %d\n", __func__, localInt);
    if ((localInt & 1) == 0) {
      if (comutexUnlock(&comutex) == coroutineSuccess) {
        mutexLocked = false;
      }
      else {
        fprintf(stderr, "Attempt to unlock comutex failed.\n");
      }
    }
    intArg = (int*)coroutineYield(&localInt);
    localInt = *intArg;
  }

  return NULL;
}

Coroutine *coroArray[NUM_COROUTINES];

int schedule(void *arg) {
  int* initialArg = (int*)arg;
  void* status = NULL;
  int numCoroutines = sizeof(coroArray) / sizeof(Coroutine*);
  int coroIndex = 0;

  while (1) {
    status = coroutineResume(coroArray[coroIndex], arg);
    if (status == COROUTINE_NOT_RESUMABLE) {
      // In a real scheduler, this would be an indication to remove the routine
      // from the list, but in our simple case, it's an error.
      fprintf(stderr, "Coroutine %d is not resumable.\n", coroIndex);
      return 1;
    }
    assert(*initialArg == 42);
    if (status != COROUTINE_BLOCKED) {
      arg = status;
    }
    coroIndex++;
    if (coroIndex == numCoroutines) {
      coroIndex = 0;
    }
  }

  return 0;
}

int main(int argc, char **argv) {
  (void) argc;
  (void) argv;
  int arg = 42;

  if (comutexInit(&comutex, comutexPlain) != coroutineSuccess) {
    fprintf(stderr, "Could not initialize comutex.\n");
    return 1;
  }

  coroArray[0] = coroutineCreate(func1);
  if (coroArray[0] == NULL) {
    fprintf(stderr, "Could not initialize coroutine 0.\n");
    return 1;
  }
  assert(arg == 42);
  coroutineSetId(coroArray[0], 0);

  coroArray[1] = coroutineCreate(func2);
  if (coroArray[1] == NULL) {
    fprintf(stderr, "Could not initialize coroutine 1.\n");
    return 1;
  }
  assert(arg == 42);
  coroutineSetId(coroArray[1], 1);

  coroArray[2] = coroutineCreate(func3);
  if (coroArray[2] == NULL) {
    fprintf(stderr, "Could not initialize coroutine 2.\n");
    return 1;
  }
  assert(arg == 42);
  coroutineSetId(coroArray[2], 2);

  schedule(&arg);

  return 0;
}

```
