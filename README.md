# Coroutines
This is a C coroutines library based on the work of [Tony Finch](http://www.dotat.at/cgi/git/picoro.git).  This approach segments the main stack into sub-stacks for the coroutines.  This library aims to provide a full-featured set of functionality for coroutines in pure C (no assembly or dependencies on third-party libraries).  Relative to what Tony originally wrote, the changes and extensions are as follows:
* Removed use of assert.  Functions now check for invalid parameters and have special return values for errors.
* Fixed non-ISO C compliant passing of function pointers as parameters to calls that take void pointers.
* Made `COROUTINE_STACK_SIZE` a compile-time define that defaults to 16 KB.
* Added ID property to coroutine object and accompanying setter/getter.
  * This is provided so that the coroutine can identify its place in an array of croutine-specific storage.
* Added coroutine mutexes.
* Added coroutine conditions.
* Added Doxygen comments.
* Added clarifying comments in some of the less-intuitive code.
* Provided a work-around for a bug in MSVC's 64-bit version of longjmp.
* Renamed previous "state" memeber of the coroutine object to "context" and added a CoroutineState to track the running state of the coroutine.
* Added thread safety to the libraries that is compile-time enabled but runtime-disabled by default.
  * Compile-time enablement can be disable by setting the `SINGLE_CORE_COROUTINES` define at compile time.
  * Disabling thread safety at compile time also eliminates the need for dynamic memory.

Really, coroutines are best suited for embedded systems, but this approach can be used within an individual process as well.  This implementation is provided for anyone who is looking for a full-featured C coroutines library.

# Example Usage
An example of a simple set of coroutines in a round robin scheduler can be found in the examples directory.  This configuration will have each coroutine update the common integer twice before releasing the mutex that gates operation.  The example program provides metrics of performance for coroutines:
* Without thread safety
* With thread safety running in a single thread
* With thread safety running in multiple threads

The output of the program is a good example of branch prediction.  The iterations get faster the longer the program runs (up to a point).  I had to throw away the results of the first run because of this.

# Performance Differences
The difference in performance between with and without thread safety is due to thread-safe coroutines having to make use of thread-specific storage rather than global variables.  The difference in performance between a single thread and multiple threads is due to resource contention among threads on the thread-specific storage lookups.

## Performance Differences as Measured in WSL
Windows Subsystem Linux uses pthreads for the thread-specific storage mechanisms.  There is a small overhead introduced by the ISO C threads wrapper I put around them in my cthreads library.
```
Scheduled tasks completed in 2.569028 seconds without threading.
Scheduled tasks completed in 2.588351 seconds with threading.
* 100.75% of non-threading baseline.
Scheduled tasks completed in an average of 7.599144 seconds with multithreading.
* 293.59% of threading baseline.
```

## Performance Differences as Measured in Windows Release Build
Visual Studio does not support ISO C threads and has very different mechanisms than pthreads for most things.  I implemented an ISO C threads library to get equivalent functionality.  The biggest difference between Windows and the ISO C threads standard is how thread-specific storage is supposed to work, which the coroutines library makes heavy use of.  In my implementation, I use red-black trees for the lookup mechanism, which may be suboptimal relative to whatever the implementation in pthreads is.
```
Scheduled tasks completed in 1.151290 seconds without threading.
Scheduled tasks completed in 1.347941 seconds with threading.
* 117.08% of non-threading baseline.
Scheduled tasks completed in an average of 4.444585 seconds with multithreading.
* 329.73% of threading baseline.
```

# Debugging
Good luck!  Segmenting the stack wreaks havoc with valgrind.  It will complain about uninitialized values all over the place.  I assure you that it's wrong in all the areas I've looked into.  The Visual Studio debugger does, however, update its stack correctly after a context switch, so it may be a better choice for debugging simple things than Linux/valgrind.
