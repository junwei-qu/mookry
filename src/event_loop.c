#include <sys/epoll.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "event_loop.h"
#include "future.h"

static void event_loop_destruct(event_loop *ev);
static uint64_t event_loop_new_source_id_thread_safe(event_loop *ev);
static future * event_loop_create_future(event_loop *ev);
static uint64_t event_loop_add_child_watch(event_loop *ev);
static uint64_t event_loop_add_idle(event_loop *ev);
static uint64_t event_loop_add_timeout(event_loop *ev);
static uint64_t event_loop_add_reader(event_loop *ev);
static uint64_t event_loop_remove_reader(event_loop *ev);
static uint64_t event_loop_add_writer(event_loop *ev);
static uint64_t event_loop_remove_writer(event_loop *ev);
static uint64_t event_loop_add_io_watch(event_loop *ev);
static uint64_t event_loop_remove_source(event_loop *ev);
static uint64_t event_loop_run_until_complete(event_loop *ev, future *future);
static uint64_t event_loop_call_soon(event_loop *ev);
static uint64_t event_loop_call_later(event_loop *ev);
static uint64_t event_loop_call_at(event_loop *ev);

static int event_loop_init(struct event_loop *ev){
    int fd = epoll_create(1);
    if(fd < 0){
        return -1;
    }
    ev->epoll_fd = fd;
    ev->mutex = calloc(1,sizeof(pthread_mutex_t));
    pthread_mutex_init(ev->mutex,NULL);
    ev->source_id = 0;
    ev->recursive_depth = 0;
    ev->destruct = event_loop_destruct;
 //   ev->poll = event_loop_poll;
    ev->new_source_id = event_loop_new_source_id_thread_safe;
    ev->create_future = event_loop_create_future;
    ev->add_child_watch = event_loop_add_child_watch;
    ev->add_idle = event_loop_add_idle;
    ev->add_timeout = event_loop_add_timeout;
    ev->add_reader = event_loop_add_reader;
    ev->remove_reader = event_loop_remove_reader;
    ev->add_writer = event_loop_add_writer;
    ev->remove_writer = event_loop_remove_writer;
    ev->add_io_watch = event_loop_add_io_watch;
    ev->remove_source = event_loop_remove_source;
    ev->run_until_complete = event_loop_run_until_complete;
    ev->call_soon = event_loop_call_soon;
    ev->call_later = event_loop_call_later;
    ev->call_at = event_loop_call_at;
    return 0;
}
static void event_loop_destruct(struct event_loop *ev){
    close(ev->epoll_fd);
    pthread_mutex_destroy(ev->mutex);
    free(ev->mutex);
}
static uint64_t event_loop_new_source_id_thread_safe(event_loop *ev){
    pthread_mutex_lock(ev->mutex);
    (ev->source_id)++;
    if(ev->source_id == 0){
        (ev->source_id)++;
    }
    pthread_mutex_unlock(ev->mutex);
    return ev->source_id;
}
static future * event_loop_create_future(event_loop *ev){
    future * future = alloc_future(ev);
    return future;
}
static uint64_t event_loop_add_child_watch(event_loop *ev){

}
static uint64_t event_loop_add_idle(event_loop *ev){

}
static uint64_t event_loop_add_timeout(event_loop *ev){

}
static uint64_t event_loop_add_reader(event_loop *ev){

}
static uint64_t event_loop_remove_reader(event_loop *ev){

}
static uint64_t event_loop_add_writer(event_loop *ev){

}
static uint64_t event_loop_remove_writer(event_loop *ev){

}
static uint64_t event_loop_add_io_watch(event_loop *ev){

}
static uint64_t event_loop_remove_source(event_loop *ev){

}
static uint64_t event_loop_run_until_complete(event_loop *ev, future *future){

}
static uint64_t event_loop_call_soon(event_loop *ev){

}
static uint64_t event_loop_call_later(event_loop *ev){

}
static uint64_t event_loop_call_at(event_loop *ev){

}
event_loop *alloc_event_loop(){
    event_loop *ev; 
    ev = calloc(1,sizeof(event_loop));
    ev->init = event_loop_init;
    if(ev->init(ev) < 0){
        free(ev);
        return NULL;
    }
    return ev;
}
void free_event_loop(event_loop *ev){
    ev->destruct(ev);
    free(ev);
}
