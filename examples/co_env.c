#include <stdio.h>
#include <mookry/coroutine.h>

void routine(void *arg){
    printf("I'm the routine!\n");
}

int
main(int argc, char **argv){
    co_env(routine, NULL);
    return 0;
}
