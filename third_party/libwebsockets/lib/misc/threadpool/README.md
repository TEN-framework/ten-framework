## Threadpool

### Overview

![overview](/doc-assets/threadpool.svg)

An api that lets you create a pool of worker threads, and a queue of tasks that
are bound to a wsi.  Tasks in their own thread  synchronize communication to the
lws service thread of the wsi via `LWS_CALLBACK_SERVER_WRITEABLE` and friends.

Tasks can produce some output, then return that they want to "sync" with the
service thread.  That causes a `LWS_CALLBACK_SERVER_WRITEABLE` in the service
thread context, where the output can be consumed, and the task told to continue,
or completed tasks be reaped.

ALL of the details related to thread synchronization and an associated wsi in
the lws service thread context are handled by the threadpool api, without needing
any pthreads in user code.

### Example

https://libwebsockets.org/git/libwebsockets/tree/minimal-examples/ws-server/minimal-ws-server-threadpool

### Lifecycle considerations

#### Tasks vs wsi

Although all tasks start out as being associated to a wsi, in fact the lifetime
of a task and that of the wsi are not necessarily linked.

You may start a long task, eg, that runs atomically in its thread for 30s, and
at any time the client may close the connection, eg, close a browser window.

There are arrangements that a task can "check in" periodically with lws to see
if it has been asked to stop, allowing the task lifetime to be related to the
wsi lifetime somewhat, but some tasks are going to be atomic and longlived.

For that reason, at wsi close an ongoing task can detach from the wsi and
continue until it ends or understands it has been asked to stop.  To make
that work, the task is created with a `cleanup` callback that performs any
freeing independent of still having a wsi around to do it... the task takes over
responsibility to free the user pointer on destruction when the task is created.

![Threadpool States](/doc-assets/threadpool-states.svg)

#### Reaping completed tasks

Once created, although tasks may run asynchronously, the task itself does not
get destroyed on completion but added to a "done queue".  Only when the lws
service thread context queries the task state with `lws_threadpool_task_status()`
may the task be reaped and memory freed.

This is analogous to unix processes and `wait()`.

If a task became detached from its wsi, then joining the done queue is enough
to get the task reaped, since there's nobody left any more to synchronize the
reaping with.

### User interface

The api is declared at https://libwebsockets.org/git/libwebsockets/tree/include/libwebsockets/lws-threadpool.h

#### Threadpool creation / destruction

The threadpool should be created at program or vhost init using
`lws_threadpool_create()` and destroyed on exit or vhost destruction using
first `lws_threadpool_finish()` and then `lws_threadpool_destroy()`.

Threadpools should be named, varargs are provided on the create function
to facilite eg, naming the threadpool by the vhost it's associated with.

Threadpool creation takes an args struct with the following members:

Member|function
---|---
threads|The maxiumum number of independent threads in the pool
max_queue_depth|The maximum number of tasks allowed to wait for a place in the pool

#### Task creation / destruction

Tasks are created and queued using `lws_threadpool_enqueue()`, this takes an
args struct with the following members

Member|function
---|---
wsi|The wsi the task is initially associated with
user|An opaque user-private pointer used for communication with the lws service thread and private state / data
task|A pointer to the function that will run in the pool thread
cleanup|A pointer to a function that will clean up finished or stopped tasks (perhaps freeing user)

Tasks also should have a name, the creation function again provides varargs
to simplify naming the task with string elements related to who started it
and why.

#### The task function itself

The task function receives the task user pointer and the task state.  The
possible task states are

State|Meaning
---|---
LWS_TP_STATUS_QUEUED|Task is still waiting for a pool thread
LWS_TP_STATUS_RUNNING|Task is supposed to do its work
LWS_TP_STATUS_SYNCING|Task is blocked waiting for sync from lws service thread
LWS_TP_STATUS_STOPPING|Task has been asked to stop but didn't stop yet
LWS_TP_STATUS_FINISHED|Task has reported it has completed
LWS_TP_STATUS_STOPPED|Task has aborted

The task function will only be told `LWS_TP_STATUS_RUNNING` or
`LWS_TP_STATUS_STOPPING` in its status argument... RUNNING means continue with the
user task and STOPPING means clean up and return `LWS_TP_RETURN_STOPPED`.

If possible every 100ms or so the task should return `LWS_TP_RETURN_CHECKING_IN`
to allow lws to inform it reasonably quickly that it has been asked to stop
(eg, because the related wsi has closed), or if it can continue.  If not
possible, it's okay but eg exiting the application may experience delays
until the running task finishes, and since the wsi may have gone, the work
is wasted.

The task function may return one of

Return|Meaning
---|---
LWS_TP_RETURN_CHECKING_IN|Still wants to run, but confirming nobody asked him to stop.  Will be called again immediately with `LWS_TP_STATUS_RUNNING` or `LWS_TP_STATUS_STOPPING`
LWS_TP_RETURN_SYNC|Task wants to trigger a WRITABLE callback and block until lws service thread restarts it with `lws_threadpool_task_sync()`
LWS_TP_RETURN_FINISHED|Task has finished, successfully as far as it goes
LWS_TP_RETURN_STOPPED|Task has finished, aborting in response to a request to stop

The SYNC or CHECKING_IN return may also have a flag `LWS_TP_RETURN_FLAG_OUTLIVE`
applied to it, which indicates to threadpool that this task wishes to remain
unstopped after the wsi closes.  This is useful in the case where the task
understands it will take a long time to complete, and wants to return a
complete status and maybe close the connection, perhaps with a token identifying
the task.  The task can then be monitored separately by using the token.

#### Synchronizing

The task can choose to "SYNC" with the lws service thread, in other words
cause a WRITABLE callback on the associated wsi in the lws service thread
context and block itself until it hears back from there via
`lws_threadpool_task_sync()` to resume the task.

This is typically used when, eg, the task has filled its buffer, or ringbuffer,
and needs to pause operations until what's done has been sent and some buffer
space is open again.

In the WRITABLE callback, in lws service thread context, the buffer can be
sent with `lws_write()` and then `lws_threadpool_task_sync()` to allow the task
to fill another buffer and continue that way.

If the WRITABLE callback determines that the task should stop, it can just call
`lws_threadpool_task_sync()` with the second argument as 1, to force the task
to stop immediately after it resumes.

#### The cleanup function

When a finished task is reaped, or a task that become detached from its initial
wsi completes or is stopped, it calls the `.cleanup` function defined in the
task creation args struct to free anything related to the user pointer.

With threadpool, responsibility for freeing allocations used by the task belongs
strictly with the task, via the `.cleanup` function, once the task has been
enqueued.  That's different from a typical non-threadpool protocol where the
wsi lifecycle controls deallocation.  This reflects the fact that the task
may outlive the wsi.

#### Protecting against WRITABLE and / or SYNC duplication

Care should be taken than data prepared by the task thread in the user priv
memory should only be sent once.  For example, after sending data from a user
priv buffer of a given length stored in the priv, zero down the length.

Task execution and the SYNC writable callbacks are mutually exclusive, so there
is no danger of collision between the task thread and the lws service thread if
the reason for the callback is a SYNC operation from the task thread.

### Thread overcommit

If the tasks running on the threads are ultimately network-bound for all or some
of their processing (via the SYNC with the WRITEABLE callback), it's possible
to overcommit the number of threads in the pool compared to the number of
threads the processor has in hardware to get better occupancy in the CPU.
