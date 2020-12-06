#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "event_loop.h"
#include "future.h"
#include "hlist.h"

static void event_loop_init(event_loop *);
static void event_loop_destruct(event_loop *);
static void event_loop_poll(event_loop *);
static uint64_t event_loop_new_source_id(event_loop *);
static future * event_loop_create_future(event_loop *);
static uint64_t event_loop_add_reader(event_loop *, int, void(*)(struct event_loop *ev, uint64_t source_id, int fd, void *arg), void *);
static uint64_t event_loop_add_writer(event_loop *, int, void(*)(struct event_loop *ev, uint64_t source_id, int fd, void *arg), void *);
static uint64_t event_loop_add_signal(event_loop *, int, void(*)(struct event_loop *ev, uint64_t source_id, int signo, void *arg), void *);
static uint64_t event_loop_add_timer(event_loop *, struct timespec *, enum EVENT_LOOP_TIMER_TYPE, void(*)(struct event_loop *ev, uint64_t source_id, void *arg), void *);
static uint64_t event_loop_call_soon(event_loop *, void(*)(struct event_loop *ev, uint64_t source_id, void *arg), void *);
static void event_loop_remove_timer(event_loop *, struct event_loop_callback_node *);
static void event_loop_remove_source(event_loop *, uint64_t);
static int timer_node_cmp(const void *arg1, const void *arg2) {
    const struct timer_node *timer_node1 = arg1;
    const struct timer_node *timer_node2 = arg2;
    if(timer_node2->timespec.tv_sec > timer_node1->timespec.tv_sec){
        return 1;
    } else if(timer_node2->timespec.tv_sec < timer_node1->timespec.tv_sec){
        return -1;
    } else if(timer_node2->timespec.tv_nsec > timer_node1->timespec.tv_nsec){
        return 1;
    } else if(timer_node2->timespec.tv_nsec < timer_node1->timespec.tv_nsec){
        return -1;
    } else {
        return 0;
    }
}

event_loop *alloc_event_loop(){
    event_loop *ev; 
    ev = calloc(1,sizeof(event_loop));
    ev->init = event_loop_init;
    ev->destruct = event_loop_destruct;
    ev->poll = event_loop_poll;
    ev->new_source_id = event_loop_new_source_id;
    ev->create_future = event_loop_create_future;
    ev->add_reader = event_loop_add_reader;
    ev->add_writer = event_loop_add_writer;
    ev->add_signal = event_loop_add_signal;
    ev->add_timer = event_loop_add_timer;
    ev->call_soon = event_loop_call_soon;
    ev->remove_source = event_loop_remove_source;
    ev->init(ev);
    return ev;
}

static void event_loop_init(struct event_loop *ev){
    ev->epollfd = epoll_create(1);
    pthread_mutex_init(&ev->mutex,NULL);
    ev->source_id = 0;
    ev->recursive_depth = 0;
    sigset_t mask;
    sigemptyset(&mask);
    ev->signalfd = signalfd(-1, &mask, SFD_NONBLOCK|SFD_CLOEXEC);
    ev->timer_heap = alloc_heap(timer_node_cmp);
    ev->timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK|TFD_CLOEXEC);
    for(int i = 0; i < EVENT_LOOP_CALLBACK_HASH_SIZE; i++){
        INIT_HLIST_HEAD(&ev->callback_hash[i]);
    }
    for(int i = 0; i < EVENT_LOOP_FD_HASH_SIZE; i++){
        INIT_HLIST_HEAD(&ev->fd_hash[i]);
    }
    INIT_LIST_HEAD(&ev->call_soon_head); 
    INIT_LIST_HEAD(&ev->signal_head); 
}

void free_event_loop(event_loop *ev){
    ev->destruct(ev);
    free(ev);
}

static void event_loop_destruct(struct event_loop *ev){
    struct event_loop_callback_node *tpos;
    struct hlist_node *pos, *tmp;
    struct hlist_head *head;
    for(int i=0; i < EVENT_LOOP_CALLBACK_HASH_SIZE; i++){
        head = &ev->callback_hash[i];
        hlist_for_each_entry_safe(tpos, pos, tmp, head, hlist_node){
            free(tpos);
	}
    }
    close(ev->epollfd);
    close(ev->signalfd);
    close(ev->timerfd);
    free_heap(ev->timer_heap);
    pthread_mutex_destroy(&ev->mutex);
}

static uint64_t event_loop_new_source_id(event_loop *ev){
    pthread_mutex_lock(&ev->mutex);
    (ev->source_id)++;
    pthread_mutex_unlock(&ev->mutex);
    return ev->source_id;
}

