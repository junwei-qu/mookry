#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include "balance_binary_heap.h"
int cmp_key(const void *arg1, const void *arg2){
    if(*(int *)arg1 > *(int *)arg2){
        return 1;
    } else if(*(int *)arg1 < *(int *)arg2){
        return -1;
    } else {
        return 0;
    }
}
int max_tree_height1(struct balance_binary_heap_node *root){
    int left_height, right_height;
    left_height = right_height = 0;
    if(root->left){
        left_height = max_tree_height1(root->left);
    }
    if(root->right){
        right_height = max_tree_height1(root->right);
    }
    if(right_height > left_height){
        return right_height + 1;
    } else {
        return left_height + 1;
    }
}
int min_tree_height1(struct balance_binary_heap_node *root){
    int left_height, right_height;
    left_height = right_height = 0;
    if(root->left){
        left_height = min_tree_height1(root->left);
    }
    if(root->right){
        right_height = min_tree_height1(root->right);
    }
    if(right_height > left_height){
        return left_height + 1;
    } else {
        return right_height + 1;
    }
}

int max_tree_height2(struct balance_binary_heap_node *root){
    int height = 0;
    while(root){
        if(root->left_children_num > root->right_children_num){
            root = root->left;
        } else {
            root = root->right;
	}
	height++;
    }
    return height;
}
int min_tree_height2(struct balance_binary_heap_node *root){
    int height = 0;
    while(root){
        if(root->left_children_num > root->right_children_num){
            root = root->right;
        } else {
            root = root->left;
	}
	height++;
    }
    return height;
}
int children_num(struct balance_binary_heap_node *root){
    int left_children_num, right_children_num;
    left_children_num = right_children_num = 0;
    if(root->left){
        left_children_num = children_num(root->left)+1;
    }
    if(root->right){
        right_children_num = children_num(root->right)+1;
    }
    return left_children_num + right_children_num;
}
int left_children_num(struct balance_binary_heap_node *root){
    if(root->left){
        return children_num(root->left)+1;
    } else {
        return 0;
    }
}
int right_children_num(struct balance_binary_heap_node *root){
    if(root->right){
        return children_num(root->right)+1;
    } else {
        return 0;
    }
}
int
main(int argc, char **argv){
   struct balance_binary_heap * heap = alloc_heap(cmp_key);
   srand(time(NULL));
   int random_num = 20000000;
   int * array = malloc(sizeof(int) * random_num);
   struct balance_binary_heap_value ** array_pointer= malloc(sizeof(struct balance_binary_heap_value *) * random_num);
   int * array_bak = malloc(sizeof(int) * random_num);
   int delete_count = 0;
   for(int i = 0; i < random_num; i++){
       array[i] = rand();
       array_bak[i] = array[i];
       array_pointer[i] = heap->insert_value(heap, array+i);
       if(i != 0 && i % 100 == 0){
           int index = rand() % (i+1);
	   if(array_pointer[index]){
               heap->delete_value(heap, array_pointer[index]);
	       array_pointer[index] = NULL;
	       array[index] = -1;
	       array_bak[index] = -1;
	       delete_count++;
	   }
       }
   }
   printf("delete count: %d\n", delete_count);
   printf("left count: %d\n", random_num-delete_count);
   printf("max_tree_height1: %d\n", max_tree_height1(heap->root));
   printf("max_tree_height2: %d\n", max_tree_height2(heap->root));
   printf("min_tree_height1: %d\n", min_tree_height1(heap->root));
   printf("min_tree_height2: %d\n", min_tree_height2(heap->root));
   struct balance_binary_heap_value *pos, *next;
   int count = 0;
   list_for_each_entry_safe(pos, next, &(heap->list_head), list_node){
        if(left_children_num(pos->node) != pos->node->left_children_num){
            printf("occurs error\n");
        }
        if(right_children_num(pos->node) != pos->node->right_children_num){
            printf("occurs error\n");
        }
        count++;
   }
   printf("traverse count: %d\n", count);
   int time_start = time(NULL);
   qsort(array_bak, random_num, sizeof(int), cmp_key);
   int time_end = time(NULL);
   printf("qsort cost time: %d(s)\n", time_end-time_start);
   struct timeval start, end;
   long diff1,diff2;
   int max_diff, min_diff;
   max_diff = 0;
   min_diff = 999999999;
   int skip_count = 0;
   for(int i = random_num-1; i >= 0; i--){
       if(array_bak[i] == -1){
           skip_count++;
           continue;
       }
       gettimeofday(&start, NULL);
       if(array_bak[i] != *(int *)heap->pop_value(heap)){
           printf("occurs error\n");
       }
       gettimeofday(&end, NULL);
       diff1 = end.tv_sec - start.tv_sec;
       diff2 = end.tv_usec - start.tv_usec;
       if(diff2 < 0){
           diff1 -= 1;
	   diff2 += 1000000;
       }
       diff2 = diff1 * 1000000 + diff2;
       if(diff2 > max_diff){
           max_diff = diff2;
       }
       if(diff2 < min_diff){
           min_diff = diff2;
       }
   }
   printf("max_diff: %d(us), min_diff: %d(us)\n", max_diff, min_diff);
   printf("skip_count: %d\n", skip_count);
   printf("pop_value: %p\n", heap->pop_value(heap));
   return 0;
}
