#include <stdio.h>
#include <mookry/coroutine.h>

void start_routine(void *arg){
    printf("I'm the start routine!\n");
}

void routine(void *arg){
    co_make(0, start_routine, NULL);
}

int
main(int argc, char **argv){
    co_env(routine, NULL);
    return 0;
}
