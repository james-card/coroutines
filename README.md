# Coroutines
This is a C coroutines library based on the work of [Tony Finch](http://www.dotat.at/cgi/git/picoro.git).  This approach does not use dynamic memory.  It segments the main stack into sub-stacks for the coroutines.  This library aims to provide a full-featured set of functionality for coroutines in pure C (no assembly or dependencies on third-party libraries).  Relative to what Tony originally wrote, the changes and extensions are as follows:
* Removed use of assert.  Functions now check for invalid parameters and have special return values for errors.
* Fixed non-ISO C compliant passing of function pointers as parameters to calls that take void pointers.
* Added ID property to coroutine object and accompanying setter/getter.
  * This is provided so that the coroutine can identify its place in an array of croutine-specific storage.
* Added coroutine mutexes.
* Added coroutine conditions.
* Added Doxygen comments.
* Added clarifying comments in some of the less-intuitive code.
* Provided a work-around for a bug in MSVC's 64-bit version of longjmp.
* Renamed previous "state" memeber of the coroutine object to "context" and added a CoroutineState to track the running state of the coroutine.
* Added thread safety to the libraries that is compile-time supported but runtime-disabled by default.
  * Compile-time support can be disabled by setting the `SINGLE_CORE_COROUTINES` define at compile time.
* Added a mechanism for the coroutine stack size to be set at runtime.
  * The default size is 16 KB.
  * The stack size is set in a multiple of 1 KB.
  * The minimum stack size is 1 KB irrespective of the size specified.
  * The stack size must be set before the first coroutine is created on the current thread.
  * This size also defines the stack size for the main routine (the routine that calls coroutineCreate).

Really, coroutines are best suited for embedded systems, but this approach can be used within an individual process as well.  This implementation is provided for anyone who is looking for a full-featured C coroutines library.

# Example Usage
An example of a simple set of coroutines in a round robin scheduler can be found in the examples directory.  This configuration will have each coroutine update the common integer twice before releasing the mutex that gates operation.  The example program provides metrics of performance for coroutines:
* Without thread safety
* With thread safety running in a single thread
* With thread safety running in multiple threads

The output of the program is a good example of branch prediction.  The iterations get faster the longer the program runs (up to a point).  I had to throw away the results of the first run because of this.

# Coroutines and Multithreading
No calls to threading functions are made when threading support isn't enabled at runtime.  This is deliberate.  Threading calls are almost guaranteed to make use of dynamic memory, which is specifically avoided by this library.  Because of the technique used to subdivide the stack, it would be possible to implement a simple dynamic memory manager using this coroutines library and then enable threading at the system level.  Although the wisdom of doing this would be questionable, avoiding threading calls unless explicitly enabled to do so at runtime allows for this possibility.

## Configuring Coroutines on Threads
Calling coroutineConfig is optional on the main thread.  If it is not called before the first coroutine is created on the main thread, a default first Coroutine context will be used with a stack size of 16 KB.  For all other threads, it is *MANDATORY* to call coroutineConfig before making any other coroutine calls.  A pointer to a valid Coroutine object must be provided.  This allows for the complete absence of any dynamic memory allocation in the Coroutines library.

coroutineConfig may be called successfully any number of times before the first coroutine is created on a thread but will fail thereafter.  Calling coroutineConfig after the first coroutine of the thread has been created will have no effect on any of the coroutine settings for that thread.

# Debugging
Good luck!  Segmenting the stack wreaks havoc with valgrind.  It will complain about uninitialized values all over the place.  I assure you that it's wrong in all the areas I've looked into.  The Visual Studio debugger does, however, update its stack correctly after a context switch, so it may be a better choice for debugging simple things than Linux/valgrind.
