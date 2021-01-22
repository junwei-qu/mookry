#include <mookry/coroutine.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>

void sig_pipe(int signo, void *arg){
    printf("sig_pipe\n");
}

void co_start(void *arg){
    struct sockaddr_in servaddr;
    socklen_t socklen = sizeof(servaddr);
    int sockfd;
    char buf[65535];
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(1234);
    bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    co_add_signal(SIGPIPE, sig_pipe, NULL);
    while(1){
        long ret = co_recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&servaddr, &socklen, -1);
	if(ret > 0){
	    co_sendto(sockfd, buf, ret, 0, (struct sockaddr *)&servaddr, socklen, -1);
	} else {
            perror("co_sendto error");
	}
    }
}

int
main(int argc, char **argv){
    co_env(co_start, NULL);
    return 0;
}
