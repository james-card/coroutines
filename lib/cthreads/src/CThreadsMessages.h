///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              01.12.2022
///
/// @file              CThreadsMessages.h
///
/// @brief             Inter-process message functionality for C threads.
///
/// @copyright
///                   Copyright (c) 2012-2024 James Card
///
/// Permission is hereby granted, free of charge, to any person obtaining a
/// copy of this software and associated documentation files (the "Software"),
/// to deal in the Software without restriction, including without limitation
/// the rights to use, copy, modify, merge, publish, distribute, sublicense,
/// and/or sell copies of the Software, and to permit persons to whom the
/// Software is furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included
/// in all copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
/// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
/// DEALINGS IN THE SOFTWARE.
///
///                                James Card
///                         http://www.jamescard.org
///
///////////////////////////////////////////////////////////////////////////////

#ifndef MESSAGES_H
#define MESSAGES_H

#include <stdint.h>
#include <stdbool.h>
#include "CThreads.h"

#ifdef __cplusplus
extern "C"
{
#endif

/// @struct thrd_msg_t
///
/// @brief Definition for a message that can be pushed onto a thread's message
/// queue.
///
/// @param type Integer value designating the type of message for the receiving
///   thread.
/// @param data A pointer to the data of the message.
/// @param size The number of bytes pointed to by the data pointer.
/// @param next A pointer to the next thrd_msg_t in a thread's message queue.
/// @param waiting A Boolean flag to indicate whether or not the sender is
///   waiting on a response message from the recipient of the message.
/// @param done A Boolean flag to indicate whether or not the receiving thread
///   has handled the message yet.
/// @param in_use A Boolean flag to indicate whether or not this thrd_msg_t is in
///   use.
/// @param from The thrd_t value for the sending thread.
/// @param to The thrd_t value for the receiving thread.
/// @param condition A condition (cnd_t) that will allow for signalling between
///   threads when setting the done flag.
/// @param lock A mutex (mtx_t) to guard the condition.
/// @param configured Whether or not the members of the message that require
///   initialization have been configured yet.
/// @param dynamically_allocated Whether or not the message was dynamically
///   allocated with thrd_msg_create.
typedef struct thrd_msg_t {
  int type;
  void *data;
  size_t size;
  struct thrd_msg_t *next;
  bool waiting;
  bool done;
  bool in_use;
  thrd_t from;
  thrd_t to;
  cnd_t condition;
  mtx_t lock;
  bool configured;
  bool dynamically_allocated;
} thrd_msg_t;

// Message queue functions
thrd_msg_t* thrd_msg_q_peek(void);
thrd_msg_t* thrd_msg_q_pop(void);
thrd_msg_t* thrd_msg_q_pop_type(int type);
thrd_msg_t* thrd_msg_q_wait(const struct timespec *ts);
thrd_msg_t* thrd_msg_q_wait_for_type(int type, const struct timespec *ts);
int thrd_msg_q_push(thrd_t thr, thrd_msg_t *msg);

// Message functions
thrd_msg_t* thrd_msg_create(void);
thrd_msg_t* thrd_msg_destroy(thrd_msg_t *msg);
int thrd_msg_init(
  thrd_msg_t *msg, int type, void *data, size_t size, bool waiting);
int thrd_msg_release(thrd_msg_t *msg);
int thrd_msg_set_done(thrd_msg_t *msg);
int thrd_msg_wait_for_done(thrd_msg_t *msg, const struct timespec *ts);
thrd_msg_t* thrd_msg_wait_for_reply(thrd_msg_t *sent, bool release, const struct timespec *ts);
thrd_msg_t* thrd_msg_wait_for_reply_with_type(thrd_msg_t *sent, bool release, int type,
  const struct timespec *ts);

// Message accessors
#define thrd_msg_type(thrd_msg_ptr) \
  (((thrd_msg_ptr) != NULL) ? (thrd_msg_ptr)->type : 0)
#define thrd_msg_data(thrd_msg_ptr) \
  (((thrd_msg_ptr) != NULL) ? (thrd_msg_ptr)->data : NULL)
#define thrd_msg_size(thrd_msg_ptr) \
  (((thrd_msg_ptr) != NULL) ? (thrd_msg_ptr)->size : 0)
// No accessor for next member element.
#define thrd_msg_waiting(thrd_msg_ptr) \
  (((thrd_msg_ptr) != NULL) ? (thrd_msg_ptr)->waiting : false)
#define thrd_msg_done(thrd_msg_ptr) \
  (((thrd_msg_ptr) != NULL) ? (thrd_msg_ptr)->done : true)
#define thrd_msg_in_use(thrd_msg_ptr) \
  (((thrd_msg_ptr) != NULL) ? (thrd_msg_ptr)->inUse : false)
#define thrd_msg_from(thrd_msg_ptr) \
  (((thrd_msg_ptr) != NULL) ? (thrd_msg_ptr)->from : 0)
#define thrd_msg_to(thrd_msg_ptr) \
  (((thrd_msg_ptr) != NULL) ? (thrd_msg_ptr)->to : 0)
#define thrd_msg_configured(thrd_msg_ptr) \
  (((thrd_msg_ptr) != NULL) ? (thrd_msg_ptr)->configured : false)


#ifdef __cplusplus
}
#endif

#endif // MESSAGES_H

