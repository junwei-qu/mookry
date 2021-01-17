#include <stdio.h>
#include <mookry/coroutine.h>

void routine1(void *arg){
    char buf[100];
    int64_t channel_id = channel_open("/test-channel", 100, 100);
    int ret = channel_send(channel_id, "hello", 6, -1);
}

void routine2(void *arg){
    char buf[100];
    int64_t channel_id = channel_open("/test-channel", 100, 100);
    int ret = channel_receive(channel_id, buf, sizeof(buf), -1);
    printf("receive data: %s\n", buf);
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
