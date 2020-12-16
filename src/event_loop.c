#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "event_loop.h"
#include "future.h"
#include "hlist.h"

static void event_loop_init(struct event_loop *ev);
static void event_loop_destruct(struct event_loop *ev);
static int event_loop_poll(struct event_loop *ev, int timeout);
static int event_loop_add_fd(struct event_loop *ev, int fd, int event_type, void(*callback)(struct event_loop *ev, int fd, int event_type, void *arg), void *arg);
static void event_loop_remove_fd(struct event_loop *ev, int fd);
static int event_loop_add_signal(struct event_loop *ev, int signo, void(*callback)(struct event_loop *ev, int signo, void *arg), void *arg);
static void event_loop_remove_signal(struct event_loop *ev, int signo);
static uint64_t event_loop_add_timer(struct event_loop *ev, struct timespec *timespec, int(*callback)(struct event_loop *ev, uint64_t timer_id, void *arg), void *arg);
static void event_loop_remove_timer(struct event_loop *ev, uint64_t timer_id);
static int event_loop_add_defer(struct event_loop *ev, int(*callback)(struct event_loop *ev, void *arg), void *arg);

static int event_loop_timer_node_cmp(const void *arg1, const void *arg2) {
    const struct event_loop_timer_node *timer_node1 = arg1;
    const struct event_loop_timer_node *timer_node2 = arg2;
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

static void event_loop_timerfd_callback(struct event_loop *ev, int fd, int event_type, void *arg){
    uint64_t exp; 
    struct itimerspec itimerspec;
    struct event_loop_timer_node *timer_node, tmp_node;
    while(read(fd, &exp, sizeof(exp)) > 0){
    }
    timer_node = NULL;
    clock_gettime(CLOCK_MONOTONIC, &(tmp_node.timespec));
    while((timer_node = ev->timer_heap->peek_value(ev->timer_heap)) && (event_loop_timer_node_cmp(timer_node, &tmp_node) >= 0)){
        if(timer_node->callback(ev, timer_node->timer_id, timer_node->arg)){
            do{
                timer_node->timespec.tv_sec += timer_node->timespec2.tv_sec;
                timer_node->timespec.tv_nsec += timer_node->timespec2.tv_nsec;
                while(timer_node->timespec.tv_nsec >= 1000000000){
                    timer_node->timespec.tv_sec += 1;
         	    timer_node->timespec.tv_nsec -= 1000000000;
                }
       	    }while(event_loop_timer_node_cmp(timer_node, &tmp_node) >= 0);
            ev->timer_heap->heapify(ev->timer_heap, timer_node->heap_value);
        } else {
            ev->timer_heap->delete_value(ev->timer_heap, timer_node->heap_value);
            hlist_del(&(timer_node->hlist_node));
    	    free(timer_node);
        }
    }
    memset(&itimerspec, 0, sizeof(struct itimerspec));
    if(timer_node){
        memcpy(&(itimerspec.it_value), &(timer_node->timespec), sizeof(struct timespec));
        timerfd_settime(ev->timerfd, TFD_TIMER_ABSTIME, &itimerspec, NULL);
    } else {
        timerfd_settime(ev->timerfd, TFD_TIMER_ABSTIME, &itimerspec, NULL);
    }
}

static void event_loop_signalfd_callback(struct event_loop *ev, int fd, int event_type, void *arg){
    struct signalfd_siginfo fdsi;
    struct event_loop_signal_node *cur, *next;
    while(read(fd, &fdsi, sizeof(struct signalfd_siginfo)) > 0){
        list_for_each_entry_safe(cur, next, &(ev->signal_head), list_node) {
            if(cur->signo == fdsi.ssi_signo){
	        cur->callback(ev, cur->signo, arg);
                break;
	    }
        }
    }
}

struct event_loop *alloc_event_loop(){
    struct event_loop *ev; 
    ev = calloc(1, sizeof(struct event_loop));
    ev->init = event_loop_init;
    ev->destruct = event_loop_destruct;
    ev->poll = event_loop_poll;
    ev->add_fd = event_loop_add_fd;
    ev->remove_fd = event_loop_remove_fd;
    ev->add_signal = event_loop_add_signal;
    ev->remove_signal = event_loop_remove_signal;
    ev->add_timer = event_loop_add_timer;
    ev->remove_timer = event_loop_remove_timer;
    ev->add_defer = event_loop_add_defer;
    ev->init(ev);
    return ev;
}

static void event_loop_init(struct event_loop *ev){
    ev->epollfd = epoll_create(1);
    ev->source_id = 1;
    int i;
    for(i = 0; i < EVENT_LOOP_FD_HASH_SIZE; i++){
        INIT_HLIST_HEAD(&ev->fd_hash[i]);
    }
    for(i = 0; i < EVENT_LOOP_TIMER_HASH_SIZE; i++){
        INIT_HLIST_HEAD(&ev->timer_hash[i]);
    }
    INIT_LIST_HEAD(&ev->defer_head); 
    INIT_LIST_HEAD(&ev->signal_head); 
    sigset_t mask;
    sigemptyset(&mask);
    ev->signalfd = signalfd(-1, &mask, SFD_NONBLOCK|SFD_CLOEXEC);
    ev->add_fd(ev, ev->signalfd, EVENT_LOOP_FD_READ, event_loop_signalfd_callback, NULL);
    ev->timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK|TFD_CLOEXEC);
    ev->add_fd(ev, ev->timerfd, EVENT_LOOP_FD_READ, event_loop_timerfd_callback, NULL);
    ev->timer_heap = alloc_heap(event_loop_timer_node_cmp);
}

