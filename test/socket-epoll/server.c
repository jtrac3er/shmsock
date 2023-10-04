#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <resolv.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <unistd.h>

#ifdef NORMAL
#include <sys/socket.h>
#else
#include "socket.h"
#endif

#define PORT 22122
#define SERVER "127.0.0.1"
#define MAXBUF 1024
#define MAX_EPOLL_EVENTS 64


#ifndef NORMAL

void __attribute__ ((constructor)) setupIface()
{
	// der server laueft auf IP=1, der Client auf IP=0
	if (!registerInterface("iface.0", 0x10000, 0, 1))
		exit(1);
		
    	printf("Iface setup finished\n");
}

#endif


int main() {
    int sockfd;
    struct sockaddr_in dest;
    char buffer[MAXBUF];
    struct epoll_event events[MAX_EPOLL_EVENTS];
    int i, num_ready;

    /*---Open socket for streaming---*/
    if ( (sockfd = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0)) < 0 ) {
        perror("Socket");
        exit(errno);
    }

    /*---Add socket to epoll---*/
    int epfd = epoll_create(1);
    struct epoll_event event;
    event.events = EPOLLIN; // Can append "|EPOLLOUT" for write events as well
    event.data.fd = sockfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &event);

    /*---Initialize server address/port struct---*/
    bzero(&dest, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(PORT);
    if ( inet_pton(AF_INET, SERVER, &dest.sin_addr.s_addr) == 0 ) {
        perror(SERVER);
        exit(errno);
    }

    /*---Connect to server---*/
    if ( connect(sockfd, (struct sockaddr*)&dest, sizeof(dest)) != 0 ) {
        if(errno != EINPROGRESS) {
            perror("Connect ");
            exit(errno);
        }
    }

    /*---Wait for socket connect to complete---*/
    num_ready = epoll_wait(epfd, events, MAX_EPOLL_EVENTS, 1000/*timeout*/);
    for(i = 0; i < num_ready; i++) {
        if(events[i].events & EPOLLIN) {
            printf("Socket %d connected\n", events[i].data.fd);
        }
    }

    /*---Wait for data---*/
    int numNoRecv = 0;
   
    while (1)
    {
     	num_ready = epoll_wait(epfd, events, MAX_EPOLL_EVENTS, 1000/*timeout*/);
     	
		for(i = 0; i < num_ready; i++) {
		    if(events[i].events & EPOLLIN) {
		        printf("Socket %d got some data\n", events[i].data.fd);
		        bzero(buffer, MAXBUF);
		        recv(sockfd, buffer, sizeof(buffer), 0);
		        printf("Received: %s", buffer);
		    }
		}
		
		if (num_ready == 0)
			numNoRecv++;
			
		if (numNoRecv == 5)
			break;
    }

    close(sockfd);
    return 0;
}

