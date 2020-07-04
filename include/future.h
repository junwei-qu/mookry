#ifndef   _FUTURE_H
#define  _FUTURE_H
#include "event_loop.h"
#include "list.h"

typedef enum future_state {
    FUTURE_STATE_PENDING,
    FUTURE_STATE_CANCELED,
    FUTURE_STATE_FINISHED
} future_state;

typedef enum future_error {
    FUTURE_ERROR_NOERROR,
    FUTURE_ERROR_INVALID_STATE,
} future_error;

typedef struct future {
    struct list_head callback_head;
    future_state state;
    future_error error;
    void *result;
    event_loop *loop;
    void (*init)(struct future *, event_loop *);
    void (*destruct)(struct future *);
    int (*cancel)(struct future *);
    int (*canceled)(struct future *);
    int (*done)(struct future *);
    void (*add_done_callback)(struct future *, void (*)(struct future *, void *));
    void (*remove_done_callback)(struct future *, void (*)(struct future*, void *));
    int (*set_result)(struct future *, void *);
    void *(*get_result)(struct future *);
    void (*set_error)(struct future *, future_error);
    future_error (*get_error)(struct future *);
} future;

future *alloc_future(event_loop *ev);
void free_future(future *future);

#endif
