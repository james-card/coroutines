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

#include "CThreadsMessages.h"
#include "RadixTree.h"

#if LOGGING_ENABLED && CTHREADS_LOGGING_ENABLED
#include "LoggingLib.h"
#else
#undef printLog
#define printLog(...) {}
#define logFile stderr
#define LOG_MALLOC_FAILURE(...) {}
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct thrd_msg_q_t thrd_msg_q_t;
void thrd_msg_q_storage_init(void);
int thrd_msg_q_create(void);
int thrd_msg_q_destroy(thrd_msg_q_t *queue);

#ifdef __cplusplus
}
#endif

/// @struct thrd_msg_q_t
///
/// @brief Definition for a thread's message queue.
///
/// @param head The head of the message queue.  Messages will be removed from
///   this pointer.
/// @param tail The tail of the message queue.  Messages will be added to this
///   pointer.
/// @param condition A condition (cnd_t) that will allow for signalling between
///   threads when adding a message to the queue.
/// @param lock A mutex (mtx_t) to guard the condition.
typedef struct thrd_msg_q_t {
  thrd_msg_t *head;
  thrd_msg_t *tail;
  cnd_t condition;
  mtx_t lock;
} thrd_msg_q_t;

/// @var thrd_msg_q_storage_initialized
///
/// @brief once_flag to keep track of whether or not the root storage for thread
/// message queues has been created.
once_flag thrd_msg_q_storage_initialized = ONCE_FLAG_INIT;

/// @var message_queues
///
/// @brief Root storage for the message queues.  Keys are thrd_t values.  Values
/// are thrd_msg_q_t values.  There is one thrd_msg_q_t in the tree for ever thread that
/// is currently running.  Queues are created when a thread starts and destroyed
/// when it exits.
static RadixTree *message_queues = NULL;

// Message queue functions

/// @fn thrd_msg_q_t* get_thread_thrd_msg_q(void)
///
/// @brief Get the message queue for the current theread.
///
/// @return Returns a pointer to the thrd_msg_q_t for the thread making this call.
/// This should always be a non-NULL value unless something is wrong with the
/// system.
static inline thrd_msg_q_t* get_thread_thrd_msg_q(void) {
  thrd_t thr = thrd_current();
  return (thrd_msg_q_t*) radixTreeGetValue(message_queues, &thr, sizeof(thr));
}

/// @fn void thrd_msg_q_storage_init(void)
///
/// @brief call_once function that initializes the message_queues radix tree
/// storage that will be used to hold the queues for each thread.
///
/// @return This function returns no value.
void thrd_msg_q_storage_init(void) {
  message_queues = radixTreeCreate(NULL);
  if (message_queues == NULL) {
    // No tree.  Can't proceed.
    LOG_MALLOC_FAILURE();
    exit(1);
  }
  
  return;
}

/// @fn int thrd_msg_q_create(void)
///
/// @brief Dynamically allocate and initialize a thrd_msg_q_t that will server as the
/// message queue for the current thread and associate it with this thread's
/// thrd_t value in the message_queues radix tree.
///
/// @return Returns thrd_success on success, thrd_error on failure.
int thrd_msg_q_create(void) {
  int return_value = thrd_success;
  thrd_msg_q_t *old_queue = NULL;
  
  if (get_thread_thrd_msg_q() != NULL) {
    // This should be impossible, but already have a queue for this thread.
    // Nothing to do.
    return return_value; // thrd_success
  }
  
  thrd_t thr = thrd_current();
  thrd_msg_q_t *new_queue = (thrd_msg_q_t*) calloc(1, sizeof(thrd_msg_q_t));
  if (new_queue == NULL) {
    goto new_queue_calloc_failure;
  }
  
  // new_queue->head and new_queue->tail are initialized to NULL by the
  // calloc call.
  if (cnd_init(&new_queue->condition) != thrd_success) {
    // Can't proceed.  Bail.
    goto cnd_init_failure;
  }
  
  if (mtx_init(&new_queue->lock, mtx_plain | mtx_timed)
    != thrd_success
  ) {
    // Can't proceed.  Bail.
    goto mtx_init_failure;
  }
  
  old_queue =
    (thrd_msg_q_t*) radixTreeSetValue(message_queues, &thr, sizeof(thr), new_queue);
  if (old_queue != NULL) {
    // This shouldn't be possible, but we have to destroy the old queue.
    thrd_msg_q_destroy(old_queue);
  }
  
  return return_value; // thrd_success
  
mtx_init_failure:
  cnd_destroy(&new_queue->condition);
cnd_init_failure:
  free(new_queue); new_queue = NULL;
new_queue_calloc_failure:
  return_value = thrd_error;
  
  return return_value; // thrd_error
}

