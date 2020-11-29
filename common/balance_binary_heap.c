#include <stdlib.h>
#include "list.h"
#include "balance_binary_heap.h"
#define BALANCE_BINARY_HEAP_SIGN 0x11223344

struct balance_binary_heap_value* heap_insert_value(struct balance_binary_heap *heap, void *pointer);
void heap_delete_value(struct balance_binary_heap *heap, struct balance_binary_heap_value *value);
void *heap_pop_value(struct balance_binary_heap *heap);
void *heap_peek_value(struct balance_binary_heap *heap);

struct balance_binary_heap * alloc_heap(int (*cmp_key)(void *, void *)){
    struct balance_binary_heap *heap = malloc(sizeof(struct balance_binary_heap));
    INIT_LIST_HEAD(&(heap->list_head));
    heap->root = NULL;
    heap->cmp_key = cmp_key;
    heap->insert_value = heap_insert_value;
    heap->delete_value = heap_delete_value;
    heap->pop_value = heap_pop_value;
    heap->peek_value = heap_peek_value;
}

void free_heap(struct balance_binary_heap *heap){
    if(heap->root){
        struct balance_binary_heap_value *pos, *next;
        list_for_each_entry_safe(pos, next, &(heap->list_head), list_node){
            free(pos->node);
	    free(pos);
	}
    }
    free(heap);
}

struct balance_binary_heap_value* heap_insert_value(struct balance_binary_heap *heap, void *pointer) {
    struct balance_binary_heap_node* node = malloc(sizeof(struct balance_binary_heap_node));
    struct balance_binary_heap_value* value = malloc(sizeof(struct balance_binary_heap_value));
    value->pointer = pointer;
    value->sign = BALANCE_BINARY_HEAP_SIGN;
    node->parent = node->left = node->right = NULL;
    node->left_children_num = node->right_children_num = 0;
    list_add_after(&(value->list_node), &(heap->list_head));
    if(heap->root == NULL) {
	node->value = value;
	value->node = node;
        heap->root = node;
	return value;
    } else {
        struct balance_binary_heap_node *parent_node = heap->root; 
	struct balance_binary_heap_node *child_node = NULL; 
	struct balance_binary_heap_value *parent_value, *tmp_value;
	int is_left_node = -1;
        while(heap->cmp_key(parent_node->value->pointer, value->pointer) >= 0){
	    if(parent_node->left_children_num <= parent_node->right_children_num){
	        parent_node->left_children_num++;
                child_node = parent_node->left;
		is_left_node = 1;
	    } else {
	        parent_node->right_children_num++;
                child_node = parent_node->right;
		is_left_node = 0;
	    }
	    if(child_node){
                parent_node = child_node;
	    } else {
                break;
	    }
	}
        if(is_left_node != -1 && !child_node){
	    node->parent = parent_node;
	    node->value = value;
	    value->node = node;
            if(is_left_node){
                parent_node->left = node;
	    } else {
                parent_node->right = node;
	    }
	    return value;
	} 
	parent_value = parent_node->value;
	parent_value->node = NULL;
	parent_node->value = value;
	value->node = parent_node;
	for(;;){
	    if(parent_node->left_children_num <= parent_node->right_children_num){
	        parent_node->left_children_num++;
                child_node = parent_node->left;
		is_left_node = 1;
	    } else {
	        parent_node->right_children_num++;
                child_node = parent_node->right;
		is_left_node = 0;
	    }
	    if(child_node){
	        tmp_value = child_node->value;
		tmp_value->node = NULL;
	        child_node->value = parent_value;
		parent_value->node = child_node;
		parent_value = tmp_value;
                parent_node = child_node;
	    } else {
	        node->parent = parent_node;
                node->value = parent_value;
		node->value->node = node;
		if(is_left_node){
	            parent_node->left = node;
		} else {
	            parent_node->right = node;
		}
		return value;
	    }
	}
    }
}

void heap_delete_value(struct balance_binary_heap *heap, struct balance_binary_heap_value *value) {
    struct balance_binary_heap_node *node = value->node;
    struct balance_binary_heap_node *max_node, *parent_node, *delete_node = heap->root; 
    struct balance_binary_heap_node *child_node = NULL; 
    struct balance_binary_heap_value *tmp_value, *delete_value = NULL;
    int is_left_node = -1;
    if(!heap->root || value->sign != BALANCE_BINARY_HEAP_SIGN){
        return;
    }
    list_del(&(value->list_node));
    while(delete_node->left_children_num || delete_node->right_children_num){
        if(delete_node->left_children_num > delete_node->right_children_num){
	    delete_node->left_children_num--;
            child_node = delete_node->left;
	} else {
	    delete_node->right_children_num--;
            child_node = delete_node->right;
        }
        delete_node = child_node;
    }
    parent_node = delete_node->parent;
    delete_value = delete_node->value;
    free(delete_node);
    free(value);
    if(!parent_node){
        heap->root = NULL;
	return;
    }
    if(delete_node == node){
        if(parent_node->left == delete_node) {
            parent_node->left = NULL;
	} else {
            parent_node->right = NULL;
	}
	return;
    }
    node->value = delete_value;
    if(node->parent && heap->cmp_key(node->value->pointer, node->parent->value->pointer) > 0){
        for(;;){
            max_node = node;
            if(node->parent && heap->cmp_key(max_node->value->pointer, node->parent->value->pointer) < 0){
                max_node = node->parent;
            }
            if(max_node == node->parent){
                return;
            } else {
                tmp_value = node->value;
    	        node->value = node->parent->value;
    	        node->parent->value = tmp_value;
    	        node = node->parent;
            }
        }
    } else {
        for(;;){
            max_node = node;
            if(node->left && heap->cmp_key(max_node->value->pointer, node->left->value->pointer) < 0){
                max_node = node->left;
                is_left_node = 1;
            }
            if(node->right && heap->cmp_key(max_node->value->pointer, node->right->value->pointer) < 0){
                max_node = node->right;
                is_left_node = 0;
            }
            if(max_node == node){
                return;
            } else if(is_left_node){
                tmp_value = node->value;
                node->value = node->left->value;
        	node->left->value = tmp_value;
        	node = node->left;
            } else {
                tmp_value = node->value;
        	node->value = node->right->value;
        	node->right->value = tmp_value;
        	node = node->right;
            }
        }
    }
}

void *heap_pop_value(struct balance_binary_heap *heap){
    if(!heap->root){
        return NULL;
    }
    void *pointer = heap->root->value->pointer;
    heap->delete_value(heap, heap->root->value);
    return pointer;
}

void *heap_peek_value(struct balance_binary_heap *heap){
    if(!heap->root){
        return NULL;
    }
    return heap->root->value->pointer;
}
