#include <stdlib.h>
#include "future.h"
#include "event_loop.h"

static void future_init(future *future, event_loop *ev);
static void future_destruct(future *future);
static int future_cancel(future *future);
static int future_canceled(future *future);
static int future_done(future *future);
static void * future_get_result(future *future, future_error *error);
static future_error future_get_error(future *future);
static int future_set_error(future *future, future_error error, future_error *errinfo);
future * alloc_future(event_loop *ev);
void free_future(future *future);

future * alloc_future(event_loop *ev){
    future *future = calloc(1,sizeof(future));
    future->init = future_init;
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
}

static int future_cancel(future *future){
    if(future->state != FUTURE_STATE_PENDING){
        return 0;
    }
    future->state = FUTURE_STATE_CANCELED;
    return 1;
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

static void * future_get_result(future *future, future_error *error){
    if(future->state == FUTURE_STATE_CANCELED){
        *error = FUTURE_ERROR_CANCELED;
        return NULL;
    }
    if(future->state != FUTURE_STATE_FINISHED){
        *error = FUTURE_ERROR_INVALID_STATE;
	return NULL;
    }
    if(future->error != FUTURE_ERROR_NOERROR){
        *error = future->error;
	return NULL;
    }
    return future->result;
}

static future_error future_get_error(future *future){
    return future->error;
}

static int future_set_error(future *future, future_error error, future_error *errinfo){
    if(future->state != FUTURE_STATE_PENDING){
        *errinfo = FUTURE_ERROR_INVALID_STATE; 
        return 0; 
    }
    future->error = error;
    future->state = FUTURE_STATE_FINISHED;
}
