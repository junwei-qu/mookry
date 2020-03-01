#ifndef   _EVENT_LOOP_H
#define  _EVENT_LOOP_H
#include <pthread.h>
#include <stdint.h>
typedef struct event_loop {
    uint32_t recursive_depth;
    uint64_t source_id;
    pthread_mutex_t *mutex;
    int epoll_fd;
    int (*init)(struct event_loop *ev);
    void (*destruct)(struct event_loop *ev);
    void (*poll)(struct event_loop *ev);
    uint64_t (*new_source_id)(struct event_loop *ev);
    struct future *(*create_future)(struct event_loop *);
    uint64_t (*add_child_watch)(struct event_loop *);
    uint64_t (*add_idle)(struct event_loop *);
    uint64_t (*add_timeout)(struct event_loop *);
    uint64_t (*add_reader)(struct event_loop *);
    uint64_t (*remove_reader)(struct event_loop *);
    uint64_t (*add_writer)(struct event_loop *);
    uint64_t (*remove_writer)(struct event_loop *);
    uint64_t (*add_io_watch)(struct event_loop *);
    uint64_t (*remove_source)(struct event_loop *);
    uint64_t (*run_until_complete)(struct event_loop*, struct future *);
    uint64_t (*call_soon)(struct event_loop *);
    uint64_t (*call_later)(struct event_loop *);
    uint64_t (*call_at)(struct event_loop *);
} event_loop;

event_loop *alloc_event_loop();
void free_event_loop(event_loop *ev);

#endif