/// @fn int thrd_msg_q_destroy(thrd_msg_q_t *queue)
///
/// @brief Destroy the specified message queue.  If none is specifiied, remove
/// the queue for the current thread and destroy it.
///
/// @param queue The queue to destroy.  If this value is NULL, the message queue
/// for the current thread will be destroyed.
///
/// @return Returns thrd_success on success, thrd_error on failure.
int thrd_msg_q_destroy(thrd_msg_q_t *queue) {
  int return_value = thrd_success;
  thrd_t thr = thrd_current();
  
  // radixTreeSetValue will do an atomic swap to keep any other thread from
  // trying to push messages onto the queue while we're in the middle of
  // deleting it.
  if (queue == NULL) {
    queue =
      (thrd_msg_q_t*) radixTreeSetValue(message_queues, &thr, sizeof(thr), NULL);
    
    // Delete the node storage for the queue in the radix tree.
    radixTreeDeleteValue(message_queues, &thr, sizeof(thr));
    
    if (queue == NULL) {
      // This shouldn't be possible, but there's no queue for this thread.
      // Nothing to do.
      return return_value; // thrd_success
    }
  }
  
  for (thrd_msg_t *cur = queue->head; cur != NULL; ) {
    thrd_msg_t *next = cur->next;
    thrd_msg_destroy(cur);
    cur = next;
  }
  
  mtx_destroy(&queue->lock);
  cnd_destroy(&queue->condition);
  free(queue); queue = NULL;
  
  return return_value;
}

/// @fn thrd_msg_t* thrd_msg_q_peek(void)
///
/// @brief Get the head of the current thread's message queue but do not remove
/// it from the queue.
///
/// @return Returns the head of the message queue, which may be NULL.
thrd_msg_t* thrd_msg_q_peek(void) {
  thrd_msg_t *head = NULL;
  
  thrd_msg_q_t *queue = get_thread_thrd_msg_q();
  if (queue != NULL) {
    head = queue->head;
  }
  
  return head;
}

/// @fn thrd_msg_t* thrd_msg_q_peek(void)
///
/// @brief Get the head of the current thread's message queue and remove it.
///
/// @return Returns the head of the message queue, which may be NULL.
thrd_msg_t* thrd_msg_q_pop(void) {
  thrd_msg_q_t *queue = get_thread_thrd_msg_q();
  thrd_msg_t *head = NULL;
  
  if ((queue == NULL) || (mtx_lock(&queue->lock) != thrd_success)) {
    // Error case.
    return head; // NULL
  }
  
  head = queue->head;
  if (head != NULL) {
    queue->head = head->next;
    head->next = NULL;
    
    if (queue->head == NULL) {
      // Empty queue.  Set queue->tail to NULL too.
      queue->tail = NULL;
    }
  }
  
  mtx_unlock(&queue->lock);
  
  return head;
}

