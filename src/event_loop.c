#include <sys/epoll.h>
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
static void event_loop_remove_source(event_loop *, uint64_t);

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
    ev->epoll_fd = epoll_create(1);
    pthread_mutex_init(&ev->mutex,NULL);
    ev->source_id = 0;
    ev->recursive_depth = 0;
    ev->signalfd = -1;
    ev->timerfd = -1;
    for(int i = 0; i < EVENT_LOOP_CALLBACK_HASH_SIZE; i++){
        INIT_HLIST_HEAD(&ev->callback_hash[i]);
    }
    for(int i = 0; i < EVENT_LOOP_FD_HASH_SIZE; i++){
        INIT_HLIST_HEAD(&ev->fd_hash[i]);
    }
    INIT_LIST_HEAD(&ev->call_soon_head); 
    INIT_LIST_HEAD(&ev->signal_head); 
    INIT_LIST_HEAD(&ev->timer_head); 
}

void free_event_loop(event_loop *ev){
    ev->destruct(ev);
    free(ev);
}

static void event_loop_destruct(struct event_loop *ev){
    struct event_loop_callback *tpos;
    struct hlist_node *pos, *tmp;
    struct hlist_head *head;
    for(int i=0; i < EVENT_LOOP_CALLBACK_HASH_SIZE; i++){
        head = &ev->callback_hash[i];
        hlist_for_each_entry_safe(tpos,pos,tmp,head,hlist_node){
            free(tpos);
	}
    }
    close(ev->epoll_fd);
    if(ev->signalfd != -1){
        close(ev->signalfd);
    }
    if(ev->timerfd != -1){
        close(ev->signalfd);
    }
    pthread_mutex_destroy(&ev->mutex);
}

static uint64_t event_loop_new_source_id(event_loop *ev){
    pthread_mutex_lock(&ev->mutex);
    (ev->source_id)++;
    if(ev->source_id == 0){
        (ev->source_id)++;
    }
    pthread_mutex_unlock(&ev->mutex);
    return ev->source_id;
}

static future * event_loop_create_future(event_loop *ev){
    future * future = alloc_future(ev);
    return future;
}

static uint64_t event_loop_add_timer(event_loop *ev, struct timespec *timespec, enum EVENT_LOOP_TIMER_TYPE timer_type, void(*callback)(struct event_loop *ev, uint64_t source_id, void *arg), void *arg){

}
static uint64_t event_loop_add_reader(event_loop *ev, int fd, void(*callback)(struct event_loop *ev, uint64_t source_id, int fd, void *arg), void *arg){

}
static uint64_t event_loop_add_writer(event_loop *ev, int fd, void(*callback)(struct event_loop *ev, uint64_t source_id, int fd, void *arg), void *arg){

}
static void event_loop_remove_source(event_loop *ev, uint64_t source_id){

}
static uint64_t event_loop_call_soon(event_loop *ev, void(*callback)(struct event_loop *ev, uint64_t source_id, void *arg), void *arg){

}
static void event_loop_poll(event_loop *ev){

}
static uint64_t event_loop_add_signal(event_loop *ev, int signo, void(*callback)(struct event_loop *ev, uint64_t source_id, int signo, void *arg), void *arg){

}