static future * event_loop_create_future(event_loop *ev){
    future * future = alloc_future(ev);
    return future;
}

static uint64_t event_loop_add_timer(event_loop *ev, struct timespec *timespec, enum EVENT_LOOP_TIMER_TYPE timer_type, void(*callback)(struct event_loop *ev, uint64_t source_id, void *arg), void *arg){
    struct event_loop_callback_node *callback_node = malloc(sizeof(struct event_loop_callback_node));
    callback_node->source_id = ev->new_source_id(ev);
    hlist_add_head(&(callback_node->hlist_node), &(ev->callback_hash[EVENT_LOOP_CALLBACK_HASH(callback_node->source_id)]));
    struct timer_node *tn = &(callback_node->list_node.timer_node);
    memcpy(&(tn->timespec), timespec, sizeof(struct timespec));
    struct timespec tmp_ts; 
    clock_gettime(CLOCK_MONOTONIC, &tmp_ts);
    tn->timespec.tv_sec += tmp_ts.tv_sec;
    tn->timespec.tv_nsec += tmp_ts.tv_nsec;
    while(tn->timespec.tv_nsec >= 1000000000){
        tn->timespec.tv_sec += 1;
	tn->timespec.tv_nsec -= 1000000000;
    }
    callback_node->callback_type = EVENT_LOOP_CALLBACK_TYPE_TIMER;
    tn->timer_type = timer_type;
    callback_node->callback = callback;
    callback_node->arg = arg;
    struct timer_node *value1, *value2;
    value1 = ev->timer_heap->peek_value(ev->timer_heap); 
    tn->heap_value = ev->timer_heap->insert_value(ev->timer_heap, tn);
    value2 = ev->timer_heap->peek_value(ev->timer_heap); 
    if(value1 != value2){
        struct itimerspec itimerspec;
        memset(&itimerspec, 0, sizeof(struct itimerspec));
	memcpy(&(itimerspec.it_value), &(value2->timespec), sizeof(struct timespec));
        timerfd_settime(ev->timerfd, TFD_TIMER_ABSTIME, &itimerspec, NULL);
    }
    return callback_node->source_id;
}

static uint64_t event_loop_add_reader(event_loop *ev, int fd, void(*callback)(struct event_loop *ev, uint64_t source_id, int fd, void *arg), void *arg){

}

static uint64_t event_loop_add_writer(event_loop *ev, int fd, void(*callback)(struct event_loop *ev, uint64_t source_id, int fd, void *arg), void *arg){

}

static void event_loop_remove_timer(event_loop *ev, struct event_loop_callback_node *callback_node) {
    struct timer_node *tn = &(callback_node->list_node.timer_node);
    struct timer_node *value1, *value2;
    struct itimerspec itimerspec;
    memset(&itimerspec, 0, sizeof(struct itimerspec));
    value1 = ev->timer_heap->peek_value(ev->timer_heap); 
    ev->timer_heap->delete_value(ev->timer_heap, tn->heap_value);
    value2 = ev->timer_heap->peek_value(ev->timer_heap); 
    if(!value2){
        timerfd_settime(ev->timerfd, TFD_TIMER_ABSTIME, &itimerspec, NULL);
    } else if(value1 != value2){
        memcpy(&(itimerspec.it_value), &(value2->timespec), sizeof(struct timespec));
        timerfd_settime(ev->timerfd, TFD_TIMER_ABSTIME, &itimerspec, NULL);
    }
}

static void event_loop_remove_source(event_loop *ev, uint64_t source_id){
    struct event_loop_callback_node *tpos;
    struct hlist_node *pos, *tmp;
    struct hlist_head *head = &(ev->callback_hash[EVENT_LOOP_CALLBACK_HASH(source_id)]);
    hlist_for_each_entry_safe(tpos, pos, tmp, head, hlist_node){
        if(tpos->source_id == source_id){
            if(tpos->callback_type == EVENT_LOOP_CALLBACK_TYPE_TIMER){
                event_loop_remove_timer(ev, tpos);
            }
	    hlist_del(pos);
            free(tpos);
	    break;
	}
    }
}

static uint64_t event_loop_call_soon(event_loop *ev, void(*callback)(struct event_loop *ev, uint64_t source_id, void *arg), void *arg){

}

static void event_loop_poll(event_loop *ev){

}

static uint64_t event_loop_add_signal(event_loop *ev, int signo, void(*callback)(struct event_loop *ev, uint64_t source_id, int signo, void *arg), void *arg){

}
