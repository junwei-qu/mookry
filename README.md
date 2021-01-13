# The purpose of mookry is to make high performance programming for network become easy， look forward to your contribution to the project!
# Requirements: linux os, x86-64, kernel version >= 2.6.17
# Install
  make<br/>
  make install
# Uninstall
  make uninstall
# API:
### void make_coroutine(uint32_t stack_size, void(*routine)(void *), void *arg);
  Create Coroutine<br/>
  stack_size: the stack size of coroutine when created<br/>
  routine: the routine will be run when coroutine start<br/>
  arg: arg will be passed to the routine
 
### ssize_t co_write(int fd, const void *buf, size_t count, double  timeout);
  requirement: fd must be nonblock<br/>
  used in coroutine environment, current coroutine will be yielded automaticly when writing not available and resumed when writing available or timeout. Please refer to the write syscall for its synopsis
###  ssize_t co_read(int fd, void *buf, size_t count, double timeout);
  requirement: fd must be nonblock<br/>
  used in coroutine environment, current coroutine will be yielded automaticly when reading not available and resumed when reading available or timeout. Please refer to the read syscall for its synopsis
###  int co_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
  requirement: sockfd must be nonblock<br/>
  used in coroutine environment, current coroutine will be yielded automaticly when connecting can't be finished immediately and resumed when connecting finished. Please refer to the connect syscall for its synopsis
### int co_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
  requirement: sockfd must be nonblock<br/>
  used in coroutine environment, current coroutine will be yielded automaticly when no connections to be accepted and resumed when connections can be accetped. Please refer to the accept syscall for its synopsis
### int co_accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);
  requirement: sockfd must be nonblock<br/>
  used in coroutine environment, current coroutine will be yielded automaticly when no connections to be accepted and resumed when connections can be accetped. Please refer to the accept4 syscall for its synopsis
###  void co_sleep(double seconds);
  used in coroutine environment, current coroutine will be yielded and resumed after some seconds.
###  void co_add_signal(int signo, void(*handler)(int signo, void *arg), void *arg);
  the handler will be executed in coroutine environment when the signo catched.
### void co_remove_signal(int signo);
  remove the handler of signo.
### int64_t channel_open(char *name, int msgsize, int maxmsg);
   open or create channel when the channel identified by name doesn't exist.<br/>
   return: return the channel_id for the current coroutine, and can be used only in the current coroutine.<br/>
   name: identify the channel.<br/>
   msgsize: specify the max size of message that can be sended to the channel.<br/>
   maxmsg: specify the max number of messages that can be sended to the channel.<br/>
### int channel_send(int64_t channel_id, const char *msg_ptr, size_t msg_len, double timeout);
   send message to channel_id returned by channel_open.<br/>
   channel_id: returned by the channel_open.<br/>
   msg_ptr: the address of message to be sended to the channel.<br/>
   msg_len: the length of message which must be less than or equal to the msgsize.<br/>
   timeout: the seconds to wait when the channel is full. if timeout is 0, it will return immediately when channel is full, if timeout is less than 0, the channel will wait until the channal has space for message.<br/>
### int channel_receive(int64_t channel_id, char *msg_ptr, size_t msg_len, double timeout);
   receive message from channel_id returned by channel_open.<br/>
   channel_id: returned by the channel_open.<br/>
   msg_ptr: the address of buffer used to save the message which will be received from the channel.<br/>
   msg_len: the length of buffer which must be greater than or equal to the msgsize.<br/>
   timeout: the seconds to wait when the channel is empty. if timeout is 0, it will return immediately when channel is empty, if timeout is less than 0, the channel will wait until the channal has data to receive.<br/>
### void channel_unlink(char *name);
   unlink the channel identified by the name.
### void channel_close(int64_t channel_id);
   close the channel for the current coroutine.
   
### examples
#### co_sleep
```
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
```
#### tcp echo server
```
#include <mookry/coroutine.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>

void reader_writer(void *arg){
    long fd = (long)arg;
    char buf[20];
    int n;
    while(1){
        n = co_read(fd, buf, sizeof(buf), 5);
        if(n > 0){
            co_write(fd, buf, n, -1);
        } else if(n <= 0){
            close(fd);
            return;
        }
    }
}

void sig_pipe(int signo, void *arg){
    printf("sig_pipe\n");
}

void co_start(void *arg){
    struct sockaddr_in servaddr;
    int fd;
    int reuse_addr = 1;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(1234);
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr ,sizeof(reuse_addr));
    bind(fd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    listen(fd, 10);
    co_add_signal(SIGPIPE, sig_pipe, NULL);
    while(1){
        long sockfd = co_accept4(fd, NULL, NULL, SOCK_NONBLOCK);
        if(sockfd > 0){
            make_coroutine(0, reader_writer, (void *)sockfd);
        } else {
            perror("accept error");
        }
    }
}

int
main(int argc, char **argv){
    enter_coroutine_environment(co_start, NULL);
    return 0;
}
```