void free_event_loop(struct event_loop *ev){
    ev->destruct(ev);
    free(ev);
}

static void event_loop_destruct(struct event_loop *ev){
    struct event_loop_fd_node *fd_node;
    struct event_loop_timer_node *timer_node;
    struct hlist_node *cur, *next;
    struct hlist_head *head;
    close(ev->epollfd);
    close(ev->signalfd);
    close(ev->timerfd);
    free_heap(ev->timer_heap);
    int i;
    for(i=0; i < EVENT_LOOP_FD_HASH_SIZE; i++){
        head = &ev->fd_hash[i];
        hlist_for_each_entry_safe(fd_node, cur, next, head, hlist_node){
            free(fd_node);
	}
    }
    for(i=0; i < EVENT_LOOP_TIMER_HASH_SIZE; i++){
        head = &ev->timer_hash[i];
        hlist_for_each_entry_safe(timer_node, cur, next, head, hlist_node){
            free(timer_node);
	}
    }
    struct event_loop_signal_node *cur_signal_node, *next_signal_node;
    list_for_each_entry_safe(cur_signal_node, next_signal_node, &(ev->signal_head), list_node) {
        free(cur_signal_node);
    }
    struct event_loop_defer_node *cur_defer_node, *next_defer_node;
    list_for_each_entry_safe(cur_defer_node, next_defer_node, &(ev->defer_head), list_node) {
        free(cur_defer_node);
    }
}

static int event_loop_add_fd(struct event_loop *ev, int fd, int event_type, void(*callback)(struct event_loop *ev, int fd, int event_type, void *arg), void *arg){
    struct epoll_event epoll_event;
    struct event_loop_fd_node *fd_node;
    struct hlist_node *cur, *next;
    struct hlist_head *head = &(ev->fd_hash[EVENT_LOOP_FD_HASH(fd)]);
    memset(&epoll_event, 0, sizeof(epoll_event));
    event_type = event_type & (EVENT_LOOP_FD_READ | EVENT_LOOP_FD_WRITE);
    if(event_type & EVENT_LOOP_FD_READ && event_type & EVENT_LOOP_FD_WRITE){
        epoll_event.events |= EPOLLIN | EPOLLOUT;
    } else if(event_type & EVENT_LOOP_FD_READ){
        epoll_event.events |= EPOLLIN;
    } else if(event_type & EVENT_LOOP_FD_WRITE){
        epoll_event.events |= EPOLLOUT;
    }
    epoll_event.events |= EPOLLRDHUP | EPOLLET;
    epoll_event.data.fd = fd;
    hlist_for_each_entry_safe(fd_node, cur, next, head, hlist_node){
        if(fd_node->fd == fd){
	    fd_node->callback = callback;
	    fd_node->arg = arg;
	    if(event_type != fd_node->event_type){
                if(!fd_node->event_type){
		    epoll_ctl(ev->epollfd, EPOLL_CTL_ADD, fd, &epoll_event);
		} else if(!event_type){
		    epoll_ctl(ev->epollfd, EPOLL_CTL_DEL, fd, NULL);
		} else {
		    epoll_ctl(ev->epollfd, EPOLL_CTL_MOD, fd, &epoll_event);
		}
	        fd_node->event_type = event_type;
	    }
	    return 0;
	}
    }
    fd_node = calloc(1, sizeof(struct event_loop_fd_node));
    hlist_add_head(&(fd_node->hlist_node), &(ev->fd_hash[EVENT_LOOP_FD_HASH(fd)]));
    fd_node->fd = fd;
    fd_node->callback = callback;
    fd_node->arg = arg;
    fd_node->event_type = event_type;
    if(event_type){
        epoll_ctl(ev->epollfd, EPOLL_CTL_ADD, fd, &epoll_event);
    }
    return 0;
}