/// @fn thrd_msg_t* thrd_msg_q_pop_type(int type)
///
/// @brief Get the first message of the specified type from the current
/// thread's message queue and remove it from the queue.
///
/// @param type The message type to look for.
///
/// @return Returns the first message of the queue with the specified type on
/// success, NULL on failure.
thrd_msg_t* thrd_msg_q_pop_type(int type) {
  thrd_msg_q_t *queue = get_thread_thrd_msg_q();
  thrd_msg_t *return_value = NULL;
  thrd_msg_t *prev = NULL;
  thrd_msg_t *cur = queue->head;
  thrd_msg_t **prev_next = &queue->head;
  
  if (mtx_lock(&queue->lock) != thrd_success) {
    // Error case.
    return return_value; // NULL
  }
  
  while ((cur != NULL) && (cur->type != type)) {
    prev = cur;
    prev_next = &cur->next;
    cur = cur->next;
  }
  
  if (cur != NULL) {
    // Desired type was found.  Remove the message from the queue.
    return_value = cur;
    *prev_next = cur->next;
    
    if (queue->head == NULL) {
      // Empty queue.  Set queue->tail to NULL too.
      queue->tail = NULL;
    }
    if (queue->tail == cur) {
      queue->tail = prev;
    }
    cur->next = NULL;
  }
  
  mtx_unlock(&queue->lock);
  
  return return_value;
}

/// @fn thrd_msg_t* thrd_msg_q_wait_for_type_(int *type, const struct timespec *ts)
///
/// @brief Wait for a message of a given type to be available in the message
/// queue or until a specified time has elapsed.  Remove the message from the
/// queue and return it if one is available before the specified time is
/// reached.
///
/// @param type A pointer to the message type to look for.  If this parameter is
///   NULL then the first message of any type will be returned.
/// @param ts A pointer to a struct timespec that specifies the end of the time
///   period to wait for.  If this parameter is NULL then an infinite timeout
///   will be used.
///
/// @return Returns the first message of the provided type if one is available
/// before the specified time.  Returns NULL if no such message is available
/// within that time period or if an error occurrs.
thrd_msg_t* thrd_msg_q_wait_for_type_(int *type, const struct timespec *ts) {
  thrd_msg_q_t *queue = get_thread_thrd_msg_q();
  thrd_msg_t *return_value = NULL;
  thrd_msg_t *prev = NULL;
  thrd_msg_t *cur = queue->head;
  thrd_msg_t **prev_next = &queue->head;
  int lock_status = thrd_success;
  int wait_status = thrd_success;
  int search_type = 0;
  if (type != NULL) {
    // This saves us from having to dereference the pointer on each iteration
    // of the loop below.
    search_type = *type;
  }
  
  if (ts == NULL) {
    lock_status = mtx_lock(&queue->lock);
  } else {
    lock_status = mtx_timedlock(&queue->lock, ts);
  }
  if (lock_status != thrd_success) {
    // Error case.
    return return_value; // NULL
  }
  // mtx_timedlock will return thrd_timedout if the timeout is reached, so we'll
  // never reach this point if we've exceeded our timeout.
  
  while (return_value == NULL) {
    while ((cur != NULL) && (type != NULL) && (cur->type != search_type)) {
      prev = cur;
      prev_next = &cur->next;
      cur = cur->next;
    }
    
    if (cur != NULL) {
      // Desired type was found.  Remove the message from the queue.
      return_value = cur;
      *prev_next = cur->next;
      
      if (queue->head == NULL) {
        // Empty queue.  Set queue->tail to NULL too.
        queue->tail = NULL;
      }
      if (queue->tail == cur) {
        queue->tail = prev;
      }
      cur->next = NULL;
    } else {
      // Desired type was not found.  Block until something else is pushed.
      if (ts == NULL) {
        wait_status = cnd_wait(&queue->condition, &queue->lock);
      } else {
        wait_status = cnd_timedwait(&queue->condition, &queue->lock, ts);
      }
      if (wait_status != thrd_success) {
        break;
      }
      // cnd_timedwait will return thrd_timedout if the timeout is reached, so
      // we won't continue the loop if we've exceeded our timeout.
    }
    
    prev = NULL;
    cur = queue->head;
    prev_next = &queue->head;
  }
  
  mtx_unlock(&queue->lock);
  
  return return_value;
}

