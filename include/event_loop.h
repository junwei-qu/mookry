#ifndef   _EVENT_LOOP_H
#define  _EVENT_LOOP_H

#include "hlist.h"
#include "list.h"
#include <pthread.h>
#include <stdint.h>

#define EVENT_LOOP_CALLBACK_HASH_SIZE 0xFFFF 
#define EVENT_LOOP_FD_HASH_SIZE    0xFF

enum EVENT_LOOP_CALLBACK_TYPE {
    EVENT_LOOP_CALLBACK_TYPE_READER,
    EVENT_LOOP_CALLBACK_TYPE_WRITER,
    EVENT_LOOP_CALLBACK_TYPE_SIGNAL,
    EVENT_LOOP_CALLBACK_TYPE_TIMER,
    EVENT_LOOP_CALLBACK_TYPE_SOON,
    EVENT_LOOP_CALLBACK_TYPE_CHILD
};

enum EVENT_LOOP_TIMER_TYPE {
    EVENT_LOOP_TIMER_TYPE_ONCE,
    EVENT_LOOP_TIMER_TYPE_INTERVAL
};

typedef struct event_loop {
    struct hlist_head callback_hash[EVENT_LOOP_CALLBACK_HASH_SIZE];
    struct hlist_head fd_hash[EVENT_LOOP_FD_HASH_SIZE];
    struct list_head call_soon_head;
    struct list_head signal_head;
    struct list_head timer_head;
    uint32_t recursive_depth;
    uint64_t source_id;
    pthread_mutex_t mutex;
    int epoll_fd;
    int signalfd;
    int timerfd;
    void (*init)(struct event_loop *);
    void (*destruct)(struct event_loop *);
    void (*poll)(struct event_loop *);
    uint64_t (*new_source_id)(struct event_loop *);
    struct future *(*create_future)(struct event_loop *);
    uint64_t (*add_reader)(struct event_loop *, int, void(*)(struct event_loop *ev, uint64_t source_id, int fd, void *arg), void *);
    uint64_t (*add_writer)(struct event_loop *, int, void(*)(struct event_loop *ev, uint64_t source_id, int fd, void *arg), void *);
    uint64_t (*add_signal)(struct event_loop *, int, void(*)(struct event_loop *ev, uint64_t source_id, int signo, void *arg), void *);
    uint64_t (*add_timer)(struct event_loop *, struct timespec *timespec, enum EVENT_LOOP_TIMER_TYPE timer_type, void(*)(struct event_loop *ev, uint64_t source_id, void *arg), void *);
    uint64_t (*call_soon)(struct event_loop *, void(*)(struct event_loop *ev, uint64_t source_id, void *arg), void *);
    void (*remove_source)(struct event_loop *, uint64_t);
} event_loop;

struct event_loop_callback {
    struct hlist_node hlist_node;
    uint64_t source_id;
    union {
	struct {
           struct list_head node;
	} call_soon_node;
        struct {
           struct hlist_node node;
	   int fd;
	} fd_hlist_node;
	struct {
           struct list_head node;
	   int signo;
	} signal_node;
	struct {
           struct list_head node;
           struct timespec timespec;
	   enum EVENT_LOOP_TIMER_TYPE timer_type;
	} timer_node;
    } list_node;
    enum EVENT_LOOP_CALLBACK_TYPE callback_type;
    void *callback;
    void *arg;
};

event_loop *alloc_event_loop();
void free_event_loop(event_loop *ev);

#endif
