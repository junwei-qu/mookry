#define _GNU_SOURCE
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include "coroutine.h"
#include "event_loop.h"
#include "list.h"
#include "channel.h"

#define COROUTINE_CHANNEL_HASH_SIZE 64
#define WAITING_COROUTINE_HASH_SIZE 64

struct coroutine {
    struct list_head list_node;
    void (*routine)(void *arg);
    void *arg;
    void *stack_pointer;
    int stack_size;
    gregset_t gregs;
    typeof(*((fpregset_t)(0))) fpregs;
    struct timespec resume_time;
    void *mem_base;
    int mem_size;
    struct hlist_head channels[COROUTINE_CHANNEL_HASH_SIZE];
};

struct channel_node {
    struct hlist_node node;
    int64_t channel_id;
};

struct co_signal_arg {
    int signo;
    void(*handler)(int signo, void *arg);
    void *arg;
} co_signal_args[_NSIG+1];
static sigset_t signal_set;

struct receive_send_list_node {
    struct list_head node;
    struct coroutine *coroutine;
};

struct waiting_node {
    struct hlist_node node;
    char name[CHANNEL_NAME_SIZE+1];
    struct list_head receive_list;
    struct list_head send_list;
};

struct event_loop *main_event_loop;
struct channel_pool *main_channel_pool;
struct coroutine  main_coroutine;
struct coroutine  *cur_coroutine = &main_coroutine;
struct hlist_head waiting_coroutine_hash[WAITING_COROUTINE_HASH_SIZE];

uint64_t coroutine_count = 0;
LIST_HEAD(ready_co_head);

void *make_fcontext(void *sp, int size, void(*routine)(struct coroutine *coroutine));
void *jump_fcontext(void **old_sp, void *new_sp, struct coroutine *coroutine, int preserve_fpu);
static inline void routine_start(struct coroutine *coroutine);
static inline int defer_destroy_coroutine(struct event_loop *ev, void *coroutine);
static inline void destroy_coroutine(struct coroutine *coroutine);
static inline void resume_coroutine(struct coroutine *coroutine);
static inline void yield_coroutine();
static inline void reader_writer_callback(struct event_loop *ev, int fd, int event_type, void *coroutine);
static inline int sleep_callback(struct event_loop *ev, int64_t timer_id, void *coroutine);
static inline void signal_callback(struct event_loop *ev, int signo, void *arg);
static inline void co_signal_callback(void *arg);
static struct waiting_node *find_waiting_node(int64_t channel_id, int create);
static inline uint32_t channel_name_hash(char *name, int len);
static inline void preempt_resume(int signo, siginfo_t *siginfo, void *arg);
static inline void preempt_interrupt(int signo, siginfo_t *siginfo, void *arg);
static inline void preempt_coroutine();
static inline void enable_preempt_interrupt();
static inline void disable_preempt_interrupt();

int co_env(void (*co_start)(void *), void *arg);
int co_make(uint32_t stack_size, void(*routine)(void *), void *arg);
ssize_t co_write(int sockfd, const void *buf, size_t count, double timeout);
ssize_t co_send(int sockfd, const void *buf, size_t len, int flags, double timeout);
ssize_t co_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen, double timeout);
ssize_t co_sendmsg(int sockfd, const struct msghdr *msg, int flags, double timeout);
ssize_t co_read(int sockfd, void *buf, size_t count, double timeout);
ssize_t co_recv(int sockfd, void *buf, size_t len, int flags, double timeout);
ssize_t co_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen, double timeout);
ssize_t co_recvmsg(int sockfd, struct msghdr *msg, int flags, double timeout);
int co_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int co_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int co_accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);
void co_sleep(double seconds);
void co_add_signal(int signo, void(*handler)(int signo, void *arg), void *arg);
void co_remove_signal(int signo);
int channel_send(int64_t channel_id, const char *msg_ptr, size_t msg_len, double timeout);
int channel_receive(int64_t channel_id, char *msg_ptr, size_t msg_len, double timeout);
void channel_unlink(char *name);
void channel_close(int64_t channel_id);
int64_t channel_open(char *name, int msgsize, int maxmsg);

static inline void enable_preempt_interrupt(){
    return;
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_sigaction = preempt_interrupt;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIGPROF, &sa, NULL);
}

