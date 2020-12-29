#include <sys/mman.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include "coroutine.h"
#include "event_loop.h"

struct coroutine {
    void (*routine)(void *arg);
    void *arg;
    void *stack_pointer;
    int stack_size;
    void *mem_base;
    int mem_size;
};

struct event_loop *main_event_loop;
struct coroutine  main_coroutine;
struct coroutine  *cur_coroutine = &main_coroutine;
uint64_t coroutine_count = 0;

void *make_fcontext(void *sp, int size, void(*routine)(struct coroutine *coroutine));
void *jump_fcontext(void **old_sp, void *new_sp, struct coroutine *coroutine, int preserve_fpu);
static inline void routine_start(struct coroutine *coroutine);
static inline int defer_destroy_coroutine(struct event_loop *ev, void *coroutine);
static inline void destroy_coroutine(struct coroutine *coroutine);
static inline struct coroutine *resume_coroutine(struct coroutine *coroutine);
static inline struct coroutine *yield_coroutine();
static inline void reader_writer_callback(struct event_loop *ev, int fd, int event_type, void *coroutine);
static inline int sleep_callback(struct event_loop *ev, uint64_t timer_id, void *coroutine);

int enter_coroutine_environment(void (*co_start)(void *), void *arg);
void make_coroutine(uint32_t stack_size, void(*routine)(void *), void *arg);
ssize_t co_write(int fd, const void *buf, size_t count);
ssize_t co_read(int fd, void *buf, size_t count);
int co_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int co_accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);
void co_sleep(float seconds);

static inline void routine_start(struct coroutine *coroutine){
    coroutine->routine(coroutine->arg);
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

static inline struct coroutine *resume_coroutine(struct coroutine *coroutine){
    assert((cur_coroutine == &main_coroutine) && (cur_coroutine != coroutine));
    cur_coroutine = coroutine;
    return jump_fcontext(&(main_coroutine.stack_pointer), cur_coroutine->stack_pointer, cur_coroutine, 1);
}

static inline struct coroutine *yield_coroutine(){
    assert(cur_coroutine != &main_coroutine);
    struct coroutine *tmp = cur_coroutine;
    cur_coroutine = &main_coroutine;
    return jump_fcontext(&(tmp->stack_pointer), cur_coroutine->stack_pointer, cur_coroutine, 1);
}

static inline void reader_writer_callback(struct event_loop *ev, int fd, int event_type, void *coroutine){
    resume_coroutine(coroutine);
}

static inline int sleep_callback(struct event_loop *ev, uint64_t timer_id, void *coroutine){
    resume_coroutine(coroutine);
    return 0;
}

int enter_coroutine_environment(void (*co_start)(void *), void *arg){
    assert(!main_event_loop);
    int ret = 0;
    main_event_loop = alloc_event_loop();
    make_coroutine(0, co_start, arg);
    while(coroutine_count && ret >= 0){
        ret = main_event_loop->poll(main_event_loop, -1);
    }
    free_event_loop(main_event_loop);
    main_event_loop = NULL;
    return ret;
}

void make_coroutine(uint32_t stack_size, void(*routine)(void *), void *arg){
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
    mprotect(mem_base, page_size, PROT_NONE);
    struct coroutine *coroutine = (struct coroutine *)((char *)mem_base + map_size - sizeof(struct coroutine));
    coroutine->mem_base = mem_base;
    coroutine->mem_size = map_size;
    coroutine->stack_size = stack_size;
    coroutine->routine = routine;
    coroutine->arg = arg;
    coroutine->stack_pointer = make_fcontext((char *)coroutine - 1, stack_size, routine_start);
    struct coroutine *tmp = cur_coroutine;
    cur_coroutine = coroutine;
    coroutine_count += 1;
    jump_fcontext(&(tmp->stack_pointer), cur_coroutine->stack_pointer, cur_coroutine, 1);
}

ssize_t co_write(int fd, const void *buf, size_t count){
    assert(main_event_loop);
    int ret;
    loop:
    ret = main_event_loop->write(main_event_loop, fd, buf, count);
    if(ret == -1 && errno == EAGAIN){
	main_event_loop->add_writer(main_event_loop, fd, reader_writer_callback, cur_coroutine);
	yield_coroutine();
	goto loop;
    }
    return ret;
}

ssize_t co_read(int fd, void *buf, size_t count){
    assert(main_event_loop);
    int ret;
    loop:
    ret = main_event_loop->read(main_event_loop, fd, buf, count);
    if(ret == -1 && errno == EAGAIN){
	main_event_loop->add_reader(main_event_loop, fd, reader_writer_callback, cur_coroutine);
	yield_coroutine();
	goto loop;
    }
    return ret;
}

int co_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen){
    assert(main_event_loop);
    int ret;
    loop:
    ret = main_event_loop->accept(main_event_loop, sockfd, addr, addrlen);
    if(ret == -1 && errno == EAGAIN){
	main_event_loop->add_reader(main_event_loop, sockfd, reader_writer_callback, cur_coroutine);
	yield_coroutine();
	goto loop;
    }
    return ret;
}

int co_accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags){
    assert(main_event_loop);
    int ret;
    loop:
    ret = main_event_loop->accept4(main_event_loop, sockfd, addr, addrlen, flags);
    if(ret == -1 && errno == EAGAIN){
	main_event_loop->add_reader(main_event_loop, sockfd, reader_writer_callback, cur_coroutine);
	yield_coroutine();
	goto loop;
    }
    return ret;
}

void co_sleep(float seconds){
    assert(main_event_loop);
    int integer_seconds = (int)(seconds);
    long nano_seconds = (long)((seconds - integer_seconds)* 1000000000);
    struct timespec ts;
    ts.tv_sec = integer_seconds;
    ts.tv_nsec = nano_seconds;
    main_event_loop->add_timer(main_event_loop, &ts, sleep_callback, cur_coroutine);
    yield_coroutine();
}
