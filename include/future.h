#ifndef   _FUTURE_H
#define  _FUTURE_H
#include "event_loop.h"
#include "list.h"

enum future_state {
    FUTURE_STATE_PENDING,
    FUTURE_STATE_CANCELED,
    FUTURE_STATE_FINISHED
};

enum future_error {
    FUTURE_ERROR_NOERROR,
    FUTURE_ERROR_INVALID_STATE,
};

struct future {
    struct list_head callback_head;
    enum future_state state;
    enum future_error error;
    void *result;
    struct event_loop *loop;
    void (*init)(struct future *, struct event_loop *);
    void (*destruct)(struct future *);
    int (*cancel)(struct future *);
    int (*canceled)(struct future *);
    int (*done)(struct future *);
    void (*add_done_callback)(struct future *, void (*)(struct future *, void *), void *);
    void (*remove_done_callback)(struct future *, void (*)(struct future*, void *));
    int (*set_result)(struct future *, void *);
    void *(*get_result)(struct future *);
    void (*set_error)(struct future *, enum future_error);
    enum future_error (*get_error)(struct future *);
};

struct future *alloc_future(struct event_loop *ev);
void free_future(struct future *future);

#endif
