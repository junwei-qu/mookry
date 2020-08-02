#include <stdlib.h>
#include "future.h"
#include "event_loop.h"
#include "list.h"

future * alloc_future(event_loop *ev);
void free_future(future *future);
static void future_init(future *future, event_loop *ev);
static void future_destruct(future *future);
static int future_cancel(future *future);
static int future_canceled(future *future);
static int future_done(future *future);
static void * future_get_result(future *future);
static int future_set_result(future *future ,void *result);
static future_error future_get_error(future *future);
static void future_set_error(future *future, future_error error);
static void future_add_done_callback(future *future, void (*callback)(struct future *, void *), void *arg);
static void future_remove_done_callback(future *future, void (*callback)(struct future*, void *));

struct done_callback_entry {
    struct list_head list_head;
    void (*callback)(struct future *, void*); 
    void *arg;
};

future * alloc_future(event_loop *ev){ 
    future *future = calloc(1,sizeof(future)); 
    future->init = future_init;
    future->destruct = future_destruct; 
    future->cancel = future_cancel;
    future->canceled = future_canceled; 
    future->done = future_done;
    future->get_result = future_get_result; 
    future->set_result = future_set_result; 
    future->get_error = future_get_error;
    future->set_error = future_set_error; 
    future->add_done_callback = future_add_done_callback; 
    future->remove_done_callback = future_remove_done_callback; 
    future->init(future, ev); 
    return future; 
}

void free_future(future *future){ 
    future->destruct(future);
    free(future); 
}

static void future_init(future *future, event_loop *ev){
    future->loop = ev; 
    future->state = FUTURE_STATE_PENDING;
    future->error = FUTURE_ERROR_NOERROR; 
    future->result = NULL;
    INIT_LIST_HEAD(&future->callback_head); 
}

static void future_destruct(future *future) {
    struct done_callback_entry *pos, *tmp;
    list_for_each_entry_safe(pos,tmp,&future->callback_head,list_head) {
        free(pos);
    }
    
}

static int future_cancel(future *future){
    if(future->state != FUTURE_STATE_PENDING){
        future->set_error(future, FUTURE_ERROR_INVALID_STATE); 
	return -1; 
    } 
    future->state = FUTURE_STATE_CANCELED; 
    return 0; 
}

static int future_canceled(future *future){ 
    if(future->state == FUTURE_STATE_CANCELED){
        return 1; 
    } else { 
        return 0; 
    } 
}

static int future_done(future *future){ 
    if(future->state != FUTURE_STATE_PENDING){ 
        return 1; 
    } else { 
        return 0; 
    } 
}

static void * future_get_result(future *future){
    if(!future->done(future)){ 
        future->set_error(future, FUTURE_ERROR_INVALID_STATE); 
	return NULL; 
    }
    return future->result;
}

static int future_set_result(future *future, void *result){
    if(future->done(future)){ 
        future->set_error(future, FUTURE_ERROR_INVALID_STATE); 
	return -1; 
    } 
    future->result = result;
    struct done_callback_entry *pos;
    list_for_each_entry(pos,&future->callback_head,list_head){
        pos->callback(future,pos->arg);
    }
    return 0; 
}

static future_error future_get_error(future *future){ 
    return future->error; 
}

static void future_set_error(future *future, future_error error){
    future->error = error; 
}

static void future_add_done_callback(future *future, void (*callback)(struct future *, void *), void *arg) {
    struct done_callback_entry *entry = calloc(1, sizeof(struct done_callback_entry));
    entry->callback = callback;
    entry->arg = arg;
    list_add_before(&future->callback_head, &entry->list_head);
}

static void future_remove_done_callback(future *future, void (*callback)(struct future*, void *)) {
    struct done_callback_entry *pos, *tmp;
    list_for_each_entry_safe(pos,tmp,&future->callback_head,list_head) {
        if(pos->callback == callback){
	    list_del(&pos->list_head);
            break;
	}
    }
}