static inline void disable_preempt_interrupt(){
    signal(SIGPROF, SIG_IGN);
}

static inline void preempt_resume(int signo, siginfo_t *siginfo, void *arg){
    ucontext_t *context = arg;
    memcpy(context->uc_mcontext.gregs, cur_coroutine->gregs, sizeof(cur_coroutine->gregs));
    memcpy(context->uc_mcontext.fpregs, &(cur_coroutine->fpregs), sizeof(cur_coroutine->fpregs));
}

static inline void preempt_interrupt(int signo, siginfo_t *siginfo, void *arg){
    if(cur_coroutine != &main_coroutine && (cur_coroutine->resume_time.tv_sec || cur_coroutine->resume_time.tv_nsec)){
        struct timespec cur_time, delta_time;
	long min_time_delta = 10000000;
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cur_time);
	delta_time.tv_sec = cur_time.tv_sec - cur_coroutine->resume_time.tv_sec;
	if(cur_time.tv_nsec < cur_coroutine->resume_time.tv_nsec){
            delta_time.tv_sec -= 1;
	    delta_time.tv_nsec = cur_time.tv_nsec + 1000000000 - cur_coroutine->resume_time.tv_nsec;
	} else {
	    delta_time.tv_nsec = cur_time.tv_nsec - cur_coroutine->resume_time.tv_nsec;
	}
	if(delta_time.tv_sec > 0 || delta_time.tv_nsec >= min_time_delta){
            ucontext_t *context = arg;
            memcpy(cur_coroutine->gregs, context->uc_mcontext.gregs, sizeof(cur_coroutine->gregs));
            memcpy(&(cur_coroutine->fpregs), context->uc_mcontext.fpregs, sizeof(cur_coroutine->fpregs));
            context->uc_mcontext.gregs[REG_RIP] = (long)preempt_coroutine;
	}
    }
}

static inline void preempt_coroutine(){
    co_sleep(0);
    raise(SIGUSR1);
}

static inline void routine_start(struct coroutine *coroutine){
    int i;
    cur_coroutine = coroutine;
    enable_preempt_interrupt();
    for(i = 0; i < COROUTINE_CHANNEL_HASH_SIZE; i++){
        INIT_HLIST_HEAD(&coroutine->channels[i]);
    }

    coroutine->routine(coroutine->arg);

    struct hlist_head *head;
    struct hlist_node *cur, *next;
    struct channel_node *channel_node;
    for(i=0; i < COROUTINE_CHANNEL_HASH_SIZE; i++){
        head = &coroutine->channels[i];
        hlist_for_each_entry_safe(channel_node, cur, next, head, node){
	    channel_close(channel_node->channel_id);
            free(channel_node);
        }
    }
    if(!list_empty(&(coroutine->list_node))){
        list_del(&(coroutine->list_node));
    }
    main_event_loop->add_defer(main_event_loop, defer_destroy_coroutine, coroutine);
    yield_coroutine();
}

static inline int defer_destroy_coroutine(struct event_loop *ev, void *coroutine){
    destroy_coroutine(coroutine);
    return 0;
}

static inline void destroy_coroutine(struct coroutine* coroutine){
    munmap(coroutine->mem_base, coroutine->mem_size);
    coroutine_count -= 1;
}

static inline void resume_coroutine(struct coroutine *coroutine){
    assert(cur_coroutine != coroutine);
    if(cur_coroutine != &main_coroutine){
        disable_preempt_interrupt();
    }
    if((cur_coroutine != &main_coroutine) && list_empty(&(cur_coroutine->list_node))){
        list_add_before(&(cur_coroutine->list_node), &ready_co_head);
    }
    if((coroutine != &main_coroutine) && list_empty(&(coroutine->list_node))){
        list_add_before(&(coroutine->list_node), &ready_co_head);
    }
    if(cur_coroutine != &main_coroutine){
        memset(&(cur_coroutine->resume_time), 0, sizeof(cur_coroutine->resume_time));
    }
    if(coroutine != &main_coroutine){
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &(coroutine->resume_time));
    }
    cur_coroutine = jump_fcontext(&(cur_coroutine->stack_pointer), coroutine->stack_pointer, coroutine, 1);

    if(cur_coroutine != &main_coroutine){
        enable_preempt_interrupt();
    }
}

