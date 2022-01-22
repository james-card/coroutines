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
* Renamed previous "state" memeber of the coroutine object to "context" and added a CoroutineState to track the running state of the coroutine.
* Added thread safety to the libraries that is compile-time enabled but runtime-disabled by default.
  * Compile-time enablement can be disable by setting the `SINGLE_CORE_COROUTINES` define at compile time.

# Example Usage
An example of a simple set of coroutines in a round robin scheduler can be found in the examples directory.  This configuration will have each coroutine update the common integer twice before releasing the mutex that gates operation.  The example program provides metrics of performance for coroutines:
* Without thread safety
* With thread safety running in a single thread
* With thread safety running in multiple threads
The difference in performance between with and without thread safety is due to thread-safe coroutines having to make use of thread-specific storage rather than global variables.  The difference in performance between a single thread and multiple threads is due to resource contention among threads on the thread-specific storage lookups.