static void event_loop_remove_fd(struct event_loop *ev, int fd){
    struct event_loop_fd_node *fd_node;
    struct hlist_node *cur, *next;
    struct hlist_head *head = &(ev->fd_hash[EVENT_LOOP_FD_HASH(fd)]);
    hlist_for_each_entry_safe(fd_node, cur, next, head, hlist_node){
        if(fd_node->fd == fd){
	    if(fd_node->event_type){
                epoll_ctl(ev->epollfd, EPOLL_CTL_DEL, fd, NULL);
	    }
            hlist_del(&(fd_node->hlist_node));
	    free(fd_node);
	    return;
	}
    }
}

static uint64_t event_loop_add_timer(struct event_loop *ev, struct timespec *timespec, int(*callback)(struct event_loop *ev, uint64_t timer_id, void *arg), void *arg){
    struct event_loop_timer_node *timer_node = calloc(1, sizeof(struct event_loop_timer_node));
    timer_node->timer_id = ev->source_id++;
    hlist_add_head(&(timer_node->hlist_node), &(ev->timer_hash[EVENT_LOOP_TIMER_HASH(timer_node->timer_id)]));
    memcpy(&(timer_node->timespec), timespec, sizeof(struct timespec));
    memcpy(&(timer_node->timespec2), timespec, sizeof(struct timespec));
    struct timespec tmp_ts; 
    clock_gettime(CLOCK_MONOTONIC, &tmp_ts);
    timer_node->timespec.tv_sec += tmp_ts.tv_sec;
    timer_node->timespec.tv_nsec += tmp_ts.tv_nsec;
    while(timer_node->timespec.tv_nsec >= 1000000000){
        timer_node->timespec.tv_sec += 1;
	timer_node->timespec.tv_nsec -= 1000000000;
    }
    timer_node->callback = callback;
    timer_node->arg = arg;
    struct event_loop_timer_node *value1, *value2;
    value1 = ev->timer_heap->peek_value(ev->timer_heap); 
    timer_node->heap_value = ev->timer_heap->insert_value(ev->timer_heap, timer_node);
    value2 = ev->timer_heap->peek_value(ev->timer_heap); 
    if(value1 != value2){
        struct itimerspec itimerspec;
        memset(&itimerspec, 0, sizeof(struct itimerspec));
	memcpy(&(itimerspec.it_value), &(value2->timespec), sizeof(struct timespec));
        timerfd_settime(ev->timerfd, TFD_TIMER_ABSTIME, &itimerspec, NULL);
    }
    return timer_node->timer_id;
}

static void event_loop_remove_timer(struct event_loop *ev, uint64_t timer_id) {
    struct event_loop_timer_node *timer_node;
    struct event_loop_timer_node *value1, *value2;
    struct itimerspec itimerspec;
    struct hlist_node *cur, *next;
    struct hlist_head *head = &(ev->timer_hash[EVENT_LOOP_TIMER_HASH(timer_id)]);
    memset(&itimerspec, 0, sizeof(struct itimerspec));
    hlist_for_each_entry_safe(timer_node, cur, next, head, hlist_node){
        if(timer_node->timer_id == timer_id){
            value1 = ev->timer_heap->peek_value(ev->timer_heap); 
            ev->timer_heap->delete_value(ev->timer_heap, timer_node->heap_value);
            value2 = ev->timer_heap->peek_value(ev->timer_heap); 
            if(!value2){
                timerfd_settime(ev->timerfd, TFD_TIMER_ABSTIME, &itimerspec, NULL);
            } else if(value1 != value2){
                memcpy(&(itimerspec.it_value), &(value2->timespec), sizeof(struct timespec));
                timerfd_settime(ev->timerfd, TFD_TIMER_ABSTIME, &itimerspec, NULL);
            }
            hlist_del(&(timer_node->hlist_node));
	    free(timer_node);
	    return;
	}
    }
}