static inline void yield_coroutine(){
    assert(cur_coroutine != &main_coroutine);
    disable_preempt_interrupt();
    if(!list_empty(&(cur_coroutine->list_node))){
        list_del(&(cur_coroutine->list_node));
    }
    memset(&(cur_coroutine->resume_time), 0, sizeof(cur_coroutine->resume_time));
    cur_coroutine = jump_fcontext(&(cur_coroutine->stack_pointer), main_coroutine.stack_pointer, &main_coroutine, 1);
    enable_preempt_interrupt();
}

static inline void reader_writer_callback(struct event_loop *ev, int fd, int event_type, void *coroutine){
    resume_coroutine(coroutine);
}

static inline int sleep_callback(struct event_loop *ev, int64_t timer_id, void *coroutine){
    resume_coroutine(coroutine);
    return 0;
}

static inline void co_signal_callback(void *arg) {
    struct co_signal_arg *co_signal_arg = arg;
    co_signal_arg->handler(co_signal_arg->signo, co_signal_arg->arg);
}

static inline void signal_callback(struct event_loop *ev, int singo, void *arg){
    co_make(0, co_signal_callback, arg);
}

static inline uint32_t channel_name_hash(char *name, int len){
    uint32_t ret = 0;
    int i;
    for(i = 0; i < len; i++){
       ret += (unsigned char)name[i];
    }
    return ret & (WAITING_COROUTINE_HASH_SIZE - 1);
}
 
static struct waiting_node *find_waiting_node(int64_t channel_id, int create){
    struct hlist_head *waiting_head;
    struct hlist_node *cur_waiting, *next_waiting;
    struct waiting_node *waiting_node, *find_node = NULL;
    char name[CHANNEL_NAME_SIZE+1];
    main_channel_pool->getname(main_channel_pool, channel_id, name, sizeof(name));
    waiting_head = &(waiting_coroutine_hash[channel_name_hash(name, strlen(name))]);
    hlist_for_each_entry_safe(waiting_node, cur_waiting, next_waiting, waiting_head, node){
        if(strcmp(waiting_node->name, name) == 0){
            find_node = waiting_node; 
	    break;
        }
    }
    if(!find_node && create){
        find_node = calloc(1, sizeof(struct waiting_node));
        INIT_LIST_HEAD(&(find_node->send_list));
        INIT_LIST_HEAD(&(find_node->receive_list));
        strcpy(find_node->name, name);
        hlist_add_head(&(find_node->node), waiting_head);
    }
    return find_node;
}

int co_env(void (*co_start)(void *), void *arg){
    assert(!main_event_loop);
    int ret = 0;
    struct coroutine *cur, *next;
    main_event_loop = alloc_event_loop();
    main_channel_pool = alloc_channel_pool();
    sigemptyset(&signal_set);
    int i;
    for(i = 0; i < WAITING_COROUTINE_HASH_SIZE; i++){
        INIT_HLIST_HEAD(&waiting_coroutine_hash[i]);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_sigaction = preempt_resume;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIGUSR1, &sa, NULL);

    disable_preempt_interrupt();

    struct itimerval itimerval;
    memset(&itimerval, 0, sizeof(struct itimerval));
    itimerval.it_interval.tv_usec = 10000;
    itimerval.it_value.tv_usec = 10000;
    setitimer(ITIMER_PROF, &itimerval, NULL);

    co_make(0, co_start, arg);
    while((!sigisemptyset(&signal_set) || coroutine_count) && ret >= 0){
        while(!list_empty(&ready_co_head)){
           list_for_each_entry_safe(cur, next, &ready_co_head, list_node) {
	       resume_coroutine(cur);
           }
	}
        ret = main_event_loop->poll(main_event_loop, -1);
    }

    struct hlist_node *cur_waiting, *next_waiting;
    struct hlist_head *waiting_head;
    struct waiting_node *waiting_node;

    for(i = 0; i < WAITING_COROUTINE_HASH_SIZE; i++){
        waiting_head = &waiting_coroutine_hash[i];
        hlist_for_each_entry_safe(waiting_node, cur_waiting, next_waiting, waiting_head, node){
	   free(waiting_node);
        }
    }
    free_event_loop(main_event_loop);
    free_channel_pool(main_channel_pool);
    main_event_loop = NULL;
    main_channel_pool = NULL;
    return ret;
}