/// @fn thrd_msg_t* thrd_msg_q_wait(const struct timespec *ts)
///
/// @brief Wait until there is a message in the queue or until a specified has
/// been reached.  If there is a message in the queue before the timeout, pop
/// it from the queue.
///
/// @param ts A pointer to a struct timespec that specifies the end of the time
///   period to wait for.  If this parameter is NULL then an infinite timeout
///   will be used.
///
/// @return Returns the first message in the queue if one is available before
/// the specified time.  Returns NULL if no such message is available within
/// that time period or if an error occurrs.
thrd_msg_t* thrd_msg_q_wait(const struct timespec *ts) {
  return thrd_msg_q_wait_for_type_(NULL, ts);
}

/// @fn thrd_msg_t* thrd_msg_q_wait_for_type(int type, const struct timespec *ts)
///
/// @brief Wait until there is a message of a specified type in the queue or
/// until a specified has been reached.  If there is a message of the specified
/// type in the queue before the timeout, pop it from the queue.
///
///
/// @param type The message type to look for.
/// @param ts A pointer to a struct timespec that specifies the end of the time
///   period to wait for.  If this parameter is NULL then an infinite timeout
///   will be used.
///
/// @return Returns the first message of the provided type if one is available
/// before the specified time.  Returns NULL if no such message is available
/// within that time period or if an error occurrs.
thrd_msg_t* thrd_msg_q_wait_for_type(int type, const struct timespec *ts) {
  return thrd_msg_q_wait_for_type_(&type, ts);
}


/// @fn int thrd_msg_q_push(thrd_t thr, thrd_msg_t *msg)
///
/// @brief Push a message onto the message queue of a specified thread.
///
/// @param thr The thrd_t that identifies the destination thread.
/// @param msg A pointer to the thrd_msg_t to push onto the destination thread's
/// message queue.
///
/// @return Returns thrd_success on success, thrd_error on failure.
int thrd_msg_q_push(thrd_t thr, thrd_msg_t *msg) {
  int return_value = thrd_error;
  
  if (msg == NULL) {
    // Invalid.
    return return_value; // thrd_error
  }
  
  thrd_msg_q_t *queue
    = (thrd_msg_q_t*) radixTreeGetValue(message_queues, &thr, sizeof(thr));
  if (queue == NULL) {
    // Destination thread has exited.  Fail.
    return return_value; // thrd_error
  }
  
  if (mtx_lock(&queue->lock) != thrd_success) {
    // Error case.
    return return_value; // thrd_error
  }
  
  msg->from = thrd_current();
  msg->to = thr;
  msg->next = NULL;
  if (queue->tail != NULL) {
    queue->tail->next = msg;
    queue->tail = msg;
  } else {
    // Empty queue.  Populate both queue->head and queue->tail.
    queue->head = msg;
    queue->tail = msg;
  }
  
  // Let all the waiters know that there's something new in the queue now.
  return_value = cnd_broadcast(&queue->condition);
  
  mtx_unlock(&queue->lock);
  
  return return_value;
}

// Message functions

/// @fn int thrd_msg_start_use(thrd_msg_t *msg)
///
/// @brief Set all the member elements of a thrd_msg_t to their default values so
/// that we can begin using it.
///
/// @parm msg A pointer to the thrd_msg_t to configure and begin using.
///
/// @return Returns thrd_success on success, thrd_error on error.
static inline int thrd_msg_start_use(thrd_msg_t *msg) {
  int return_value = thrd_success;
  
  if (msg != NULL) {
    if (msg->in_use == false) {
      msg->type = 0;
      msg->data = NULL;
      msg->size = 0;
      msg->next = NULL;
      msg->waiting = false;
      msg->done = true;
      msg->in_use = true;
      msg->from = 0;
      if (msg->configured == false) {
        memset(&msg->condition, 0, sizeof(msg->condition));
        memset(&msg->lock, 0, sizeof(msg->condition));
        if (cnd_init(&msg->condition) == thrd_success) {
          if (mtx_init(&msg->lock, mtx_plain | mtx_timed) == thrd_success) {
            msg->configured = true;
          } else {
            cnd_destroy(&msg->condition);
            return_value = thrd_error;
            // msg->configured remains false
          }
        } else {
          return_value = thrd_error;
          // msg->configured remains false
        }
      }
      // Don't touch msg->dynamically_allocated;
    } // Else this message is already setup
  } else {
    return_value = thrd_error;
  }
  
  return return_value;
}

