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