int co_make(uint32_t stack_size, void(*routine)(void *), void *arg){
    int page_size = sysconf(_SC_PAGE_SIZE);
    if(!stack_size){
        stack_size = DEFAULT_COROUTINE_STACK_SIZE;
    }
    int map_size = sizeof(struct coroutine) + stack_size + page_size;
    if(map_size < 2 * page_size){
        map_size = 2 * page_size;
    } else if(map_size & (page_size - 1)){
        map_size += page_size;
	map_size -= (map_size & (page_size - 1));
    }
    void *mem_base = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1 ,0);
    if(!mem_base){
        return -1;
    }
    mprotect(mem_base, page_size, PROT_NONE);
    struct coroutine *coroutine = (struct coroutine *)((char *)mem_base + map_size - sizeof(struct coroutine));
    memset(coroutine, 0, sizeof(struct coroutine));
    INIT_LIST_HEAD(&(coroutine->list_node));
    coroutine->mem_base = mem_base;
    coroutine->mem_size = map_size;
    coroutine->stack_size = stack_size;
    coroutine->routine = routine;
    coroutine->arg = arg;
    coroutine->stack_pointer = make_fcontext((char *)coroutine - 1, stack_size, routine_start);
    coroutine_count += 1;
    resume_coroutine(coroutine);
    return 0;
}

ssize_t co_write(int sockfd, const void *buf, size_t count, double timeout){
    assert(main_event_loop);
    int ret, timeout_ret = 0;
    loop:
    while((ret = main_event_loop->write(main_event_loop, sockfd, buf, count)) < 0 && errno == EINTR){
    }
    if(ret >= 0 || timeout == 0){
        return ret;
    }
    if(timeout_ret == 1){
        errno = 0;
        return 0;
    }
    if(ret == -1 && errno == EAGAIN){
	main_event_loop->add_writer(main_event_loop, sockfd, reader_writer_callback, cur_coroutine);
        int64_t timer_id = 0;
	if(timeout > 0) {
            int integer_seconds = (int)(timeout);
            long nano_seconds = (long)((timeout - integer_seconds)* 1000000000);
            struct timespec ts;
            ts.tv_sec = integer_seconds;
            ts.tv_nsec = nano_seconds;
            timer_id = main_event_loop->add_timer(main_event_loop, &ts, sleep_callback, cur_coroutine);
	}
        yield_coroutine();
	if(timer_id > 0){
            main_event_loop->remove_timer(main_event_loop, timer_id);
	    timeout_ret = 1;
	}
	main_event_loop->remove_writer(main_event_loop, sockfd);
	goto loop;
    }
    return ret;
}

ssize_t co_send(int sockfd, const void *buf, size_t len, int flags, double timeout) {
    assert(main_event_loop);
    int ret, timeout_ret = 0;
    loop:
    while((ret = main_event_loop->send(main_event_loop, sockfd, buf, len, flags)) < 0 && errno == EINTR){
    }
    if(ret >= 0 || timeout == 0){
        return ret;
    }
    if(timeout_ret == 1){
        errno = 0;
        return 0;
    }
    if(ret == -1 && errno == EAGAIN){
	main_event_loop->add_writer(main_event_loop, sockfd, reader_writer_callback, cur_coroutine);
        int64_t timer_id = 0;
	if(timeout > 0) {
            int integer_seconds = (int)(timeout);
            long nano_seconds = (long)((timeout - integer_seconds)* 1000000000);
            struct timespec ts;
            ts.tv_sec = integer_seconds;
            ts.tv_nsec = nano_seconds;
            timer_id = main_event_loop->add_timer(main_event_loop, &ts, sleep_callback, cur_coroutine);
	}
        yield_coroutine();
	if(timer_id > 0){
            main_event_loop->remove_timer(main_event_loop, timer_id);
	    timeout_ret = 1;
	}
	main_event_loop->remove_writer(main_event_loop, sockfd);
	goto loop;
    }
    return ret;
}