/// @fn thrd_msg_t* thrd_msg_create(void)
///
/// @brief Dynamically allocate a new thrd_msg_t and set all its member elements to
/// a default state.
///
/// @return Returns a pointer to a newly-allocated and configured thrd_msg_t on
/// success, NULL on failure.
thrd_msg_t* thrd_msg_create(void) {
  thrd_msg_t *msg = (thrd_msg_t*) calloc(1, sizeof(thrd_msg_t));
  if (thrd_msg_start_use(msg) == thrd_success) {
    msg->dynamically_allocated = true;
  } else {
    // Some kind of problem.  Can't use this message.
    free(msg); msg = NULL;
  }
  
  return msg;
}

/// @fn thrd_msg_t* thrd_msg_destroy(thrd_msg_t *msg)
///
/// @brief If nothing is waiting on the message, deallocate all of the message's
/// resources and return NULL.  If something is waiting on the message, mark the
/// message as done and no longer in use and broadcast to all the waiters.  Note
/// that the thrd_msg_t itself will only be freed if it was allocated by thrd_msg_create.
///
/// @param msg A pointer to the thrd_msg_t to destroy.
///
/// @return Returns NULL if the thrd_msg_t was fully deallocated, a pointer to the
/// original thrd_msg_t otherwise.
thrd_msg_t* thrd_msg_destroy(thrd_msg_t *msg) {
  if (msg == NULL) {
    // Nothing to do.
    return msg;
  }
  
  // Don't touch msg->type.
  // Don't touch msg->data.
  // Don't touch msg->size.
  // Don't touch msg->next.
  // Don't touch msg->waiting.
  msg->in_use = false;
  // Don't touch from.
  if (msg->configured == true) {
    if (mtx_trylock(&msg->lock) == thrd_success) {
      msg->done = true;
    
      if (msg->waiting == false) {
        // Nothing is waiting.  Destroy the resources.
        mtx_unlock(&msg->lock);
        cnd_destroy(&msg->condition);
        mtx_destroy(&msg->lock);
        msg->configured = false;
        if (msg->dynamically_allocated == true) {
          free(msg); msg = NULL;
        }
      } else {
        // Something is waiting.  Signal the waiters.  It will be up to them to
        // destroy this message again later.
        cnd_broadcast(&msg->condition);
        mtx_unlock(&msg->lock);
      }
    } else {
      // We can't do any signalling.  Just tear down everything.
      msg->done = true;
      msg->waiting = false;
      cnd_destroy(&msg->condition);
      mtx_destroy(&msg->lock);
      msg->configured = false;
      if (msg->dynamically_allocated == true) {
        free(msg); msg = NULL;
      }
    }
  } else if (msg->dynamically_allocated == true) {
    // msg->lock and mtx->condition are not initialized, so notihing to destroy.
    free(msg); msg = NULL;
  }
  
  return msg;
}

/// @fn int thrd_msg_init(
///   thrd_msg_t *msg, int type, void *data, size_t size, bool waiting)
///
/// @brief Initialize a message to make it ready for sending to another thread.
///
/// @param msg A pointer to an allocated thrd_msg_t.  Will be properly configured if
///   not already so.
/// @param type The message type to use for the message.
/// @param data A pointer to the data for the message (if any).
/// @param size The number of bytes pointed to by the data pointer.
/// @param waiting Whether or not the caller of this function will be waiting on
///   a response to this message from the destination thread.
///
/// @return Returns thrd_success on success, thrd_error on failure.
int thrd_msg_init(
  thrd_msg_t *msg, int type, void *data, size_t size, bool waiting
) {
  int return_value = thrd_error;
  
  if (msg == NULL) {
    // Nothing we can do.  Fail.
    return return_value; // thrd_error
  } else if (thrd_msg_start_use(msg) != thrd_success) {
    // Couldn't configure this message for use for some reason.  Fail.
    return return_value; // thrd_error
  }
  
  msg->type = type;
  msg->data = data;
  msg->size = size;
  msg->next = NULL;
  msg->waiting = waiting;
  msg->done = false;
  // No need to set msg->in_use since we called thrd_msg_start_use above.
  // Don't touch msg->from in case this message is being reused.
  return_value = thrd_success;
  
  return return_value;
}

