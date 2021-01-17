#include <stdio.h>
#include <mookry/coroutine.h>

void routine(void *arg){
    printf("I'm the routine!\n");
}

int
main(int argc, char **argv){
    enter_coroutine_environment(routine, NULL);
    return 0;
}
