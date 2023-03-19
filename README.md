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
  * The stack size must be set before the first coroutine is created on the current thread.
  * This size also defines the stack size for the main routine (the routine that calls coroutineCreate).
* Removed static default first Coroutine within the library.
* Added a mechanism to provide a first Coroutine context to operate from.

Really, coroutines are best suited for embedded systems, but this approach can be used within an individual process as well.  This implementation is provided for anyone who is looking for a full-featured C coroutines library.

# Example Usage
An example of a simple set of coroutines in a round robin scheduler can be found in the examples directory.  This configuration will have each coroutine update the common integer twice before releasing the mutex that gates operation.  The example program provides metrics of performance for coroutines:
* Without thread safety
* With thread safety running in a single thread
* With thread safety running in multiple threads

The output of the program is a good example of branch prediction.  The iterations get faster the longer the program runs (up to a point).  I had to throw away the results of the first run because of this.

# Configuring Coroutines
coroutineConfig must be called on the current thread before any other coroutine operations are performed.  coroutineConfig can configure the stack size of the coroutines and the Coroutine to use for the main Coroutine (the part of the program that calls coroutineCreate).  If a stack size of less than 1 KB is provided to coroutineConfig, the default stack size will be used.  Currently, this default is 16 KB.  If a NULL first Coroutine pointer is provided then the first Coroutine used by the library for the current thread will not be changed.  This is only permitted if a valid first Coroutine pointer has previously been specified.  Providing the first Coroutine to use from outside the library allows the library to operate without the need for dynamic memory.

coroutineConfig may be called successfully any number of times before the first coroutine is created on a thread but will fail thereafter.  Calling coroutineConfig after the first coroutine of the thread has been created will have no effect on any of the coroutine settings for that thread.

All coroutines (and the main routine that issues the coroutineCreate calls) have the same stack size.  This is due to the way the stack subdivision process works.  What actually happens on a call to coroutineCreate is that the current stack is extended by the specified size.  Allowing for the stack size to be changed during the lifetime of the thread would result in counterintuitive behavior that would be error prone.  Also, Coroutines are reused as they exit and become available again.  Allowing for changing stack sizes would prevent this reuse and add *A LOT* of complexity to the code.

# Coroutines and Multithreading
No calls to threading functions are made when threading support isn't enabled at runtime.  This is deliberate.  Threading calls are almost guaranteed to make use of dynamic memory, which is specifically avoided by this library.  Because of the technique used to subdivide the stack, it would be possible to implement a simple dynamic memory manager using this coroutines library and then enable threading at the system level.  Although the wisdom of doing this would be questionable, avoiding threading calls unless explicitly enabled to do so at runtime allows for this possibility.

If you do want to make use of coroutines in a multi-threaded environment, make sure you call `coroutineSetThreadingSupportEnabled(true);` from the main thread before calling coroutineConfig on each thread.  Failure to do so *WILL* result in segmentation faults since each thread will be modifying global parameters within the library rather than thread-specific parameters.

# Debugging
Good luck!  Segmenting the stack wreaks havoc with valgrind.  It will complain about uninitialized values all over the place becaue it considers any values on the stack above a return point to be uninitialized.  coroutineCreate sets up the stack and then returns to the calling function, which confuses valgrind like this.  The Visual Studio debugger does, however, update its stack correctly after a context switch, so it may be a better choice for debugging simple things than Linux/valgrind.  It would probably be best to debug coroutines as independent funcitons (that do not yield) first before trying to use them as coroutines.

If you're getting segmentation faults when attempting to resume or when returning from a yield, you probably need to increase your stack size.  The Coroutine objects and their associated contexts are stored at the base of each stack segment.  If the segment below overflows the context will get corrupted and these calls are likely to crash.
