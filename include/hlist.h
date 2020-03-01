#ifndef _HLIST_H
#define _HLIST_H

struct hlist_head {
    struct hlist_node *first;
};

struct hlist_node {
    struct hlist_node *next, **pprev;
};

#define HLIST_HEAD_INIT { .first = NULL }
#define HLIST_HEAD(name) struct hlist_head name = {  .first = NULL }
#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)
#define INIT_HLIST_NODE(ptr) ((ptr)->next = NULL, (ptr)->pprev = NULL)

static inline int hlist_unhashed(const struct hlist_node *h) {
    return !h->pprev;
}

static inline int hlist_empty(const struct hlist_head *h) {
    return !h->first;
}

static inline void hlist_del(struct hlist_node *n) {
    struct hlist_node *next = n->next;
    struct hlist_node **pprev = n->pprev;
    *pprev = next;
    if (next)
        next->pprev = pprev;
    INIT_HLIST_NODE(n);
}

static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h) {
    struct hlist_node *first = h->first;
    n->next = first;
    if (first)
        first->pprev = &n->next;
    h->first = n;
    n->pprev = &h->first;
}

#define hlist_entry(ptr, type, member) container_of(ptr,type,member)

#define hlist_for_each(pos, head) for (pos = (head)->first;pos;pos = pos->next)

#define hlist_for_each_safe(pos, n, head) for (pos = (head)->first; pos && ({ n = pos->next; 1; });pos = n)

#define hlist_for_each_entry(tpos, pos, head, member) for (pos = (head)->first;pos && ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;});pos = pos->next)

#define hlist_for_each_entry_continue(tpos, pos, member) for (pos = (pos)->next;pos && ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;});pos = pos->next)

#define hlist_for_each_entry_from(tpos, pos, member) for (; pos && ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;});pos = pos->next)

#define hlist_for_each_entry_safe(tpos, pos, n, head, member) 	for (pos = (head)->first;pos && ({ n = pos->next; 1; }) && ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;});pos = n)

#endif