ssize_t co_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen, double timeout){
    assert(main_event_loop);
    int ret, timeout_ret = 0;
    loop:
    while((ret = main_event_loop->sendto(main_event_loop, sockfd, buf, len, flags, dest_addr, addrlen)) < 0 && errno == EINTR){
    }
    if(ret >= 0 || timeout == 0){
        return ret;
    }
    if(timeout_ret == 1){
        errno = 0;
        return 0;
    }
    if(ret == -1 && errno == EAGAIN){
	main_event_loop->add_writer(main_event_loop, sockfd, reader_writer_callback, cur_coroutine);
        int64_t timer_id = 0;
	if(timeout > 0) {
            int integer_seconds = (int)(timeout);
            long nano_seconds = (long)((timeout - integer_seconds)* 1000000000);
            struct timespec ts;
            ts.tv_sec = integer_seconds;
            ts.tv_nsec = nano_seconds;
            timer_id = main_event_loop->add_timer(main_event_loop, &ts, sleep_callback, cur_coroutine);
	}
        yield_coroutine();
	if(timer_id > 0){
            main_event_loop->remove_timer(main_event_loop, timer_id);
	    timeout_ret = 1;
	}
	main_event_loop->remove_writer(main_event_loop, sockfd);
	goto loop;
    }
    return ret;
}

ssize_t co_sendmsg(int sockfd, const struct msghdr *msg, int flags, double timeout){
    assert(main_event_loop);
    int ret, timeout_ret = 0;
    loop:
    while((ret = main_event_loop->sendmsg(main_event_loop, sockfd, msg, flags)) < 0 && errno == EINTR){
    }
    if(ret >= 0 || timeout == 0){
        return ret;
    }
    if(timeout_ret == 1){
        errno = 0;
        return 0;
    }
    if(ret == -1 && errno == EAGAIN){
	main_event_loop->add_writer(main_event_loop, sockfd, reader_writer_callback, cur_coroutine);
        int64_t timer_id = 0;
	if(timeout > 0) {
            int integer_seconds = (int)(timeout);
            long nano_seconds = (long)((timeout - integer_seconds)* 1000000000);
            struct timespec ts;
            ts.tv_sec = integer_seconds;
            ts.tv_nsec = nano_seconds;
            timer_id = main_event_loop->add_timer(main_event_loop, &ts, sleep_callback, cur_coroutine);
	}
        yield_coroutine();
	if(timer_id > 0){
            main_event_loop->remove_timer(main_event_loop, timer_id);
	    timeout_ret = 1;
	}
	main_event_loop->remove_writer(main_event_loop, sockfd);
	goto loop;
    }
    return ret;
}

ssize_t co_read(int sockfd, void *buf, size_t count, double timeout){
    assert(main_event_loop);
    int ret, timeout_ret = 0;
    loop:
    while((ret = main_event_loop->read(main_event_loop, sockfd, buf, count)) < 0 && errno == EINTR){
    }
    if(ret >= 0 || timeout == 0){
        return ret;
    }
    if(timeout_ret == 1){
        errno = 0;
        return 0;
    }
    if(ret == -1 && errno == EAGAIN){
	main_event_loop->add_reader(main_event_loop, sockfd, reader_writer_callback, cur_coroutine);
        int64_t timer_id = 0;
	if(timeout > 0) {
            int integer_seconds = (int)(timeout);
            long nano_seconds = (long)((timeout - integer_seconds)* 1000000000);
            struct timespec ts;
            ts.tv_sec = integer_seconds;
            ts.tv_nsec = nano_seconds;
            timer_id = main_event_loop->add_timer(main_event_loop, &ts, sleep_callback, cur_coroutine);
	}
        yield_coroutine();
	if(timer_id > 0){
            main_event_loop->remove_timer(main_event_loop, timer_id);
	    timeout_ret = 1;
	}
	main_event_loop->remove_reader(main_event_loop, sockfd);
	goto loop;
    }
    return ret;
}