/// @fn int thrd_msg_release(thrd_msg_t *msg)
///
/// @brief Release a message from use.
///
/// @param msg A pointer to a previously-allocated thrd_msg_t.
///
/// @return Returns thrd_success on success, thrd_error on failure.
int thrd_msg_release(thrd_msg_t *msg) {
  int return_value = thrd_success;
  
  if (msg == NULL) {
    // A NULL message is already released.  Just return.
    return return_value; // thrd_success
  }
  
  // Don't touch msg->type.
  // Don't touch msg->data.
  // Don't touch msg->size.
  // Don't touch msg->next.
  // Don't touch msg->waiting.
  msg->in_use = false;
  // Don't touch msg->from.
  if (msg->configured == true) {
    if (mtx_trylock(&msg->lock) == thrd_success) {
      msg->done = true;
      
      if (msg->waiting == true) {
        // Something is waiting.  Signal the waiters.  It will be up to them to
        // destroy this message again later.
        cnd_broadcast(&msg->condition);
      }
      mtx_unlock(&msg->lock);
    } else {
      // Something is wrong here.  We're releasing a thrd_msg_t that is not owned
      // by us.  We can't do a broadcast.  Just mark it done and return an
      // error.
      msg->done = true;
      return_value = thrd_error;
    }
  } else {
    // Nothing we can do but set the done flag.
    msg->done = true;
  }
  // Don't touch msg->condition.
  // Don't touch msg->lock.
  // Don't touch msg->configured.
  // Don't touch msg->dynamically_allocated.
  
  return return_value;
}

/// @fn int thrd_msg_set_done(thrd_msg_t *msg)
///
/// @brief Set the done flag for the message and signal any waiters.
///
/// @param msg A pointer to a previously-allocated thrd_msg_t.
///
/// @return Returns thrd_success on success, thrd_error on failure.
int thrd_msg_set_done(thrd_msg_t *msg) {
  int return_value = thrd_error;
  
  if (msg == NULL) {
    // Invalid.
    return return_value; // thrd_error
  }
  
  // Don't touch msg->type.
  // Don't touch msg->data.
  // Don't touch msg->size.
  // Don't touch msg->next.
  // Don't touch msg->waiting.
  // Don't touch msg->in_use.
  if (msg->configured == true) {
    mtx_lock(&msg->lock);
    msg->done = true;
    
    if (msg->waiting == true) {
      // Something is waiting.  Signal the waiters.  It will be up to them to
      // destroy this message again later.
      if (cnd_broadcast(&msg->condition) == thrd_success) {
        return_value = thrd_success;
      } // else return_value remains thrd_error.
    } else {
      return_value = thrd_success;
    }
    mtx_unlock(&msg->lock);
  } else {
    // Nothing we can do but set the done flag.
    msg->done = true;
    return_value = thrd_success;
  }
  // Don't touch msg->from.
  // Don't touch msg->condition.
  // Don't touch msg->lock.
  // Don't touch msg->configured.
  // Don't touch msg->dynamically_allocated.
  
  return return_value;
}

