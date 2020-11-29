#ifndef _BALANCE_BINARY_HEAP_H
#define _BALANCE_BINARY_HEAP_H

struct balance_binary_heap_node {
    struct balance_binary_heap_node *parent;
    struct balance_binary_heap_node *left;
    struct balance_binary_heap_node *right;
    unsigned long left_children_num;
    unsigned long right_children_num;
    struct balance_binary_heap_value *value;
};

struct balance_binary_heap_value {
    unsigned long sign;
    struct list_head list_node;
    struct balance_binary_heap_node *node;
    void *pointer;
};

struct balance_binary_heap {
    struct list_head list_head;
    struct balance_binary_heap_node *root;     
    int (*cmp_key)(void *, void *);
    struct balance_binary_heap_value* (*insert_value)(struct balance_binary_heap* heap, void *pointer);
    void (*delete_value)(struct balance_binary_heap* heap, struct balance_binary_heap_value *value);
    void *(*pop_value)(struct balance_binary_heap* heap);
    void *(*peek_value)(struct balance_binary_heap* heap);
};

struct balance_binary_heap * alloc_heap(int (*cmp_key)(void *, void *));
void free_heap(struct balance_binary_heap *heap);

#endif