ssize_t co_recv(int sockfd, void *buf, size_t len, int flags, double timeout){
    assert(main_event_loop);
    int ret, timeout_ret = 0;
    loop:
    while((ret = main_event_loop->recv(main_event_loop, sockfd, buf, len, flags)) < 0 && errno == EINTR){
    }
    if(ret >= 0 || timeout == 0){
        return ret;
    }
    if(timeout_ret == 1){
        errno = 0;
        return 0;
    }
    if(ret == -1 && errno == EAGAIN){
	main_event_loop->add_reader(main_event_loop, sockfd, reader_writer_callback, cur_coroutine);
        int64_t timer_id = 0;
	if(timeout > 0) {
            int integer_seconds = (int)(timeout);
            long nano_seconds = (long)((timeout - integer_seconds)* 1000000000);
            struct timespec ts;
            ts.tv_sec = integer_seconds;
            ts.tv_nsec = nano_seconds;
            timer_id = main_event_loop->add_timer(main_event_loop, &ts, sleep_callback, cur_coroutine);
	}
        yield_coroutine();
	if(timer_id > 0){
            main_event_loop->remove_timer(main_event_loop, timer_id);
	    timeout_ret = 1;
	}
	main_event_loop->remove_reader(main_event_loop, sockfd);
	goto loop;
    }
    return ret;
}

ssize_t co_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen, double timeout){
    assert(main_event_loop);
    int ret, timeout_ret = 0;
    loop:
    while((ret = main_event_loop->recvfrom(main_event_loop, sockfd, buf, len, flags, src_addr, addrlen)) < 0 && errno == EINTR){
    }
    if(ret >= 0 || timeout == 0){
        return ret;
    }
    if(timeout_ret == 1){
        errno = 0;
        return 0;
    }
    if(ret == -1 && errno == EAGAIN){
	main_event_loop->add_reader(main_event_loop, sockfd, reader_writer_callback, cur_coroutine);
        int64_t timer_id = 0;
	if(timeout > 0) {
            int integer_seconds = (int)(timeout);
            long nano_seconds = (long)((timeout - integer_seconds)* 1000000000);
            struct timespec ts;
            ts.tv_sec = integer_seconds;
            ts.tv_nsec = nano_seconds;
            timer_id = main_event_loop->add_timer(main_event_loop, &ts, sleep_callback, cur_coroutine);
	}
        yield_coroutine();
	if(timer_id > 0){
            main_event_loop->remove_timer(main_event_loop, timer_id);
	    timeout_ret = 1;
	}
	main_event_loop->remove_reader(main_event_loop, sockfd);
	goto loop;
    }
    return ret;
}

ssize_t co_recvmsg(int sockfd, struct msghdr *msg, int flags, double timeout){
    assert(main_event_loop);
    int ret, timeout_ret = 0;
    loop:
    while((ret = main_event_loop->recvmsg(main_event_loop, sockfd, msg, flags)) < 0 && errno == EINTR){
    }
    if(ret >= 0 || timeout == 0){
        return ret;
    }
    if(timeout_ret == 1){
        errno = 0;
        return 0;
    }
    if(ret == -1 && errno == EAGAIN){
	main_event_loop->add_reader(main_event_loop, sockfd, reader_writer_callback, cur_coroutine);
        int64_t timer_id = 0;
	if(timeout > 0) {
            int integer_seconds = (int)(timeout);
            long nano_seconds = (long)((timeout - integer_seconds)* 1000000000);
            struct timespec ts;
            ts.tv_sec = integer_seconds;
            ts.tv_nsec = nano_seconds;
            timer_id = main_event_loop->add_timer(main_event_loop, &ts, sleep_callback, cur_coroutine);
	}
        yield_coroutine();
	if(timer_id > 0){
            main_event_loop->remove_timer(main_event_loop, timer_id);
	    timeout_ret = 1;
	}
	main_event_loop->remove_reader(main_event_loop, sockfd);
	goto loop;
    }
    return ret;
}

int co_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen){
    assert(main_event_loop);
    int ret, optval;
    socklen_t optlen = sizeof(optval);
    while((ret = connect(sockfd, addr, addrlen)) < 0 && errno == EINTR){
    }
    if(ret == -1 && (errno == EAGAIN || errno == EINPROGRESS)){
	main_event_loop->add_writer(main_event_loop, sockfd, reader_writer_callback, cur_coroutine);
	yield_coroutine();
	main_event_loop->remove_writer(main_event_loop, sockfd);
	optval = 0;
	getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen);
	if(optval != 0){
            errno = optval;
	    ret = -1;
	} else {
            ret = 0;
	}
    }
    return ret;
}

