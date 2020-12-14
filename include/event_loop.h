#ifndef   _EVENT_LOOP_H
#define  _EVENT_LOOP_H

#include "hlist.h"
#include "list.h"
#include "balance_binary_heap.h"
#include <pthread.h>
#include <stdint.h>

#define EVENT_LOOP_TIMER_HASH_SIZE 8192 
#define EVENT_LOOP_TIMER_HASH(timer_id) ((timer_id) & (EVENT_LOOP_TIMER_HASH_SIZE - 1))
#define EVENT_LOOP_FD_HASH_SIZE 8192 
#define EVENT_LOOP_FD_HASH(fd) ((fd) & (EVENT_LOOP_FD_HASH_SIZE - 1))
#define EVENT_LOOP_FD_READ 0x01
#define EVENT_LOOP_FD_WRITE 0x02

struct event_loop {
    int epollfd;
    int signalfd;
    int timerfd;
    uint64_t source_id;
    struct hlist_head fd_hash[EVENT_LOOP_FD_HASH_SIZE];
    struct list_head defer_head;
    struct list_head signal_head;
    struct hlist_head timer_hash[EVENT_LOOP_TIMER_HASH_SIZE];
    struct balance_binary_heap *timer_heap;
    void (*init)(struct event_loop *ev);
    void (*destruct)(struct event_loop *ev);
    void (*poll)(struct event_loop *ev);
    int (*add_fd)(struct event_loop *ev, int fd, int event_type, void(*callback)(struct event_loop *ev, int fd, int event_type, void *arg), void *arg);
    void (*remove_fd)(struct event_loop *ev, int fd);
    int (*add_signal)(struct event_loop *ev, int signo, void(*callback)(struct event_loop *ev, int signo, void *arg), void *arg);
    void (*remove_signal)(struct event_loop *ev, int signo);
    uint64_t (*add_timer)(struct event_loop *ev, struct timespec *timespec, int(*callback)(struct event_loop *ev, uint64_t timer_id, void *arg), void *arg);
    void (*remove_timer)(struct event_loop *ev, uint64_t timer_id);
    int (*add_defer)(struct event_loop *ev, int(*callback)(struct event_loop *ev, void *arg), void *arg);
};

struct event_loop_timer_node {
   struct hlist_node hlist_node;
   struct balance_binary_heap_value *heap_value;
   struct timespec timespec;
   struct timespec timespec2;
   uint64_t timer_id;
   int (*callback)(struct event_loop *ev, uint64_t timer_id, void *arg);
   void *arg;
};

struct event_loop_signal_node {
    struct list_head list_node;
    int signo;
    void (*callback)(struct event_loop *ev, int signo, void *arg);
    void *arg;
};

struct event_loop_fd_node {
    struct hlist_node hlist_node;
    int fd;
    int event_type;
    void (*callback)(struct event_loop *ev, int fd, int event_type, void *arg);
    void *arg;
};

struct event_loop_defer_node {
    struct list_head list_node;
    int (*callback)(struct event_loop *ev, void *arg);
    void *arg;
};

struct event_loop *alloc_event_loop();
void free_event_loop(struct event_loop *ev);

#endif