static int event_loop_add_signal(struct event_loop *ev, int signo, void(*callback)(struct event_loop *ev, int signo, void *arg), void *arg){
    struct event_loop_signal_node *cur, *next;
    list_for_each_entry_safe(cur, next, &(ev->signal_head), list_node) {
        if(cur->signo == signo){
	    cur->callback = callback;
	    cur->arg = arg;
	    return 0;
	}
    }
    cur = calloc(1, sizeof(struct event_loop_signal_node));
    sigset_t mask;
    sigemptyset(&mask);
    sigprocmask(SIG_SETMASK, NULL, &mask);
    sigaddset(&mask, signo);
    sigprocmask(SIG_SETMASK, &mask, NULL);
    signalfd(ev->signalfd, &mask, SFD_NONBLOCK|SFD_CLOEXEC);
    cur->signo = signo;
    cur->callback = callback;
    cur->arg = arg;
    list_add_before(&(cur->list_node), &(ev->signal_head));
    return 0;
}

static void event_loop_remove_signal(struct event_loop *ev, int signo){
    struct event_loop_signal_node *cur, *next;
    list_for_each_entry_safe(cur, next, &(ev->signal_head), list_node) {
        if(cur->signo == signo){
            sigset_t mask;
            sigemptyset(&mask);
            sigprocmask(SIG_SETMASK, NULL, &mask);
            sigdelset(&mask, signo);
            sigprocmask(SIG_SETMASK, &mask, NULL);
            signalfd(ev->signalfd, &mask, SFD_NONBLOCK|SFD_CLOEXEC);
	    list_del(&cur->list_node);
	    free(cur);
            return;
	}
    }
}

static int event_loop_add_defer(struct event_loop *ev, int(*callback)(struct event_loop *ev, void *arg), void *arg){
    struct event_loop_defer_node *defer_node = calloc(1, sizeof(struct event_loop_defer_node));
    defer_node->callback = callback;
    defer_node->arg = arg;
    list_add_before(&(defer_node->list_node), &(ev->defer_head));
    return 0;
}

static int event_loop_poll(struct event_loop *ev, int timeout){
    int nfds, n, fd, events, event_type;
    struct event_loop_fd_node *fd_node;
    struct event_loop_defer_node *cur_defer, *next_defer;
    struct hlist_node *cur, *next;
    struct hlist_head *head; 
    event_type = 0;
    nfds = epoll_wait(ev->epollfd, ev->events, EVENT_LOOP_MAX_EVENTS, timeout);
    if(nfds > 0){
        for(n = 0; n < nfds; n++){
            fd = ev->events[n].data.fd;
	    events = ev->events[n].events;
	    if(events & EPOLLIN){
                event_type |= EVENT_LOOP_FD_READ;
	    }
	    if(events & EPOLLOUT){
                event_type |= EVENT_LOOP_FD_WRITE;
	    }
	    if(events & EPOLLRDHUP){
                event_type |= EVENT_LOOP_FD_READ;
	    }
	    if(events & EPOLLERR){
                event_type |= EVENT_LOOP_FD_READ;
	    }
	    if(events & EPOLLHUP){
                event_type |= EVENT_LOOP_FD_READ;
	    }
            head = &(ev->fd_hash[EVENT_LOOP_FD_HASH(fd)]);
            hlist_for_each_entry_safe(fd_node, cur, next, head, hlist_node){
                if(fd_node->fd == fd){
		    fd_node->callback(ev, fd, event_type, fd_node->arg);
		    break;
	        }
            }
	}
    }
    list_for_each_entry_safe(cur_defer, next_defer, &(ev->defer_head), list_node) {
        if(!cur_defer->callback(ev, cur_defer->arg)){
	    list_del(&cur_defer->list_node);
	    free(cur_defer);
	}
    }
    return nfds;
}

