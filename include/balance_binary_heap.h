#ifndef _BALANCE_BINARY_HEAP_H
#define _BALANCE_BINARY_HEAP_H

#include <stdint.h>
#include "list.h"

struct balance_binary_heap_node {
    struct balance_binary_heap_node *parent;
    struct balance_binary_heap_node *left;
    struct balance_binary_heap_node *right;
    uint64_t left_children_num;
    uint64_t right_children_num;
    struct balance_binary_heap_value *value;
};

struct balance_binary_heap_value {
    unsigned long sign;
    struct balance_binary_heap_node *node;
    void *pointer;
};

struct balance_binary_heap {
    struct balance_binary_heap_node *root;     
    int (*cmp_key)(const void *, const void *);
    struct balance_binary_heap_value* (*insert_value)(struct balance_binary_heap* heap, void *pointer);
    void (*delete_value)(struct balance_binary_heap* heap, struct balance_binary_heap_value *value);
    void (*heapify)(struct balance_binary_heap *heap, struct balance_binary_heap_value *value);
    void *(*pop_value)(struct balance_binary_heap* heap);
    void *(*peek_value)(struct balance_binary_heap* heap);
};

struct balance_binary_heap * alloc_heap(int (*cmp_key)(const void *, const void *));
void free_heap(struct balance_binary_heap *heap);

#endif
