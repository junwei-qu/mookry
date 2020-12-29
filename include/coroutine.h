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
int co_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int co_accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);
void co_sleep(float seconds);

#endif