int co_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen){
    assert(main_event_loop);
    int ret;
    loop:
    while((ret = main_event_loop->accept(main_event_loop, sockfd, addr, addrlen)) < 0 && errno == EINTR){
    }
    if(ret == -1 && errno == EAGAIN){
	main_event_loop->add_reader(main_event_loop, sockfd, reader_writer_callback, cur_coroutine);
	yield_coroutine();
	main_event_loop->remove_reader(main_event_loop, sockfd);
	goto loop;
    }
    return ret;
}

int co_accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags){
    assert(main_event_loop);
    int ret;
    loop:
    while((ret = main_event_loop->accept4(main_event_loop, sockfd, addr, addrlen, flags)) < 0 && errno == EINTR){
    }
    if(ret == -1 && errno == EAGAIN){
	main_event_loop->add_reader(main_event_loop, sockfd, reader_writer_callback, cur_coroutine);
	yield_coroutine();
	main_event_loop->remove_reader(main_event_loop, sockfd);
	goto loop;
    }
    return ret;
}

void co_sleep(double seconds){
    assert(main_event_loop);
    int integer_seconds = (int)(seconds);
    long nano_seconds = (long)((seconds - integer_seconds)* 1000000000);
    struct timespec ts;
    ts.tv_sec = integer_seconds;
    ts.tv_nsec = nano_seconds;
    main_event_loop->add_timer(main_event_loop, &ts, sleep_callback, cur_coroutine);
    yield_coroutine();
}

void co_add_signal(int signo, void(*handler)(int signo, void *arg), void *arg){
    assert(main_event_loop);
    struct co_signal_arg * co_signal_arg = co_signal_args + signo;
    co_signal_arg->signo = signo;
    co_signal_arg->handler = handler;
    co_signal_arg->arg = arg;
    main_event_loop->add_signal(main_event_loop, signo, signal_callback, co_signal_arg);
    sigaddset(&signal_set, signo);
}

void co_remove_signal(int signo){
    assert(main_event_loop);
    main_event_loop->remove_signal(main_event_loop, signo);
    sigdelset(&signal_set, signo);
}

int64_t channel_open(char *name, int msgsize, int maxmsg){
    assert(main_channel_pool);
    int64_t channel_id = main_channel_pool->open(main_channel_pool, name, msgsize, maxmsg);
    if(channel_id < 0){
        return -1;
    }
    struct channel_node *channel_node = calloc(1, sizeof(channel_node));       
    channel_node->channel_id = channel_id;
    struct hlist_head *head = &(cur_coroutine->channels[channel_id & (COROUTINE_CHANNEL_HASH_SIZE -1)]);
    hlist_add_head(&(channel_node->node), head);
    return channel_id;
}

void channel_close(int64_t channel_id){
    assert(main_channel_pool);
    struct hlist_node *cur, *next;
    struct channel_node *channel_node;
    struct hlist_head *head = &(cur_coroutine->channels[channel_id & (COROUTINE_CHANNEL_HASH_SIZE -1)]);
    hlist_for_each_entry_safe(channel_node, cur, next, head, node){
        if(channel_node->channel_id == channel_id){
	    main_channel_pool->close(main_channel_pool, channel_id);
            break;
        }
    }
}

void channel_unlink(char *name){
    assert(main_channel_pool);
    main_channel_pool->unlink(main_channel_pool, name);
}