/// @fn int thrd_msg_wait_for_done(thrd_msg_t *msg, const struct timespec *ts)
///
/// @brief Wait on a message until another thread indicates that it's done
/// or until the specified time has been reached.
///
/// @param msg A pointer to a previously-allocated thrd_msg_t.
/// @param ts A pointer to a struct timespec that specifies the end of the time
///   period to wait for.  If this pointer is NULL, an infinite timeout will be
///   used.
///
/// @return Returns thrd_success on success, thrd_error on failure.
int thrd_msg_wait_for_done(thrd_msg_t *msg, const struct timespec *ts) {
  int return_value = thrd_error;
  
  if (msg == NULL) {
    // Invalid.
    return return_value; // thrd_error
  } else if (msg->configured == false) {
    // We can't do this.  Waiting for done requires the use of the lock and
    // condition in the message.  It doesn't make any sense for us to try and
    // initialize them at this point because whatever made this call is already
    // in a bad state.  We shouldn't try to fix things because we don't know
    // what's going on above us.  Just return bad status.
    return return_value; // thrd_error
  }
  
  int lock_status = thrd_success;
  int wait_status = thrd_success;
  if (msg->done == true) {
    return_value = thrd_success;
  } else {
    if (ts == NULL) {
      lock_status = mtx_lock(&msg->lock);
    } else {
      lock_status = mtx_timedlock(&msg->lock, ts);
    }
    if (lock_status != thrd_success) {
      // Either we timed out or there's a problem with the lock.  Either way, we
      // don't want to continue and we're going to exit with an error since we
      // never received the done flag.
      return return_value; // thrd_error
    }
    
    msg->waiting = true;
    while (msg->done == false) {
      if (ts == NULL) {
        wait_status = cnd_wait(&msg->condition, &msg->lock);
      } else {
        wait_status = cnd_timedwait(&msg->condition, &msg->lock, ts);
      }
      if (wait_status != thrd_success) {
        // Either we timed out or there's a problem with the condition.  Again,
        // we don't want to proceed like this.
        break;
      }
    }
    msg->waiting = false;
    
    if (msg->done == true) {
      return_value = thrd_success;
    }
    mtx_unlock(&msg->lock);
  }
  
  return return_value;
}

/// @fn thrd_msg_t* thrd_msg_wait_for_reply_with_type_(
///   thrd_msg_t *sent, bool release, int *type, const struct timespec *ts)
///
/// @brief Wait for a reply from the recipient of a message.
///
/// @param sent The message that was originally sent to the recipient.
/// @param release Whether or not the provided sent message should be released
///   (*NOT* destroyed) after the recipient has indicated that they're done
///   processing our sent message.
/// @param type A pointer to an integer type of message that the caller is
///   waiting for.  If this parameter is NULL, no type will be considered.
/// @param ts A pointer to a struct timespec that holds the end time to wait
///   until for a reply.  If this parameter is NULL, then an infinite timeout
///   will be used.
///
/// @return Returns a pointer to the thrd_msg_t received from the recipient of
/// the original message on success, NULL on failure or if the provided timeout
/// time is reached.
thrd_msg_t* thrd_msg_wait_for_reply_with_type_(
  thrd_msg_t *sent, bool release,
  int *type, const struct timespec *ts
) {
  thrd_msg_t *reply = NULL;

  thrd_msg_q_t *queue = get_thread_thrd_msg_q();
  if (queue == NULL) {
    // Something is wrong.  Bail.
    return reply; // NULL
  }

  if (sent == NULL) {
    // Invalid.
    return reply; // NULL
  }

  // We need to grab the original recipient of the message that was sent before
  // we wait for done in case the recipient reuses this message as the reply.
  thrd_t recipient = sent->to;

  if (thrd_msg_wait_for_done(sent, ts) != thrd_success) {
    // Invalid state of the message.  Fail.
    return reply; // NULL
  }

  if (release == true) {
    // We're done with the message that was originally sent and the caller has
    // indicated that it is to be released now.
    thrd_msg_release(sent);
  }

  // Recipient has processed the message.  We now need to wait for their reply.
  int lockStatus = thrd_success;
  if (ts == NULL) {
    lockStatus = mtx_lock(&queue->lock);
  } else {
    lockStatus = mtx_timedlock(&queue->lock, ts);
  }
  if (lockStatus != thrd_success) {
    // Either we've timed out or there's a problem with the lock.  Either way,
    // we're done.  Bail.
    return reply; // NULL
  }

  // comutexTimedLock will return coroutineTimedout if the timeout is
  // reached, so we'll never reach this point if we've exceeded our timeout.
  thrd_msg_t *prev = NULL;
  thrd_msg_t *cur = queue->head;
  thrd_msg_t **prev_next = &queue->head;
  int searchType = 0;
  if (type != NULL) {
    // This saves us from having to dereference the pointer in every iteration
    // of the loop below.
    searchType = *type;
  }

  // Enter our main wait loop.
  int waitStatus = thrd_success;
  while (reply == NULL) {
    while ((cur != NULL)
      && ((cur->from != recipient)
        || ((type != NULL) && (cur->type != searchType))
      )
    ) {
      prev = cur;
      prev_next = &cur->next;
      cur = cur->next;
    }

    if (cur != NULL) {
      // Desired reply was found.  Remove the message from the coroutine.
      reply = cur;
      *prev_next = cur->next;

      if (queue->head == NULL) {
        // Empty queue.  Set coroutine->lastMessage to NULL too.
        queue->tail = NULL;
      }
      if (queue->tail == cur) {
        queue->tail = prev;
      }
      cur->next = NULL;
    } else {
      // Desired reply was not found.  Block until something else is pushed.
      if (ts == NULL) {
        waitStatus = cnd_wait(&queue->condition, &queue->lock);
      } else {
        waitStatus = cnd_timedwait(
          &queue->condition, &queue->lock, ts);
      }
      if (waitStatus != thrd_success) {
        // Something isn't as expected.  Bail.
        break;
      }
      // coconditionTimedWait will return thrd_timedout if the timeout is
      // reached, so we won't continue the loop if we've exceeded our timeout.
    }

    prev = NULL;
    cur = queue->head;
    prev_next = &queue->head;
  }

  mtx_unlock(&queue->lock);

  return reply;
}

