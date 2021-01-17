#include <stdio.h>
#include <mookry/coroutine.h>

void start_routine(void *arg){
    printf("I'm the start routine!\n");
}

void routine(void *arg){
    make_coroutine(0, start_routine, NULL);
}

int
main(int argc, char **argv){
    enter_coroutine_environment(routine, NULL);
    return 0;
}
