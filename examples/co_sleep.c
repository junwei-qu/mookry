#include <stdio.h>
#include <time.h>
#include <mookry/coroutine.h>

void routine1(void *arg){
    while(1){
        printf("routine1 time: %ld\n", time(NULL));
        co_sleep(1);
    }
}

void routine2(void *arg){
    while(1){
        printf("routine2 time: %ld\n", time(NULL));
        co_sleep(1);
    }
}

void co_start(void *arg){
    make_coroutine(0, routine1, NULL);
    make_coroutine(0, routine2, NULL);
}

int
main(int argc, char **argv){
    enter_coroutine_environment(co_start, NULL);
    return 0;
}