int channel_receive(int64_t channel_id, char *msg_ptr, size_t msg_len, double timeout){
    assert(main_channel_pool);
    struct hlist_node *cur, *next;
    struct channel_node *channel_node;
    struct waiting_node *find_node = NULL;
    struct hlist_head *head = &(cur_coroutine->channels[channel_id & (COROUTINE_CHANNEL_HASH_SIZE -1)]);
    int need_find = 1;
    hlist_for_each_entry_safe(channel_node, cur, next, head, node){
        if(channel_node->channel_id == channel_id){
	    if(msg_len < main_channel_pool->getmsgsize(main_channel_pool, channel_id)){
                return -1;
	    }
	    if(main_channel_pool->isempty(main_channel_pool, channel_id)){
	        if(timeout == 0){
		    errno = EAGAIN;
                    return -1;
		}
	        find_node = find_waiting_node(channel_id, 1);
                struct receive_send_list_node receive_list_node;
		receive_list_node.coroutine = cur_coroutine;
		list_add_before(&(receive_list_node.node), &(find_node->receive_list));
		uint64_t timer_id = 0;
		if(timeout > 0) {
                    int integer_seconds = (int)(timeout);
                    long nano_seconds = (long)((timeout - integer_seconds)* 1000000000);
                    struct timespec ts;
                    ts.tv_sec = integer_seconds;
                    ts.tv_nsec = nano_seconds;
                    timer_id = main_event_loop->add_timer(main_event_loop, &ts, sleep_callback, cur_coroutine);
		}
                yield_coroutine();
		if(timer_id > 0){
                    main_event_loop->remove_timer(main_event_loop, timer_id);
		}
		list_del(&(receive_list_node.node));
	        if(list_empty(&(find_node->receive_list)) && list_empty(&(find_node->send_list))){
	            hlist_del(&(find_node->node));
	            free(find_node);
		    find_node = NULL;
		    need_find = 0;
	        }
		if(main_channel_pool->isempty(main_channel_pool, channel_id)){
		    return 0;
		}
	    } 
	    int receive_ret = main_channel_pool->receive(main_channel_pool, channel_id, msg_ptr, msg_len);
	    if(receive_ret >= 0){
	        if(!find_node && need_find){
	            find_node = find_waiting_node(channel_id, 0);
		}
		if(find_node && !list_empty(&(find_node->send_list))){
	            struct receive_send_list_node *send_list_node = list_entry(find_node->send_list.next, typeof(*send_list_node), node);
	            resume_coroutine(send_list_node->coroutine);
		}
	    }
	    return receive_ret;
        }
    }
    errno = EINVAL;
    return -1;
}

int channel_send(int64_t channel_id, const char *msg_ptr, size_t msg_len, double timeout){
    assert(main_channel_pool);
    struct hlist_node *cur, *next;
    struct channel_node *channel_node;
    struct waiting_node *find_node = NULL;
    struct hlist_head *head = &(cur_coroutine->channels[channel_id & (COROUTINE_CHANNEL_HASH_SIZE -1)]);
    int need_find = 1;
    hlist_for_each_entry_safe(channel_node, cur, next, head, node){
        if(channel_node->channel_id == channel_id){
	    if(msg_len > main_channel_pool->getmsgsize(main_channel_pool, channel_id)){
                return -1;
	    }
	    if(main_channel_pool->isfull(main_channel_pool, channel_id)){
	        if(timeout == 0){
		    errno = EAGAIN;
                    return -1;
		}
	        find_node = find_waiting_node(channel_id, 1);
                struct receive_send_list_node send_list_node;
		send_list_node.coroutine = cur_coroutine;
		list_add_before(&(send_list_node.node), &(find_node->send_list));
		int64_t timer_id = 0;
		if(timeout > 0) {
                    int integer_seconds = (int)(timeout);
                    long nano_seconds = (long)((timeout - integer_seconds)* 1000000000);
                    struct timespec ts;
                    ts.tv_sec = integer_seconds;
                    ts.tv_nsec = nano_seconds;
                    timer_id = main_event_loop->add_timer(main_event_loop, &ts, sleep_callback, cur_coroutine);
		}
                yield_coroutine();
		if(timer_id > 0){
                    main_event_loop->remove_timer(main_event_loop, timer_id);
		}
		list_del(&(send_list_node.node));
	        if(list_empty(&(find_node->receive_list)) && list_empty(&(find_node->send_list))){
	            hlist_del(&(find_node->node));
	            free(find_node);
		    find_node = NULL;
		    need_find = 0;
	        }
		if(main_channel_pool->isfull(main_channel_pool, channel_id)){
		    return 0;
		}
	    } 
	    int send_ret = main_channel_pool->send(main_channel_pool, channel_id, msg_ptr, msg_len);
	    if(send_ret >= 0){
	        if(!find_node && need_find){
	            find_node = find_waiting_node(channel_id, 0);
		}
		if(find_node && !list_empty(&(find_node->receive_list))){
                    struct receive_send_list_node *receive_list_node = list_entry(find_node->receive_list.next, typeof(*receive_list_node), node);
       	            resume_coroutine(receive_list_node->coroutine);
                }
	    }
	    return send_ret;
        }
    }
    errno = EINVAL;
    return -1;
}
