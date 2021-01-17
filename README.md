# The purpose of mookry is to make high performance programming for network become easy, look forward to your contribution to the project!
---
## 1. Requirements: linux os, x86-64, kernel version >= 2.6.17
## 2. Install
- make
- make install  
This will copy libmookry.so into /usr/lib64 directory and header files into /usr/include/mookry.
## 3. Uninstall
- make uninstall
## 4. API
- ### enter_coroutine_environment(void(*routine)(void *), void *arg);  
**DESCRIPTION**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;**enter_coroutine_environment()** initializes the environment where coroutine executes. The **routine** is invoked after coroutine enviroment initialized, and **arg** is passed as the sole argument of **routine()**.<br/><br/>
**RETURN VALUE**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;On success, **enter_coroutine_environment()** returns 0; On error, -1 is returned, and errno is set appropriately.<br/><br/>
**EXAMPLES**
```
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
```
- ### void make_coroutine(uint32_t stack_size, void(*routine)(void *), void *arg);  
**DESCRIPTION**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;**make_coroutine()** creates a coroutine whose stack size is **stack_size**. If **stack_size** is 0, the default size 2M is alloced. The new coroutine starts execution by  invoking **routine(); arg** is passed as the sole argument of **routine()**.<br/><br/>
**RETURN VALUE**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;On success, **make_coroutine()** return 0; On error, -1 is returned, and errno is set appropriately.<br/><br/>
**ERRORS**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;**ENOMEM** No memory is available.<br/><br/>
**EXAMPLES**
```
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
```
- ### ssize_t co_write(int sockfd, const void *buf, size_t count, double timeout);
**DESCRIPTION**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;**co_write()** writes up to **count** bytes from the buffer starting at **buf** to the file referred to by the file descriptor **sockfd**. The number of bytes written may be less than **count** if, for example, there is insufficient space on the underlying physical medium, or the RLIMIT_FSIZE resource limit is  encountered, or the call was interrupted by a signal handler after having written less than **count**  bytes. If the **timeout** is 0, ** co_write** returns immediately when there is insufficient space on the underlying physical medium. If **timeout** is less than 0, **co_write** will be yielded automatically when writing not available and resumed when writing available. If **timeout** is greater than 0, it will return 0 when writing not available after **timeout** seconds.<br/><br/>
**RETURN VALUE**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;On success, the number of bytes written is returned.  On error, -1 is returned, and errno is set to indicate the cause of the error. On timeout, 0 is returned.
- ### ssize_t co_read(int fd, void *buf, size_t count, double timeout);
**DESCRIPTION**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;**co_read()** attempts to read up to **count** bytes from file descriptor **sockfd** into the buffer starting at **buf**. If the **timeout** is 0, **co_read** returns immediately when reading not available. If **timeout** is less than 0, **co_read** will be yielded automatically when reading not available and resumed when reading available. If **timeout** is greater than 0, it will return 0 when writing not available after **timeout** seconds.<br/><br/>
**RETURN VALUE**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;On success, the number of bytes read is returned.  On error, -1 is returned, and errno is set to indicate the cause of the error. On timeout, 0 is returned.
- ### int co_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
**DESCRIPTION**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;**co_connect()** connects the socket referred to by the file descriptor **sockfd** to the address specified by **addr**. The **addrlen** argument specifies the size of addr. The **co_connect()** will be yielded automatically when connecting can't be finished immediately and resumed when connecting finished or error occurs.<br/><br/>
**RETURN VALUE**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;If the connection or binding succeeds, zero is returned.  On error, -1 is returned, and errno is set appropriately.
- ### int co_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
**DESCRIPTION**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;The argument **sockfd** is a socket that has been created with socket, bound to a local address with bind, and is listening for connections after a listen. The current coroutine will be yielded automatically when no connections to be accepted and resumed when having connections to be accetped.<br/><br/>
**RETURN VALUE**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;On success, it returns a file descriptor for the accepted socket (a nonnegative integer). On error, -1 is returned, errno is  set  appropriately, and addrlen is left unchanged.
- ### int co_accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);
**DESCRIPTION**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;The argument **sockfd** is a socket that has been created with socket, bound to a local address with bind, and is listening for connections after a listen. The current coroutine will be yielded automatically when no connections to be accepted and resumed when having connections to be accetped. If flags is 0, then **co_accept4()** is the same as **co_accept()**. The following values can be bitwise ORed in flags to obtain different behavior:
SOCK_NONBLOCK, SOCK_CLOEXEC<br/><br/>
**RETURN VALUE**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;On success, it returns a file descriptor for the accepted socket (a nonnegative integer). On error, -1 is returned, errno is  set  appropriately, and addrlen is left unchanged.<br/><br/>
**EXAMPLES**
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
        n = co_read(fd, buf, sizeof(buf));
        if(n > 0){
            co_write(fd, buf, n);
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
- ### void co_sleep(double seconds);
**DESCRIPTION**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;The current coroutine will be yielded and resumed after seconds specified in **seconds** have elapsed.<br/><br/>
**EXAMPLES**
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
- ### void co_add_signal(int signo, void(*handler)(int signo, void *arg), void *arg);
**DESCRIPTION**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;One coroutine will be created automatically and the **handler** will be invoked with **signo**, **arg** arguments in the created coroutine when signal **signo** occurs.
- ### void co_remove_signal(int signo);
**DESCRIPTION**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Remove the handler of signal **signo**.
- ### int64_t channel_open(char *name, int msgsize, int maxmsg);
**DESCRIPTION**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Open an existed channel or create one channel if the channel identified by **name** doesn't exist. The **name** identify the channel. The **msgsize** specifies the max length of message to be send. The **maxmsg** specifies the max number of messages that can be populated in the channel simultaneously.<br/><br/>
**RETURN VALUE**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;On success, it returns an integer which can be used in the channel_send, channel_receive, channel_close. On error, -1 is returned, errno is  set  appropriately. The integer is valid only in the current coroutine, and channel_close will be invoked automatically when the current coroutine exits.
- ### int channel_send(int64_t channel_id, const char *msg_ptr, size_t msg_len, double timeout);
**DESCRIPTION**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Send a message to the channel referred by channel_id which is returned by channel_open. The message starts at **msg_ptr**, and its length is **msg_len**. The **msg_len** must be greater than 0 and less than or equal to the **msgsize** which specifies the max length  of message to be send when the channel is created. The **timeout** specifies the max seconds to wait when the channel is full.<br/><br/>
**RETURN VALUE**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;On success, the number of bytes send is returned.  On error, -1 is returned, and errno is set to indicate the cause of the error. On timeout, 0 is returned.
- ### int channel_receive(int64_t channel_id, char *msg_ptr, size_t msg_len, double timeout);
**DESCRIPTION**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Receive a message from the channel referred by channel_id which is returned by channel_open. The message will be placed in the buffer which starts at **msg_ptr**. The **msg_len** must be greater than or equal to the **msgsize** which specifies the max length  of message to be send when the channel is created. The **timeout** specifies the max seconds to wait when the channel is empty.<br/><br/>
**RETURN VALUE**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;On success, the number of bytes received is returned.  On error, -1 is returned, and errno is set to indicate the cause of the error. On timeout, 0 is returned.
- ### void channel_unlink(char *name);
**DESCRIPTION**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;**channel_unlink()** deletes a channel identified by **name**. If no coroutine opens it, the channel is deleted. If any coroutine still have the channel open, the channel will remain in existence until all coroutines close the channel.
- ### void channel_close(int64_t channel_id);
**DESCRIPTION**  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Close the channel referred by channel_id in the current coroutine. The channel_close will be invoked automatically when the current coroutine exits.
