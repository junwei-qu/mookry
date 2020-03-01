#ifndef _LIST_H
#define _LIST_H

struct list_head {
    struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

#define INIT_LIST_HEAD(ptr) do { \
    (ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

static inline void __list_add(struct list_head *new, struct list_head *prev,	struct list_head *next) {
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

static inline void list_add_after(struct list_head *new, struct list_head *head) {
    __list_add(new, head, head->next);
}

static inline void list_add_before(struct list_head *new, struct list_head *head) {
    __list_add(new, head->prev, head);
}

static inline void __list_del(struct list_head * prev, struct list_head * next) {
    next->prev = prev;
    prev->next = next;
}

static inline void list_del(struct list_head *entry) {
    __list_del(entry->prev, entry->next);
    INIT_LIST_HEAD(entry);
}

static inline void list_move_before(struct list_head *list, struct list_head *head) {
    __list_del(list->prev, list->next);
    list_add_before(list, head);
}

static inline void list_move_after(struct list_head *list, struct list_head *head) {
    __list_del(list->prev, list->next);
    list_add_after(list, head);
}

/**
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static inline int list_empty(const struct list_head *head) {
    return head->next == head && head->prev == head;
}

/**
 * list_join - join the @list to the @head
 */
static inline void list_join(struct list_head *list, struct list_head *head) {
    if (!list_empty(list)) {
        struct list_head *first = list->next;
        struct list_head *last = list->prev;
        struct list_head *at = head->next;

        first->prev = head;
        head->next = first;

        last->next = at;
        at->prev = last;
        INIT_LIST_HEAD(list);
    }
}

#define list_entry(ptr, type, member)  container_of(ptr, type, member)

#define list_for_each(pos, head)  for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_reverse(pos, head) for (pos = (head)->prev; pos != (head); pos = pos->prev)

#define list_for_each_safe(pos, n, head) for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)

#define list_for_each_reverse_safe(pos, n, head) for (pos = (head)->prev, n = pos->prev; pos != (head); pos = n, n = pos->prev)

#define list_for_each_entry(pos, head, member) for (pos = list_entry((head)->next, typeof(*pos), member); &pos->member != (head); pos = list_entry(pos->member.next, typeof(*pos), member))

#define list_for_each_entry_reverse(pos, head, member) for (pos = list_entry((head)->prev, typeof(*pos), member); &pos->member != (head); pos = list_entry(pos->member.prev, typeof(*pos), member))

#define list_prepare_entry(pos, head, member) ((pos) ? : list_entry(head, typeof(*pos), member))

#define list_for_each_entry_continue(pos, head, member) 	for (pos = list_entry(pos->member.next, typeof(*pos), member);&pos->member != (head);pos = list_entry(pos->member.next, typeof(*pos), member))

#define list_for_each_entry_safe(pos, n, head, member) for (pos = list_entry((head)->next, typeof(*pos), member),n = list_entry(pos->member.next, typeof(*pos), member);&pos->member != (head);pos = n, n = list_entry(n->member.next, typeof(*n), member))

#define list_for_each_entry_reverse_safe(pos, n, head, member) for (pos = list_entry((head)->prev, typeof(*pos), member),n = list_entry(pos->member.prev, typeof(*pos), member);&pos->member != (head);pos = n, n = list_entry(n->member.prev, typeof(*n), member))

#endif
