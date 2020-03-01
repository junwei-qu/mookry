#include "future.h"
void future_init(struct future *future, struct event_loop *ev){
    future->loop = ev;
    future->state = FUTURE_STATE_PENDING;
    future->error = FUTURE_ERROR_NOERROR;
    future->result = NULL;
}

int future_cancel(struct future *future){
    if(future->state != FUTURE_STATE_PENDING){
        return 0;
    }
    future->state = FUTURE_STATE_CANCELED;
    return 1;
}

int future_canceled(struct future *future){
    if(future->state == FUTURE_STATE_CANCELED){
        return 1;
    } else {
        return 0;
    }
}

int future_done(struct future *future){
    if(future->state != FUTURE_STATE_PENDING){
        return 1;
    } else {
        return 0;
    }
}

void * future_get_result(struct future *future, enum future_error *error){
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

enum future_error future_get_error(struct future *future){
    return future->error;
}

int future_set_error(struct future *future, enum future_error error, enum future_error *errinfo){
    if(future->state != FUTURE_STATE_PENDING){
        *errinfo = FUTURE_ERROR_INVALID_STATE; 
        return 0; 
    }
    future->error = error;
    future->state = FUTURE_STATE_FINISHED;
}
