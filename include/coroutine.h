#ifndef  _COROUTINE_H
#define  _COROUTINE_H

#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>

#define DEFAULT_COROUTINE_STACK_SIZE 2 * 1024 * 1024

int enter_coroutine_environment(void (*co_start)(void *), void *arg);
void make_coroutine(uint32_t stack_size, void(*routine)(void *), void *arg);
ssize_t co_write(int fd, const void *buf, size_t count);
ssize_t co_read(int fd, void *buf, size_t count);
int co_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int co_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int co_accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);
void co_sleep(double seconds);
void co_add_signal(int signo, void(*handler)(int signo, void *arg), void *arg);
void co_remove_signal(int signo);
int channel_send(int64_t channel_id, const char *msg_ptr, size_t msg_len, double timeout);
int channel_receive(int64_t channel_id, char *msg_ptr, size_t msg_len, double timeout);
void channel_unlink(char *name);
void channel_close(int64_t channel_id);
int64_t channel_open(char *name, int msgsize, int maxmsg);

#endif
