

// Client side C/C++ program to demonstrate Socket
// programming
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef NORMAL
#include <sys/socket.h>
#else
#include "socket.h"
#endif

#define PORT 22122



#ifndef NORMAL

void __attribute__ ((constructor)) setupIface()
{
	// der server laueft auf IP=1, der Client auf IP=0
	if (!registerInterface("iface.0", 0x10000, 1, 0))
		exit(1);
		
    	printf("Iface setup finished\n");
}

#endif

char hello1[] = "Hello1 from client";
char hello2[] = "Hello2 from client";
char hello3[] = "Hello3 from client";
char hello4[] = "Hello4 from client";


int main(int argc, char const* argv[])
{
	int status, valread, client_fd;
	struct sockaddr_in serv_addr;
	
	char buffer[1024] = { 0 };
	if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("\n Socket creation error \n");
		return -1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	// Convert IPv4 and IPv6 addresses from text to binary
	// form
	if (inet_pton(AF_INET, "1.0.0.0", &serv_addr.sin_addr)
		<= 0) {
		printf(
			"\nInvalid address/ Address not supported \n");
		return -1;
	}

	if ((status
		= connect(client_fd, (struct sockaddr*)&serv_addr,
				sizeof(serv_addr)))
		< 0) {
		printf("\nConnection Failed \n");
		return -1;
	}
	send(client_fd, hello1, strlen(hello1), 0);
	send(client_fd, hello2, strlen(hello2), 0);
	send(client_fd, hello3, strlen(hello3), 0);
	send(client_fd, hello4, strlen(hello4), 0);
	
	printf("Hello message sent\n");
	printf("Press Any Key to Continue\n");  
	getchar();   
	
	printf("%s\n", buffer);

	// closing the connected socket
	close(client_fd);
	return 0;
}