/// @fn thrd_msg_t* thrd_msg_wait_for_reply(
///   thrd_msg_t *sent, bool release, const struct timespec *ts)
///
/// @brief Block until a reply has been received from the original recipient of
/// the provided message or until a specified future time has been reached.
///
/// @param sent The message that was originally sent to the recipient.
/// @param release Whether or not the provided sent message should be released
///   (*NOT* destroyed) after the recipient has indicated that they're done
///   processing our sent message.
/// @param ts A pointer to a struct timespec that holds the end time to wait
///   until for a reply.  If this parameter is NULL, then an infinite timeout
///   will be used.
///
/// @return Returns a pointer to the thrd_msg_t received from the recipient of
/// the original message on success, NULL on failure.
thrd_msg_t* thrd_msg_wait_for_reply(thrd_msg_t *sent, bool release,
  const struct timespec *ts
) {
  return thrd_msg_wait_for_reply_with_type_(sent, release, NULL, ts);
}

/// @fn thrd_msg_t* thrd_msg_wait_for_reply_with_type(
///   thrd_msg_t *sent, bool release, int type, const struct timespec *ts)
///
/// @brief Block until a reply of a specified type has been received from the
/// original recipient of the provided message or until a specified future time
/// has been reached.
///
/// @param sent The message that was originally sent to the recipient.
/// @param release Whether or not the provided sent message should be released
///   (*NOT* destroyed) after the recipient has indicated that they're done
///   processing our sent message.
/// @param type An integer type of message that the caller is waiting for.
/// @param ts A pointer to a struct timespec that holds the end time to wait
///   until for a reply.  If this parameter is NULL, then an infinite timeout
///   will be used.
///
/// @return Returns a pointer to the thrd_msg_t received from the recipient of
/// the original message of the specified tyep on success, NULL on failure or if
/// the provided timeout time is reached.
thrd_msg_t* thrd_msg_wait_for_reply_with_type(thrd_msg_t *sent, bool release,
  int type, const struct timespec *ts
) {
  return thrd_msg_wait_for_reply_with_type_(sent, release, &type, ts);
}

